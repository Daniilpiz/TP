#include "application.h"
#include <filesystem>
#include <iostream>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8); SetConsoleOutputCP(CP_UTF8);
#endif
    try {
        std::string mode, path;
        std::cout << "Лабораторная работа №4\n1 — многопоточная обработка\n2 — асинхронная обработка\nРежим: ";
        if (!std::getline(std::cin, mode)) return 0;
        if (mode != "1" && mode != "2") throw std::runtime_error("Выберите режим 1 или 2");
        if (argc > 2) throw std::runtime_error("Запуск: lab4.exe [каталог]");
        if (argc == 2) path = argv[1];
        else {
            std::cout << "Каталог для наблюдения (Enter = input): ";
            if (!std::getline(std::cin, path)) return 0;
        }
        if (path.empty()) {
            // По умолчанию ищем существующую папку input: рядом с местом запуска,
            // затем рядом с исполняемым файлом и в родительских каталогах —
            // это важно при запуске exe из build\ двойным щелчком.
            std::filesystem::path exeDir;
#ifdef _WIN32
            wchar_t buffer[32768];
            if (GetModuleFileNameW(nullptr, buffer, 32768))
                exeDir = std::filesystem::path(buffer).parent_path();
#else
            if (argc > 0)
                exeDir = std::filesystem::absolute(std::filesystem::path(argv[0])).parent_path();
#endif
            const auto cwd = std::filesystem::current_path();
            const std::filesystem::path candidates[] = {
                cwd / "input",
                exeDir / "input",
                exeDir.parent_path() / "input",
                exeDir.parent_path().parent_path() / "input",
            };
            path = "input"; // если ничего не найдено — создадим в текущем каталоге
            bool found = false;
            for (const auto& candidate : candidates) {
                if (candidate.empty()) continue;
                std::error_code ec;
                if (std::filesystem::is_directory(candidate, ec) && !ec) {
                    path = candidate.u8string();
                    found = true;
                    break;
                }
            }
            if (!found) std::filesystem::create_directories(path);
        }
        Application app(std::filesystem::u8path(path), mode == "2");
        std::cout << "Наблюдение за каталогом: " << app.directory().u8string() << '\n';
        std::cout << "Максимум одновременных обработчиков: " << app.concurrency() << '\n';
        std::string command;
        while (true) {
            std::cout << "\n1 — очередь\n2 — результат по ID\n3 — сменить каталог\n"
                         "4 — сбросить завершенные результаты\n5 — очистить очередь\n"
                         "6 — статусы всех файлов\n7 — отменить операцию по ID\n0 — выход\nВыбор: " << std::flush;
            if (!std::getline(std::cin, command) || command == "0") break;
            try {
                if (command == "1") app.printQueue();
                else if (command == "4") app.resetResults();
                else if (command == "5") app.resetQueue();
                else if (command == "6") app.printStatus();
                else if (command == "3") {
                    std::cout << "Новый каталог: " << std::flush;
                    if (!std::getline(std::cin, path)) break;
                    app.changeDirectory(std::filesystem::u8path(path));
                    std::cout << "Каталог изменен. Ранее добавленные операции сохраняются.\n";
                } else if (command == "2" || command == "7") {
                    std::cout << "ID файла (см. пункт 6): " << std::flush;
                    std::string value;
                    if (!std::getline(std::cin, value)) break;
                    std::uint64_t id;
                    std::istringstream parser(value);
                    if (!(parser >> id) || id == 0 || value.find('-') != std::string::npos ||
                        !(parser >> std::ws).eof()) throw std::runtime_error("Некорректный ID");
                    if (command == "2") app.printResult(id); else app.cancel(id);
                } else std::cout << "Неизвестная команда.\n";
            } catch (const std::exception& error) { std::cout << "Ошибка: " << error.what() << '\n'; }
        }
        std::cout << "Остановка: отмена операций и ожидание потоков.\n";
    } catch (const std::exception& error) {
        std::cerr << "Ошибка: " << error.what() << '\n'; return 1;
    }
    return 0;
}
