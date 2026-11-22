#include "ProcessingEngine.h"
#include "FileProcessor.h"
#include <algorithm>
#include <iostream>

ProcessingEngine::ProcessingEngine(ThreadSafeQueue<std::string>& queue,
                                   const lab4::Lab2Adapter& lab2)
    : queue_(queue), lab2_(lab2) {}

ProcessingEngine::~ProcessingEngine() { stop(); }

void ProcessingEngine::start() {
    if (running_) return;
    queue_.resume();   // очередь могла быть остановлена предыдущим stop()
    running_ = true;
    unsigned n = std::max(1u, std::thread::hardware_concurrency());
    for (unsigned i = 0; i < n; ++i)
        workers_.emplace_back(&ProcessingEngine::worker, this);
}

void ProcessingEngine::stop() {
    if (!running_.exchange(false)) return;
    paused_ = false;
    pauseCv_.notify_all();     // рабочие должны выйти из паузы и завершиться
    queue_.stop();
    for (auto& t : workers_) if (t.joinable()) t.join();
    workers_.clear();
}

void ProcessingEngine::pauseWorkers() {
    bool expected = false;
    if (paused_.compare_exchange_strong(expected, true)) {
        std::cout << "[engine] workers paused\n";
    }
}

void ProcessingEngine::resumeWorkers() {
    if (paused_.exchange(false)) {
        std::lock_guard<std::mutex> lk(pauseMtx_);
        pauseCv_.notify_all();
        std::cout << "[engine] workers resumed\n";
    }
}

void ProcessingEngine::worker() {
    std::string path;
    while (running_ && queue_.pop(path)) {
        {
            std::lock_guard<std::mutex> lk(seenMtx_);
            seenFiles_.insert(path);
        }

        // Пауза обработки (демонстрация блокировок): ждём снятия paused_.
        {
            std::unique_lock<std::mutex> lk(pauseMtx_);
            pauseCv_.wait(lk, [&]{ return !paused_.load() || !running_.load(); });
        }
        if (!running_) return;

        std::string r = processFile(path, lab2_);
        {
            std::lock_guard<std::mutex> lk(resultsMtx_);
            results_[path] = r;
        }
    }
}

bool ProcessingEngine::getResult(const std::string& file, std::string& out) const {
    std::lock_guard<std::mutex> lk(resultsMtx_);
    auto it = results_.find(file);
    if (it == results_.end()) return false;
    out = it->second;
    return true;
}

std::map<std::string, std::string> ProcessingEngine::getAllResults() const {
    std::lock_guard<std::mutex> lk(resultsMtx_);
    return results_;
}

void ProcessingEngine::resetResults() {
    std::lock_guard<std::mutex> lk(resultsMtx_);
    results_.clear();
}

std::vector<std::string> ProcessingEngine::getAllSeenFiles() const {
    std::lock_guard<std::mutex> lk(seenMtx_);
    return { seenFiles_.begin(), seenFiles_.end() };
}