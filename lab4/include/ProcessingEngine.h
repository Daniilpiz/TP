#pragma once
#include <atomic>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include "Lab2Adapter.h"
#include "ThreadSafeQueue.h"

class ProcessingEngine {
public:
    explicit ProcessingEngine(ThreadSafeQueue<std::string>& queue,
                              const lab4::Lab2Adapter& lab2);
    ~ProcessingEngine();

    void start();
    void stop();

    bool getResult(const std::string& file, std::string& out) const;
    std::map<std::string, std::string> getAllResults() const;
    void resetResults();

    std::vector<std::string> getAllSeenFiles() const;

private:
    void worker();

    ThreadSafeQueue<std::string>& queue_;
    const lab4::Lab2Adapter& lab2_;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{false};

    mutable std::mutex resultsMtx_;
    std::map<std::string, std::string> results_;

    mutable std::mutex seenMtx_;
    std::set<std::string> seenFiles_;
};