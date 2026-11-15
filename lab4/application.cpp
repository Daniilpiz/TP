#include "application.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;
using namespace std::chrono_literals;
namespace {
const char* stateName(State state) {
    switch (state) {
    case State::Queued: return "В очереди";
    case State::Running: return "Обрабатывается";
    case State::Done: return "Завершено";
    case State::Cancelled: return "Отменено";
    case State::Error: return "Ошибка";
    }
    return "?";
}
bool terminal(State state) {
    return state == State::Done || state == State::Cancelled || state == State::Error;
}
bool inside(const fs::path& path, const fs::path& parent) {
    auto a = path.begin(), b = parent.begin();
    for (; b != parent.end(); ++a, ++b) {
        if (a == path.end()) return false;
#ifdef _WIN32
        if (CompareStringOrdinal(a->c_str(), -1, b->c_str(), -1, TRUE) != CSTR_EQUAL) return false;
#else
        if (*a != *b) return false;
#endif
    }
    return true;
}
}

Application::Application(const fs::path& directory, bool asynchronous)
    : concurrency_(std::max(1u, std::thread::hardware_concurrency())),
      asynchronous_(asynchronous) {
    // Use a separate session directory so results never overwrite source files.
    const auto root = fs::absolute("l4_results");
    fs::create_directories(root);
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned n = 0;; ++n) {
        results_ = root / (std::to_string(stamp) + "_" + std::to_string(n));
        if (fs::create_directory(results_)) break;
    }
    results_ = fs::canonical(results_);
    changeDirectory(directory);
    try {
        if (asynchronous_) scheduler_ = std::thread(&Application::schedule, this);
        else for (unsigned i = 0; i < concurrency_; ++i) {
            workers_.emplace_back([this] {
                Task job;
                while (queue_.waitPop(job)) run(job);
            });
        }
        watcher_ = std::thread(&Application::watch, this);
    } catch (...) {
        stopping_ = true; queue_.close(); wake_.notify_all();
        if (scheduler_.joinable()) scheduler_.join();
        for (auto& worker : workers_) worker.join();
        throw;
    }
}

Application::~Application() {
    stopping_ = true;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& entry : jobs_) entry.second->cancel = true;
    }
    queue_.close(); wake_.notify_all();
    if (watcher_.joinable()) watcher_.join();
    if (scheduler_.joinable()) scheduler_.join();
    for (auto& worker : workers_) worker.join();
}

std::filesystem::path Application::directory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return directory_;
}

void Application::changeDirectory(const fs::path& directory) {
    const auto resolved = fs::canonical(directory);
    if (!fs::is_directory(resolved)) throw std::runtime_error("Указанный путь не является каталогом");
    if (inside(resolved, results_.parent_path()))
        throw std::runtime_error("Каталог результатов нельзя использовать как входной");
    std::lock_guard<std::mutex> lock(mutex_);
    directory_ = resolved; ++generation_; watcherError_.clear(); wake_.notify_all();
}

void Application::watch() {
    struct Seen {
        std::uintmax_t size;
        fs::file_time_type modified;
        std::chrono::steady_clock::time_point stableSince;
        bool submitted = false;
    };
    std::map<fs::path, Seen> seen;
    std::uint64_t generation = 0;
#ifdef _WIN32
    HANDLE notification = INVALID_HANDLE_VALUE;
#endif
    while (!stopping_) {
        fs::path directory;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            directory = directory_;
            if (generation != generation_) {
                generation = generation_; seen.clear();
#ifdef _WIN32
                if (notification != INVALID_HANDLE_VALUE) FindCloseChangeNotification(notification);
                notification = FindFirstChangeNotificationW(directory.c_str(), FALSE,
                    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE);
#endif
            }
        }
        try {
            const auto now = std::chrono::steady_clock::now();
            std::map<fs::path, Seen> current;
            for (const auto& entry : fs::directory_iterator(directory)) {
                if (stopping_) break;
                std::error_code ec;
                // Skip symbolic links to avoid reading files outside the selected directory.
                if (entry.is_symlink(ec) || ec || !entry.is_regular_file(ec) || ec) continue;
                const auto size = entry.file_size(ec);
                if (ec) continue;
                const auto modified = entry.last_write_time(ec);
                if (ec) continue;
                const auto path = entry.path();
                auto previous = seen.find(path);
                Seen item{size, modified, now, false};
                if (previous != seen.end() && previous->second.size == size &&
                    previous->second.modified == modified) item = previous->second;
                // Wait for the file to stabilize before adding it to the queue.
                if (!item.submitted && now - item.stableSince >= 300ms) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (generation == generation_ && !stopping_) {
                        auto job = std::make_shared<Job>();
                        job->id = nextId_++; job->input = path; job->size = size;
                        job->output = results_ / (std::to_string(job->id) + "_" + path.filename().u8string() + ".clean.md");
                        jobs_[job->id] = job; queue_.push(job);
                        item.submitted = true; wake_.notify_all();
                    }
                }
                current.emplace(path, item);
            }
            seen.swap(current);
            std::lock_guard<std::mutex> lock(mutex_);
            if (generation == generation_) watcherError_.clear();
        } catch (const std::exception& error) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (generation == generation_) watcherError_ = error.what();
        }
#ifdef _WIN32
        // System notifications wake the watcher immediately; the timeout allows
        // file stability checks, directory changes, and application shutdown.
        if (notification != INVALID_HANDLE_VALUE) {
            const auto result = WaitForSingleObject(notification, 100);
            if (result == WAIT_OBJECT_0 && !FindNextChangeNotification(notification)) {
                FindCloseChangeNotification(notification); notification = INVALID_HANDLE_VALUE;
            } else if (result == WAIT_FAILED) {
                FindCloseChangeNotification(notification); notification = INVALID_HANDLE_VALUE;
            }
        } else
#endif
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait_for(lock, 100ms, [&] { return stopping_ || generation != generation_; });
        }
    }
#ifdef _WIN32
    if (notification != INVALID_HANDLE_VALUE) FindCloseChangeNotification(notification);
#endif
}

void Application::run(const Task& job) {
    fs::path partial = job->output; partial += ".part";
    try {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (job->cancel || stopping_) { job->state = State::Cancelled; return; }
            job->state = State::Running;
        }
        const auto size = fs::file_size(job->input);
        const auto modified = fs::last_write_time(job->input);
        auto stats = processFile(job->input, partial, job->cancel, job->bytes);
        if (size != fs::file_size(job->input) || modified != fs::last_write_time(job->input))
            throw std::runtime_error("Файл изменился во время обработки; ожидайте новую операцию");
        std::lock_guard<std::mutex> lock(mutex_);
        if (job->cancel || stopping_) throw OperationCancelled();
        fs::rename(partial, job->output);
        job->stats = stats; job->state = State::Done;
    } catch (const OperationCancelled&) {
        std::lock_guard<std::mutex> lock(mutex_); job->state = State::Cancelled;
    } catch (const std::exception& error) {
        std::lock_guard<std::mutex> lock(mutex_);
        job->state = job->cancel ? State::Cancelled : State::Error; job->error = error.what();
    }
    std::error_code ignored; fs::remove(partial, ignored);
    wake_.notify_all();
}

void Application::schedule() {
    // One dispatcher stores futures and limits concurrent async operations
    // to the hardware concurrency. launch::async prevents deferred execution.
    std::vector<std::future<void>> operations;
    while (!stopping_) {
        for (auto it = operations.begin(); it != operations.end();) {
            if (it->wait_for(0ms) == std::future_status::ready) {
                it->get(); it = operations.erase(it);
            } else ++it;
        }
        Task job;
        while (!stopping_ && operations.size() < concurrency_ && queue_.tryPop(job)) {
            try {
                operations.push_back(std::async(std::launch::async, [this, job] { run(job); }));
            } catch (const std::exception& error) {
                std::lock_guard<std::mutex> lock(mutex_);
                job->state = State::Error; job->error = error.what();
            }
        }
        std::unique_lock<std::mutex> lock(mutex_);
        wake_.wait_for(lock, 25ms);
    }
    for (auto& operation : operations) operation.get();
}

void Application::printQueue() const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto snapshot = queue_.snapshot();
    std::cout << '\n' << "Каталог: " << directory_.u8string() << "\nВ очереди: " << snapshot.size() << '\n';
    for (const auto& job : snapshot) std::cout << job->id << " | " << job->input.u8string() << '\n';
    if (!watcherError_.empty()) std::cout << "Ошибка наблюдения: " << watcherError_ << '\n';
}

void Application::printStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << '\n';
    for (const auto& entry : jobs_) {
        const auto& job = entry.second;
        const auto bytes = job->bytes.load();
        const auto percent = job->state == State::Done ? 100.0 :
            (job->size ? std::min(100.0, 100.0 * bytes / job->size) : 0.0);
        std::cout << job->id << " | " << stateName(job->state) << " | " << percent << "% | "
                  << job->input.u8string();
        if (job->cancel && job->state == State::Running) std::cout << " | Запрошена отмена";
        if (!job->error.empty()) std::cout << " | " << job->error;
        std::cout << '\n';
    }
    if (jobs_.empty()) std::cout << "Операций пока нет.\n";
}

void Application::printResult(std::uint64_t id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) { std::cout << "ID не найден.\n"; return; }
    const auto& job = it->second;
    std::cout << '\n' << stateName(job->state) << '\n';
    if (job->state != State::Done) {
        if (!job->error.empty()) std::cout << job->error << '\n';
        return;
    }
    std::cout << "Результат: " << job->output.u8string() << "\nКорректных анкет: " << job->stats.valid
              << ", отклонено: " << job->stats.invalid << "\nПервые 100 строк:\n";
    std::ifstream in(job->output, std::ios::binary);
    if (!in) { std::cout << "Не удалось открыть результат.\n"; return; }
    std::string line;
    for (int i = 0; i < 100 && std::getline(in, line); ++i) std::cout << line << '\n';
}

void Application::cancel(std::uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) { std::cout << "ID не найден.\n"; return; }
    auto& job = it->second;
    if (terminal(job->state)) { std::cout << "Операция уже завершена.\n"; return; }
    job->cancel = true;
    if (job->state == State::Queued) job->state = State::Cancelled;
    std::cout << "Запрошена отмена операции " << id << ".\n";
}

void Application::resetQueue() {
    std::lock_guard<std::mutex> lock(mutex_);
    // Also cancel jobs already dequeued by the dispatcher but not yet started.
    queue_.clear();
    for (auto& entry : jobs_) if (entry.second->state == State::Queued) {
        entry.second->cancel = true; entry.second->state = State::Cancelled;
    }
    std::cout << "Очередь очищена. Выполняющиеся операции продолжаются.\n";
}

void Application::resetResults() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t removed = 0;
    for (auto it = jobs_.begin(); it != jobs_.end();) {
        if (!terminal(it->second->state)) { ++it; continue; }
        std::error_code error;
        if (it->second->state == State::Done) fs::remove(it->second->output, error);
        if (error) {
            std::cout << "Не удалось удалить результат ID " << it->first << ": " << error.message() << '\n';
            ++it;
        } else { it = jobs_.erase(it); ++removed; }
    }
    std::cout << "Удалено завершенных операций: " << removed << ". Текущие операции продолжаются.\n";
}
