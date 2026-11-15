#pragma once
#include "file_processor.h"
#include "thread_safe_queue.h"
#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <thread>

enum class State { Queued, Running, Done, Cancelled, Error };
struct Job {
    std::uint64_t id = 0, size = 0;
    std::filesystem::path input, output;
    std::atomic<bool> cancel{false};
    std::atomic<std::uint64_t> bytes{0};
    // The following fields are protected by Application::mutex_.
    State state = State::Queued;
    ProcessingStats stats;
    std::string error;
};
class Application {
    using Task = std::shared_ptr<Job>;
    ThreadSafeQueue<Task> queue_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::map<std::uint64_t, Task> jobs_;
    std::filesystem::path directory_, results_;
    std::uint64_t generation_ = 0, nextId_ = 1;
    std::atomic<bool> stopping_{false};
    unsigned concurrency_;
    bool asynchronous_;
    std::string watcherError_;
    std::thread watcher_, scheduler_;
    std::vector<std::thread> workers_;
    void watch();
    void run(const Task& job);
    void schedule();
public:
    Application(const std::filesystem::path& directory, bool asynchronous);
    ~Application();
    void changeDirectory(const std::filesystem::path& directory);
    std::filesystem::path directory() const;
    void printQueue() const;
    void printStatus() const;
    void printResult(std::uint64_t id) const;
    void cancel(std::uint64_t id);
    void resetQueue();
    void resetResults();
    unsigned concurrency() const { return concurrency_; }
};
