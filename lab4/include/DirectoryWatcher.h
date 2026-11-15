// DirectoryWatcher.h
#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include "ThreadSafeQueue.h"

class DirectoryWatcher {
public:
    explicit DirectoryWatcher(ThreadSafeQueue<std::string>& queue);
    ~DirectoryWatcher();

    void start(const std::string& dir);
    void changeDirectory(const std::string& dir);
    void stop();

private:
    void run();
    void scanOnce();

    ThreadSafeQueue<std::string>& queue_;
    std::thread thread_;
    std::atomic<bool> running_{false};

    mutable std::mutex dirMtx_;
    std::condition_variable wakeCv_;      // мгновенный выход из паузы при stop()
    std::string currentDir_;
    bool dirChanged_ = false;

    // known_ используется только потоком watcher-а, мьютекс не нужен
    std::map<std::string, std::filesystem::file_time_type> known_;
};