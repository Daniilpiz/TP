#include "FileProcessor.h"

std::string processFile(const std::string& path, const lab4::Lab2Adapter& lab2) {
    // Задания 1 и 2 ЛР №2: валидация разметки + извлечение элементов.
    // Чтение построочное - большие файлы не загружаются в память целиком.
    auto rep = lab2.process(path);
    return lab4::Lab2Adapter::briefSummary(rep);
}
