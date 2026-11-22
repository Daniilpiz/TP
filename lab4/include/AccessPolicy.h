// AccessPolicy.h
//
// Политика доступа к файлам для ЛР №4:
//   1) Ограничение количества попыток обработки одного файла потоками
//      (лимит задаётся глобально, по умолчанию = 3).
//   2) Блокировка конкретного файла: пока файл заблокирован, ни один поток
//      не может его обработать — каждая попытка засчитывается и печатается
//      в консоль («attempt N/M»), но обработка не выполняется.
//   3) Разблокировка файла: после неё попытки снова проходят (в пределах лимита).
//
// Все методы потокобезопасны (один мьютекс защищает все счётчики и блокировки).
#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

class AccessPolicy {
public:
    struct Info {
        std::size_t attempts = 0;   // число сделанных попыток обработки
        std::size_t limit = 0;      // текущий глобальный лимит попыток
        bool blocked = false;       // заблокирован ли файл
        bool exhausted = false;     // исчерпан ли лимит попыток
    };

    static AccessPolicy& instance();

    void setMaxAttempts(std::size_t n);
    std::size_t maxAttempts() const;

    // Запросить разрешение на обработку файла потоком.
    // Возвращает true, если обработку выполнять можно.
    // Если нельзя — reason содержит объяснение, а attempt/currentLimit —
    // статистику попытки (для вывода «attempt N/M» в лог).
    bool tryAcquire(const std::string& path,
                    std::string& reason,
                    std::size_t& attempt,
                    std::size_t& currentLimit);

    void block(const std::string& path);
    void unblock(const std::string& path);
    bool isBlocked(const std::string& path) const;
    std::vector<std::string> blockedFiles() const;

    // Снять все блокировки (по полезному префиксу пути тоже).
    std::size_t unblockAll();

    Info info(const std::string& path) const;
    std::map<std::string, Info> snapshot() const;

    // Полный сброс статистики попыток (например, при resetResults()).
    void resetAttempts();

private:
    AccessPolicy() = default;

    mutable std::mutex mtx_;
    std::size_t maxAttempts_ = 3;
    std::map<std::string, std::size_t> attempts_;
    std::set<std::string> blocked_;
};
