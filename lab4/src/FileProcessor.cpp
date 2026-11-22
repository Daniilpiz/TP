#include "FileProcessor.h"

#include <iostream>

std::atomic<int> processedCount{0};

int getProcessedCount() { return processedCount.load(); }

// Обработка файла с учётом политики доступа (AccessPolicy):
//  - пока файл заблокирован, потоки получают отказ (попытка засчитывается);
//  - после превышения лимита попыток файл больше не обрабатывается.
// Возвращает пустую строку, если обработка запрещена политикой.
std::string processFile(const std::string& path, const lab4::Lab2Adapter& lab2) {
    std::string reason;
    std::size_t attempt = 0, limit = 0;
    if (!AccessPolicy::instance().tryAcquire(path, reason, attempt, limit)) {
        // Заблокированный/исчерпавший лимит файл НЕ считается обработанным.
        return std::string("DENIED: ") + reason +
               " (attempt " + std::to_string(attempt) + "/" +
               std::to_string(limit) + ")";
    }

    // Задания 1 и 2 ЛР №2: валидация разметки + извлечение элементов.
    // Чтение построочное - большие файлы не загружаются в память целиком.
    auto rep = lab2.process(path);
    ++processedCount;
    return lab4::Lab2Adapter::briefSummary(rep);
}
