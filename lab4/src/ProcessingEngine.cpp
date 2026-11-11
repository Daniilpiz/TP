#include "ProcessingEngine.h"
#include "FileProcessor.h"
#include <algorithm>

ProcessingEngine::ProcessingEngine(ThreadSafeQueue<std::string>& queue)
    : queue_(queue) {}

ProcessingEngine::~ProcessingEngine() { stop(); }

void ProcessingEngine::start() {
    if (running_) return;
    running_ = true;
    unsigned n = std::max(1u, std::thread::hardware_concurrency());
    for (unsigned i = 0; i < n; ++i)
        workers_.emplace_back(&ProcessingEngine::worker, this);
}

void ProcessingEngine::stop() {
    if (!running_.exchange(false)) return;
    queue_.stop();
    for (auto& t : workers_) if (t.joinable()) t.join();
    workers_.clear();
}

void ProcessingEngine::worker() {
    std::string path;
    while (running_ && queue_.pop(path)) {
        {
            std::lock_guard<std::mutex> lk(seenMtx_);
            seenFiles_.insert(path);
        }
        std::string r = processFile(path);
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