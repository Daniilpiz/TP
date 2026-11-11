#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cstddef>

template <typename T>
class ThreadSafeQueue {
public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            q_.push(std::move(value));
        }
        cv_.notify_one();
    }

    // Блокирующее извлечение; возвращает false, если очередь остановлена
    bool pop(T& out) {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait(lk, [&]{ return !q_.empty() || stopped_; });
        if (q_.empty()) return false;
        out = std::move(q_.front());
        q_.pop();
        return true;
    }

    std::vector<T> snapshot() const {
        std::lock_guard<std::mutex> lk(mtx_);
        std::vector<T> v;
        std::queue<T> copy = q_;
        while (!copy.empty()) { v.push_back(std::move(copy.front())); copy.pop(); }
        return v;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return q_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lk(mtx_);
        std::queue<T> empty;
        q_.swap(empty);
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            stopped_ = true;
        }
        cv_.notify_all();
    }

private:
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<T> q_;
    bool stopped_ = false;
};