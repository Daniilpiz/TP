// AsyncProcessor.h
#pragma once
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

enum class TaskStatus { Pending, Processing, Done, Cancelled, Failed };

class AsyncProcessor {
public:
    ~AsyncProcessor();

    // Запускает асинхронную обработку набора файлов (например, снимка очереди).
    void startAsync(const std::vector<std::string>& files);

    // Запросить отмену. Уже запущенные файлы дочитаются до конца,
    // но их результаты будут помечены как Cancelled.
    void cancel();

    // Дождаться завершения текущей асинхронной операции.
    void waitAll();

    bool isRunning() const { return running_; }

    TaskStatus getStatus(const std::string& file) const;
    bool getResult(const std::string& file, std::string& out) const;
    std::map<std::string, std::pair<TaskStatus, std::string>> snapshot() const;

private:
    std::atomic<bool> running_{false};
    std::atomic<bool> cancelled_{false};
    std::thread runner_;

    mutable std::mutex mtx_;
    std::map<std::string, TaskStatus> status_;
    std::map<std::string, std::string> results_;
};