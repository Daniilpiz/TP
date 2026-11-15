#pragma once
#include <condition_variable>
#include <deque>
#include <mutex>
#include <vector>

// One mutex protects all queue access; waiting threads do not busy-wait.
template<class T> class ThreadSafeQueue {
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<T> items_;
    bool closed_ = false;
public:
    bool push(T item) {
        { std::lock_guard<std::mutex> lock(mutex_);
          if (closed_) return false;
          items_.push_back(std::move(item)); }
        ready_.notify_one();
        return true;
    }
    bool waitPop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        ready_.wait(lock, [&] { return closed_ || !items_.empty(); });
        if (items_.empty()) return false;
        item = std::move(items_.front()); items_.pop_front();
        return true;
    }
    bool tryPop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (items_.empty()) return false;
        item = std::move(items_.front()); items_.pop_front();
        return true;
    }
    std::vector<T> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {items_.begin(), items_.end()};
    }
    std::vector<T> clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<T> removed(items_.begin(), items_.end());
        items_.clear(); return removed;
    }
    void close() {
        { std::lock_guard<std::mutex> lock(mutex_); closed_ = true; }
        ready_.notify_all();
    }
};
