// AccessPolicy.cpp
#include "AccessPolicy.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// Нормализация пути: чтобы «file1.md», «./file1.md» и «test_dir/../file1.md»
// считались одним и тем же файлом (и блокировка, и счётчик попыток).
static std::string normPath(const std::string& p) {
    std::error_code ec;
    auto canonical = fs::weakly_canonical(fs::path(p), ec);
    if (!ec && !canonical.empty()) return canonical.string();
    return fs::path(p).lexically_normal().string();
}

AccessPolicy& AccessPolicy::instance() {
    static AccessPolicy inst;
    return inst;
}

void AccessPolicy::setMaxAttempts(std::size_t n) {
    std::lock_guard<std::mutex> lk(mtx_);
    maxAttempts_ = n == 0 ? 1 : n;
}

std::size_t AccessPolicy::maxAttempts() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return maxAttempts_;
}

bool AccessPolicy::tryAcquire(const std::string& path,
                              std::string& reason,
                              std::size_t& attempt,
                              std::size_t& currentLimit) {
    const std::string key = normPath(path);
    std::lock_guard<std::mutex> lk(mtx_);

    std::size_t& cnt = attempts_[key];
    ++cnt;                       // любая попытка потока засчитывается
    attempt       = cnt;
    currentLimit  = maxAttempts_;
    const bool isBlocked = blocked_.count(key) != 0;

    if (isBlocked) {
        reason = "BLOCKED by policy";
        std::cout << "[policy] " << path << ": attempt " << attempt
                  << "/" << maxAttempts_ << " -> BLOCKED, processing not allowed\n";
        return false;
    }
    if (cnt > maxAttempts_) {
        reason = "attempt limit exceeded";
        std::cout << "[policy] " << path << ": attempt " << attempt
                  << "/" << maxAttempts_ << " -> LIMIT EXCEEDED, further processing denied\n";
        return false;
    }
    reason.clear();
    std::cout << "[policy] " << path << ": attempt " << attempt
              << "/" << maxAttempts_ << " -> allowed\n";
    return true;
}

void AccessPolicy::block(const std::string& path) {
    std::lock_guard<std::mutex> lk(mtx_);
    blocked_.insert(normPath(path));
}

void AccessPolicy::unblock(const std::string& path) {
    std::lock_guard<std::mutex> lk(mtx_);
    blocked_.erase(normPath(path));
}

bool AccessPolicy::isBlocked(const std::string& path) const {
    std::lock_guard<std::mutex> lk(mtx_);
    return blocked_.count(normPath(path)) != 0;
}

std::vector<std::string> AccessPolicy::blockedFiles() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return { blocked_.begin(), blocked_.end() };
}

std::size_t AccessPolicy::unblockAll() {
    std::lock_guard<std::mutex> lk(mtx_);
    std::size_t n = blocked_.size();
    blocked_.clear();
    return n;
}

AccessPolicy::Info AccessPolicy::info(const std::string& path) const {
    const std::string key = normPath(path);
    std::lock_guard<std::mutex> lk(mtx_);
    Info i;
    i.limit  = maxAttempts_;
    i.blocked = blocked_.count(key) != 0;
    auto it = attempts_.find(key);
    if (it != attempts_.end()) {
        i.attempts  = it->second;
        i.exhausted = it->second >= maxAttempts_;
    }
    return i;
}

std::map<std::string, AccessPolicy::Info> AccessPolicy::snapshot() const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::map<std::string, Info> out;
    for (const auto& [p, cnt] : attempts_) {
        Info i;
        i.attempts  = cnt;
        i.limit     = maxAttempts_;
        i.blocked   = blocked_.count(p) != 0;
        i.exhausted = cnt >= maxAttempts_;
        out[p] = i;
    }
    for (const auto& b : blocked_) {
        if (!out.count(b)) {
            Info i;
            i.limit   = maxAttempts_;
            i.blocked = true;
            out[b] = i;
        }
    }
    return out;
}

void AccessPolicy::resetAttempts() {
    std::lock_guard<std::mutex> lk(mtx_);
    attempts_.clear();
}
