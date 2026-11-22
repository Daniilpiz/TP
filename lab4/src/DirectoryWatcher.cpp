// DirectoryWatcher.cpp
#include "DirectoryWatcher.h"
#include "AccessPolicy.h"
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
    wakeCv_.notify_all();          // не ждём остаток паузы
    if (thread_.joinable()) thread_.join();
}

void DirectoryWatcher::run() {
    while (running_) {
        scanOnce();
        // Пауза 500 мс, прерываемая по stop() через condition_variable.
        std::unique_lock<std::mutex> lk(dirMtx_);
        wakeCv_.wait_for(lk, std::chrono::milliseconds(500),
                         [&]{ return !running_.load(); });
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

    // directory_iterator может бросить исключение (папку удалили/закрыли) -
    // поток watcher-а не должен падать.
    try {
        std::error_code ec;
        for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
             !ec && it != end; it.increment(ec)) {
            if (ec) break;
            const auto& entry = *it;
            if (!entry.is_regular_file(ec)) continue;

            const auto path  = entry.path().string();
            const auto mtime = fs::last_write_time(entry, ec);
            if (ec) continue;

            // Файл заблокирован политикой доступа — в очередь не попадает.
            if (AccessPolicy::instance().isBlocked(path)) continue;

            auto kit = known_.find(path);
            if (kit == known_.end() || kit->second != mtime) {
                known_[path] = mtime;
                queue_.push(path);
            }
        }
    } catch (const fs::filesystem_error&) {
        // каталог временно недоступен - следующая итерация попробует снова
    }
}