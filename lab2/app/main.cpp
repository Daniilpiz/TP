// Лабораторная работа №2. C++ и регулярные выражения (вариант: Markdown).
// Точка входа: вся логика находится в src/*.cpp (markdown, form, app).
#include <iostream>

#include "mdform/app.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleCP(65001);         // ввод и вывод в UTF-8
    SetConsoleOutputCP(65001);
#endif
    std::ios::sync_with_stdio(false);
    return mdform::runCli(argc, argv, std::cin, std::cout);
}
