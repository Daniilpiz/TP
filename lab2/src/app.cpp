#include "mdform/app.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "mdform/form.hpp"
#include "mdform/markdown.hpp"

namespace mdform {
namespace {

// Файл с большим буфером чтения (быстрее на огромных файлах)
struct InputFile {
    std::vector<char> buf = std::vector<char>(1 << 20);
    std::ifstream f;
    bool open(const std::string& path) {
        f.rdbuf()->pubsetbuf(buf.data(), static_cast<std::streamsize>(buf.size()));  // до open()
        f.open(path, std::ios::binary);
        return f.is_open();
    }
};

std::string trimQuotes(std::string s) {
    std::size_t b = s.find_first_not_of(" \t\r\n\"");
    if (b == std::string::npos) return "";
    std::size_t e = s.find_last_not_of(" \t\r\n\"");
    return s.substr(b, e - b + 1);
}

std::string ask(std::istream& in, std::ostream& out, const char* prompt) {
    out << prompt;
    std::string s;
    std::getline(in, s);
    return trimQuotes(s);
}

void showForm(const Form& f, std::ostream& out) {
    Form res;
    std::vector<std::string> errs;
    out << "Вход:  " << formToString(f) << "\n";
    if (normalizeForm(f, res, errs)) {
        out << "Выход: " << formToString(res) << "\n\n";
    } else {
        for (const auto& e : errs) out << "Ошибка -> " << e << "\n";
        out << "\n";
    }
}

// Проверка + (если это Markdown) поиск элементов. Возвращает код завершения.
int findInFile(const std::string& path, std::size_t limit, bool check, std::ostream& out) {
    if (check) {
        InputFile f;
        if (!f.open(path)) {
            out << "Ошибка: не удалось открыть файл " << path << "\n";
            return 1;
        }
        out << "Проверка файла на соответствие Markdown...\n";
        ValidationResult v;
        checkMarkdown(f.f, out, &v);
        if (v.detected != MarkupKind::Markdown) {
            out << "Поиск элементов не выполнен: текст не является Markdown.\n";
            return 2;
        }
        if (!v.ok) out << "(в Markdown есть ошибки, поиск всё равно выполняется)\n";
        out << "\n";
    }
    InputFile f;
    if (!f.open(path)) {
        out << "Ошибка: не удалось открыть файл " << path << "\n";
        return 1;
    }
    out << "Поиск заголовков, абзацев и списков...\n";
    printMdResult(findMarkdownElements(f.f, limit), out);
    return 0;
}

int checkFile(const std::string& path, std::ostream& out) {
    InputFile f;
    if (!f.open(path)) {
        out << "Ошибка: не удалось открыть файл " << path << "\n";
        return 1;
    }
    return checkMarkdown(f.f, out) ? 0 : 2;
}

void checkSample(const char* title, const char* text, std::ostream& out) {
    out << "--- " << title << " ---\n";
    std::istringstream ss(text);
    checkMarkdown(ss, out);
    out << "\n";
}

}  // namespace

const char* libraryKind() {
#ifdef MDFORM_SHARED
    return "динамическая библиотека (.dll)";
#else
    return "статическая библиотека (.lib) / прямая сборка";
#endif
}

void runMenu(std::istream& in, std::ostream& out, const std::string& dataDir) {
    Dictionaries dict;
    loadDictionaries(dataDir, dict, &out);
    out << "\n";

    for (;;) {
        out << "=========== Лабораторная работа №2 (Markdown) ===========\n"
               " 1 - Проверить файл на соответствие Markdown (задание 1)\n"
               " 2 - Проверить встроенные примеры: Markdown / ошибки / HTML / AsciiDoc\n"
               " 3 - Найти заголовки, абзацы и списки в файле (задание 2)\n"
               " 4 - Найти элементы во встроенном примере\n"
               " 5 - Анкета: ввод по полям (задание 3)\n"
               " 6 - Анкета: ввод одной строкой\n"
               " 7 - Анкета: пример из задания\n"
               " 8 - Генератор случайных анкет и проверка функции\n"
               " 0 - Выход\n> ";
        std::string choice;
        if (!std::getline(in, choice)) break;
        choice = trimQuotes(choice);

        if (choice == "0") break;

        if (choice == "1") {
            std::string path = ask(in, out, "Путь к файлу: ");
            if (!path.empty()) checkFile(path, out);
            out << "\n";
        } else if (choice == "2") {
            checkSample("Корректный Markdown", sampleMarkdown(), out);
            checkSample("Markdown с ошибками", sampleBrokenMarkdown(), out);
            checkSample("HTML", sampleHtml(), out);
            checkSample("AsciiDoc", sampleAsciiDoc(), out);
            checkSample("Обычный текст", "Просто строка без всякой разметки.\n", out);
        } else if (choice == "3") {
            std::string path = ask(in, out, "Путь к файлу: ");
            if (path.empty()) continue;
            std::string lim = ask(in, out, "Сколько элементов каждого вида показать [1000]: ");
            std::size_t limit = 1000;
            try { if (!lim.empty()) limit = std::stoull(lim); } catch (...) {}
            findInFile(path, limit, true, out);
            out << "\n";
        } else if (choice == "4") {
            std::istringstream ss(sampleMarkdown());
            printMdResult(findMarkdownElements(ss), out);
            out << "\n";
        } else if (choice == "5") {
            showForm(inputForm(in, out), out);
        } else if (choice == "6") {
            out << "Фамилия Имя Отчество Дата Телефон Почта:\n";
            std::string line;
            std::getline(in, line);
            if (auto f = parseLine(line)) showForm(*f, out);
            else out << "Ошибка: нужно не меньше 6 значений, разделённых пробелами\n\n";
        } else if (choice == "7") {
            showForm(*parseLine("Сидоров ИваН ПетРОви1ч 2002-03-01 +7996-8432-431 sidorovVV@pochta.ru"),
                     out);
        } else if (choice == "8") {
            std::string s = ask(in, out, "Сколько анкет сгенерировать [10]: ");
            int n = 10;
            try { if (!s.empty()) n = std::max(1, std::stoi(s)); } catch (...) {}
            runGeneratorTest(n, dict, out);
            out << "\n";
        } else {
            out << "Неизвестная команда\n\n";
        }
    }
}

int runCli(int argc, char** argv, std::istream& in, std::ostream& out) {
    if (argc <= 1) {
        runMenu(in, out);
        return 0;
    }
    const std::string cmd = argv[1];
    if ((cmd == "check" || cmd == "find") && argc >= 3) {
        const std::string path = argv[2];
        if (cmd == "check") return checkFile(path, out);

        std::size_t limit = 1000;
        bool check = true;
        for (int i = 3; i < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--skip-check") check = false;
            else {
                try { limit = std::stoull(a); } catch (...) {}
            }
        }
        return findInFile(path, limit, check, out);
    }
    out << "Использование:\n"
           "  " << argv[0] << "                           интерактивное меню\n"
           "  " << argv[0] << " check <файл.md>           проверить файл на Markdown\n"
           "  " << argv[0] << " find <файл.md> [лимит] [--skip-check]\n"
           "                                          найти заголовки, абзацы, списки\n";
    return cmd == "help" || cmd == "--help" ? 0 : 1;
}

}  // namespace mdform
