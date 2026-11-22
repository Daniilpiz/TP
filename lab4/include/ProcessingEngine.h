#pragma once
#include <atomic>
#include <condition_variable>
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

    // Запуск/остановка рабочих потоков без остановки очереди:
    // позволяет «заморозить» обработку на время демонстрации блокировок.
    void pauseWorkers();
    void resumeWorkers();

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
    std::atomic<bool> paused_{false};     // рабочие ждут снятия паузы

    mutable std::mutex pauseMtx_;
    std::condition_variable pauseCv_;     // мгновенный выход из паузы

    mutable std::mutex resultsMtx_;
    std::map<std::string, std::string> results_;

    mutable std::mutex seenMtx_;
    std::set<std::string> seenFiles_;
};