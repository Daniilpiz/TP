// FileProcessor.h
#pragma once
#include <string>

#include "Lab2Adapter.h"

// Возвращает строковое представление результата обработки файла.
// Реализует функционал ЛР №2 (задания 1 и 2) через библиотеку mdform (ЛР №3):
// проверка текста на соответствие разметке Markdown + поиск заголовков,
// абзацев и списков. Функция потокобезопасна.
std::string processFile(const std::string& path, const lab4::Lab2Adapter& lab2);
// ЗАМЕНИТЕ тело на вашу функцию из ЛР №2 / №3.
std::string processFile(const std::string& path);
std::atomic<int> processedCount_{0};
int getProcessedCount() const { return processedCount_; }
