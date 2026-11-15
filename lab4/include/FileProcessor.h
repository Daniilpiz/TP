// FileProcessor.h
#pragma once
#include <string>

// Возвращает строковое представление результата обработки файла.
// ЗАМЕНИТЕ тело на вашу функцию из ЛР №2 / №3.
std::string processFile(const std::string& path);
std::atomic<int> processedCount_{0};
int getProcessedCount() const { return processedCount_; }