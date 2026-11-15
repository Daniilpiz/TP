#include "FileProcessor.h"

std::string processFile(const std::string& path, const lab4::Lab2Adapter& lab2) {
    // Задания 1 и 2 ЛР №2: валидация разметки + извлечение элементов.
    // Чтение построочное - большие файлы не загружаются в память целиком.
    auto rep = lab2.process(path);
    return lab4::Lab2Adapter::briefSummary(rep);
}
#include "Lab2Lib.h"
#include <fstream>
#include <sstream>
#include <chrono>

std::string processFile(const std::string& path) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::ifstream f(path);
    if (!f) return "ERROR: cannot open " + path;

    std::ostringstream buf;
    buf << f.rdbuf();
    const std::string text = buf.str();

    std::ostringstream out;
    out << "[" << lab2::languageName() << "] ";

    std::string err;
    if (!lab2::validate(text, err)) {
        out << "INVALID: " << err;
        return out.str();
    }

    auto el = lab2::extract(text);
    out << "headers="    << el.headers.size()
        << " paragraphs=" << el.paragraphs.size()
        << " lists="      << el.lists.size();
    return out.str();
}
