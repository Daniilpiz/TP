#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include "AsyncProcessor.h"
#include "DirectoryWatcher.h"
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

int main() {
    namespace fs = std::filesystem;

    ThreadSafeQueue<std::string> queue;
    DirectoryWatcher watcher(queue);
    ProcessingEngine engine(queue);
    AsyncProcessor   async;

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
                  << " 2. Show result of a file\n"
                  << " 3. Change watched directory\n"
                  << " 4. Reset processing results\n"
                  << " 5. Reset queue\n"
                  << "--- Part 2 (async) ---\n"
                  << " 6. Start async processing (all seen files)\n"
                  << " 7. Cancel async processing\n"
                  << " 8. Show async task status\n"
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
            if (engine.getResult(f, r)) std::cout << "Result: " << r << "\n";
            else                        std::cout << "No result for " << f << "\n";
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