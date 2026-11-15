#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

#include "AsyncProcessor.h"
#include "DirectoryWatcher.h"
#include "Lab2Adapter.h"
#include "ProcessingEngine.h"
#include "ThreadSafeQueue.h"

static const char* statusName(TaskStatus s) {
    switch (s) {
        case TaskStatus::Pending:    return "Pending";
        case TaskStatus::Processing: return "Processing";
        case TaskStatus::Done:       return "Done";
        case TaskStatus::Cancelled:  return "Cancelled";
        case TaskStatus::Failed:     return "Failed";
    }
    return "?";
}

// Каталог со словарями для генератора анкет (задание 3в ЛР2):
// сначала рядом с исполняемым файлом, затем исходники lab3.
static std::string findDataDir() {
    namespace fs = std::filesystem;
    for (const char* cand : {"data", "../data", "../../lab3/data", "../../lab2/data"}) {
        if (fs::exists(fs::path(cand) / "surnames.txt")) return cand;
    }
    return "data";   // loadDictionaries подставит встроенные словари
}

int main(int argc, char** argv) {
    namespace fs = std::filesystem;

    // Единый экземпляр функционала ЛР №2 (задания 1-3), используется из всех потоков.
    const lab4::Lab2Adapter lab2(findDataDir());

    ThreadSafeQueue<std::string> queue;
    DirectoryWatcher watcher(queue);
    ProcessingEngine engine(queue, lab2);
    AsyncProcessor   async(lab2);

    // Неинтерактивный режим: Lab4 --process <dir> [file.md ...]
    // обрабатывает каталог и печатает ПОДРОБНЫЙ отчёт по каждому файлу (ЛР2, задания 1-2).
    if (argc > 2 && std::strcmp(argv[1], "--process") == 0) {
        const std::string dir = argv[2];
        if (!fs::is_directory(dir)) { std::cerr << "Not a directory\n"; return 1; }
        for (int i = 3; i < argc; ++i) {
            auto rep = lab2.process(fs::path(dir) / argv[i]);
            std::cout << lab4::Lab2Adapter::fullReport(rep) << "\n";
        }
        return 0;
    }

    std::string dir;
    std::cout << "Directory to watch: ";
    std::getline(std::cin, dir);
    if (dir.empty()) dir = ".";
    if (!fs::is_directory(dir)) { std::cerr << "Not a directory\n"; return 1; }

    watcher.start(dir);
    engine.start();

    bool quit = false;
    while (!quit) {
        std::cout << "\n===== MENU =====\n"
                  << " 1. Show queue state\n"
                  << " 2. Show result of a file (detailed report, LR2 tasks 1-2)\n"
                  << " 3. Change watched directory\n"
                  << " 4. Reset processing results\n"
                  << " 5. Reset queue\n"
                  << "--- Part 2 (async) ---\n"
                  << " 6. Start async processing (all seen files)\n"
                  << " 7. Cancel async processing\n"
                  << " 8. Show async task status\n"
                  << "--- LR2 task 3 (form validation) ---\n"
                  << " 9. Input and validate a form interactively\n"
                  << "10. Validate a single line (Surname Name Patronymic Date Phone Email)\n"
                  << "11. Generate random forms from dictionaries and validate them\n"
                  << "12. Run generator self-test (N generated forms through validator)\n"
                  << " 0. Exit\n> ";
        int c = -1;
        if (!(std::cin >> c)) { std::cin.clear(); std::cin.ignore(1<<20, '\n'); continue; }
        std::cin.ignore(1<<20, '\n');

        switch (c) {
        case 1: {
            auto s = queue.snapshot();
            std::cout << "Queue size: " << s.size() << "\n";
            for (auto& p : s) std::cout << "  " << p << "\n";
            break;
        }
        case 2: {
            std::cout << "File: ";
            std::string f; std::getline(std::cin, f);
            std::string r;
            if (engine.getResult(f, r)) {
                std::cout << "Summary: " << r << "\n";
                // подробный отчёт по элементам ЛР2 (задания 1 и 2)
                std::cout << lab4::Lab2Adapter::fullReport(lab2.process(f));
            } else std::cout << "No result for " << f << "\n";
            break;
        }
        case 3: {
            std::cout << "New directory: ";
            std::string nd; std::getline(std::cin, nd);
            if (fs::is_directory(nd)) { watcher.changeDirectory(nd); std::cout << "OK\n"; }
            else                      { std::cout << "Invalid path\n"; }
            break;
        }
        case 4:
            engine.resetResults();
            std::cout << "Results reset\n";
            break;
        case 5:
            queue.clear();
            std::cout << "Queue cleared\n";
            break;
        case 6: {
            // все файлы, которые когда-либо видел engine
            auto files = engine.getAllSeenFiles();

            // плюс те, что ещё висят в очереди прямо сейчас
            for (const auto& f : queue.snapshot()) {
                if (std::find(files.begin(), files.end(), f) == files.end())
                    files.push_back(f);
            }

            if (files.empty()) {
                std::cout << "No files yet (watcher hasn't found any)\n";
                break;
            }

            async.startAsync(files);
            std::cout << "Async started on " << files.size() << " files\n";
            break;
        }
        case 7:
            async.cancel();
            std::cout << "Cancellation requested\n";
            break;
        case 8: {
            auto s = async.snapshot();
            std::cout << "Async state: " << (async.isRunning() ? "running" : "idle") << "\n";
            for (auto& [f, pr] : s)
                std::cout << "  [" << statusName(pr.first) << "] " << f
                          << (pr.second.empty() ? "" : " -> " + pr.second) << "\n";
            break;
        }
        case 9: {
            // Задание 3а+3б: ввод данных анкеты и их проверка/нормализация.
            auto in = lab2.inputForm(std::cin, std::cout);
            lab4::Lab2Adapter::Form out;
            std::vector<std::string> errs;
            if (lab2.normalizeForm(in, out, errs)) {
                std::cout << "Normalized:\n" << mdform::formToString(out) << "\n";
            } else {
                std::cout << "Errors:\n";
                for (auto& e : errs) std::cout << "  - " << e << "\n";
            }
            break;
        }
        case 10: {
            std::cout << "Line: ";
            std::string line; std::getline(std::cin, line);
            if (auto parsed = lab2.parseLine(line)) {
                lab4::Lab2Adapter::Form out;
                std::vector<std::string> errs;
                if (lab2.normalizeForm(*parsed, out, errs))
                    std::cout << "Normalized:\n" << mdform::formToString(out) << "\n";
                else {
                    std::cout << "Parsed, but validation failed:\n";
                    for (auto& e : errs) std::cout << "  - " << e << "\n";
                }
            } else std::cout << "Cannot parse line\n";
            break;
        }
        case 11: {
            // Задание 3в: генератор случайных данных по словарям ФИО.
            std::cout << "Count: ";
            int n = 0;
            if (!(std::cin >> n) || n <= 0 || n > 1000) { std::cin.clear(); std::cin.ignore(1<<20,'\n'); n = 5; }
            std::cin.ignore(1<<20, '\n');
            std::mt19937_64 gen(std::random_device{}());
            for (int i = 0; i < n; ++i) {
                auto raw = lab2.generateRandom(gen());
                std::cout << "Input : " << mdform::formToString(raw) << "\n";
                lab4::Lab2Adapter::Form norm;
                std::vector<std::string> errs;
                if (lab2.normalizeForm(raw, norm, errs))
                    std::cout << "Output: " << mdform::formToString(norm) << "\n";
                else {
                    std::cout << "Output: REJECTED\n";
                    for (auto& e : errs) std::cout << "  - " << e << "\n";
                }
            }
            break;
        }
        case 12: {
            std::cout << "Count: ";
            int n = 0;
            if (!(std::cin >> n) || n <= 0 || n > 10000) { std::cin.clear(); std::cin.ignore(1<<20,'\n'); n = 20; }
            std::cin.ignore(1<<20, '\n');
            int mismatches = lab2.runGeneratorTest(n, std::cout);
            std::cout << (mismatches == 0 ? "SELF-TEST PASSED\n"
                                          : "SELF-TEST FAILED (" + std::to_string(mismatches) + " mismatches)\n");
            break;
        }
        case 0:
            quit = true;
            break;
        default:
            std::cout << "Unknown option\n";
        }
    }

    async.cancel();
    async.waitAll();
    engine.stop();
    watcher.stop();
    return 0;
}
