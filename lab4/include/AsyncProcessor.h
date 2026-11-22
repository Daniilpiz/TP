// AsyncProcessor.h
#pragma once
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Lab2Adapter.h"

enum class TaskStatus { Pending, Processing, Done, Cancelled, Failed, Blocked, LimitReached };

class AsyncProcessor {
public:
    explicit AsyncProcessor(const lab4::Lab2Adapter& lab2);
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
    // Пересчёт статусов Blocked / LimitReached по данным AccessPolicy.
    // Возвращает актуальную карту «файл -> статус».
    std::map<std::string, TaskStatus> refreshBlockedStatuses();

    const lab4::Lab2Adapter& lab2_;
    std::atomic<bool> running_{false};
    std::atomic<bool> cancelled_{false};

    // runner_ живёт в shared_ptr под мьютексом: join() выполняется только
    // потоком-создателем через waitAll(), что исключает гонку за thread.
    std::shared_ptr<std::thread> runner_;

    mutable std::mutex mtx_;   // защищает runner_ и карты ниже
    std::map<std::string, TaskStatus> status_;
    std::map<std::string, std::string> results_;
};
