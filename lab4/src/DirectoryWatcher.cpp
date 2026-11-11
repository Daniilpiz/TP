// DirectoryWatcher.cpp
#include "DirectoryWatcher.h"
#include <iostream>
namespace fs = std::filesystem;

DirectoryWatcher::DirectoryWatcher(ThreadSafeQueue<std::string>& queue)
    : queue_(queue) {}

DirectoryWatcher::~DirectoryWatcher() { stop(); }

void DirectoryWatcher::start(const std::string& dir) {
    {
        std::lock_guard<std::mutex> lk(dirMtx_);
        currentDir_ = dir;
        dirChanged_ = true;
    }
    running_ = true;
    thread_ = std::thread(&DirectoryWatcher::run, this);
}

void DirectoryWatcher::changeDirectory(const std::string& dir) {
    std::lock_guard<std::mutex> lk(dirMtx_);
    currentDir_ = dir;
    dirChanged_ = true;
}

void DirectoryWatcher::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void DirectoryWatcher::run() {
    while (running_) {
        scanOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void DirectoryWatcher::scanOnce() {
    std::string dir;
    {
        std::lock_guard<std::mutex> lk(dirMtx_);
        dir = currentDir_;
        if (dirChanged_) { known_.clear(); dirChanged_ = false; }
    }
    if (dir.empty()) return;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;

        const auto path  = entry.path().string();
        const auto mtime = fs::last_write_time(entry, ec);
        if (ec) continue;

        auto it = known_.find(path);
        if (it == known_.end() || it->second != mtime) {
            known_[path] = mtime;
            queue_.push(path);
        }
    }
}