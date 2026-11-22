#include "AsyncProcessor.h"
#include "AccessPolicy.h"
#include "FileProcessor.h"
#include <algorithm>
#include <iostream>

// Статус Blocked/LimitReached (см. TaskStatus) означают, что обработка файла
// запрещена политикой доступа: файл заблокирован или исчерпан лимит попыток.
AsyncProcessor::AsyncProcessor(const lab4::Lab2Adapter& lab2) : lab2_(lab2) {}

AsyncProcessor::~AsyncProcessor() {
    cancel();
    waitAll();
}

void AsyncProcessor::startAsync(const std::vector<std::string>& files) {
    // завершить предыдущую операцию (join только из потока-создателя runner_)
    cancel();
    waitAll();

    {
        std::lock_guard<std::mutex> lk(mtx_);
        status_.clear();
        results_.clear();
        for (const auto& f : files) status_[f] = TaskStatus::Pending;
        cancelled_ = false;   // сброс флага под мьютексом - вместе с очисткой карт
    }

    running_ = true;

    runner_ = std::make_shared<std::thread>([this, files]() {
        unsigned hw = std::max(1u, std::thread::hardware_concurrency());
        std::atomic<std::size_t> idx{0};

        auto work = [this, &files, &idx]() {
            for (;;) {
                std::size_t i = idx.fetch_add(1);
                if (i >= files.size()) return;
                const std::string& f = files[i];

                {
                    std::lock_guard<std::mutex> lk(mtx_);
                    if (cancelled_) { status_[f] = TaskStatus::Cancelled; continue; }
                    status_[f] = TaskStatus::Processing;
                }

                std::string r;
                try {
                    r = processFile(f, lab2_);
                    // Файл заблокирован политикой или лимит попыток исчерпан:
                    // задача не выполнена, но помечается как Failed с причиной.
                    if (r.rfind("DENIED:", 0) == 0) {
                        std::lock_guard<std::mutex> lk(mtx_);
                        results_[f] = r;
                        status_[f] = TaskStatus::Failed;
                        continue;
                    }
                }
                catch (...) {
                    std::lock_guard<std::mutex> lk(mtx_);
                    status_[f] = TaskStatus::Failed;
                    continue;
                }

                std::lock_guard<std::mutex> lk(mtx_);
                if (cancelled_) { status_[f] = TaskStatus::Cancelled; }
                else            { results_[f] = std::move(r); status_[f] = TaskStatus::Done; }
            }
        };

        std::vector<std::thread> workers;
        for (unsigned t = 0; t < hw; ++t) workers.emplace_back(work);
        for (auto& w : workers) w.join();
        running_ = false;
    });
}

void AsyncProcessor::cancel() { cancelled_ = true; }

// Актуализация статусов Blocked / LimitReached по данным AccessPolicy.
// Вызывается из getStatus()/snapshot(): сначала под mtx_ собираем список
// задач, затем (уже без mtx_, чтобы не держать два мьютекса сразу)
// переспрашиваем политику и обновляем статусы.
std::map<std::string, TaskStatus> AsyncProcessor::refreshBlockedStatuses() const {
    std::vector<std::string> files;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        files.reserve(status_.size());
        for (const auto& [f, st] : status_) files.push_back(f);
    }

    auto& policy = AccessPolicy::instance();
    std::map<std::string, TaskStatus> result;
    for (const auto& f : files) {
        TaskStatus st = TaskStatus::Pending;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = status_.find(f);
            if (it != status_.end()) st = it->second;
        }
        // Уже выполненные/отменённые задачи не переопределяем.
        if (st == TaskStatus::Done || st == TaskStatus::Cancelled ||
            st == TaskStatus::Processing) {
            result[f] = st;
            continue;
        }
        const auto i = policy.info(f);
        if (i.blocked) {
            st = TaskStatus::Blocked;
        } else if (i.exhausted) {
            st = TaskStatus::LimitReached;
        } else if (st == TaskStatus::Failed) {
            // Failed мог быть выставлен из-за отказа политики - после
            // разблокировки сбрасываем в Pending.
            st = TaskStatus::Pending;
        }
        result[f] = st;
        std::lock_guard<std::mutex> lk(mtx_);
        status_[f] = st;
    }
    return result;
}

void AsyncProcessor::waitAll() {
    std::shared_ptr<std::thread> r;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        r = std::move(runner_);   // забираем владение потоком
    }
    if (r && r->joinable()) r->join();
}

TaskStatus AsyncProcessor::getStatus(const std::string& file) const {
    auto statuses = refreshBlockedStatuses();
    auto it = statuses.find(file);
    return it == statuses.end() ? TaskStatus::Pending : it->second;
}

bool AsyncProcessor::getResult(const std::string& file, std::string& out) const {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = results_.find(file);
    if (it == results_.end()) return false;
    out = it->second;
    return true;
}

std::map<std::string, std::pair<TaskStatus, std::string>>
AsyncProcessor::snapshot() const {
    auto statuses = refreshBlockedStatuses();   // актуальные Blocked/LimitReached
    std::lock_guard<std::mutex> lk(mtx_);
    std::map<std::string, std::pair<TaskStatus, std::string>> out;
    for (const auto& [f, st] : statuses) {
        auto rit = results_.find(f);
        out[f] = { st, rit == results_.end() ? std::string{} : rit->second };
    }
    return out;
}
