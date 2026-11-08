// Лабораторная работа №3. Демонстрационный проект.
// Один и тот же код собирается дважды:
//   demo_static  - линковка со статической библиотекой (mdform_static.lib)
//   demo_dynamic - линковка с динамической библиотекой (mdform_dyn.dll + mdform_dyn.lib)
// Вызываются функции из ЛР2: проверка Markdown, поиск элементов, проверка анкеты, генератор.
#include <iostream>
#include <random>
#include <sstream>
#include <string>

#include "mdform/app.hpp"
#include "mdform/form.hpp"
#include "mdform/markdown.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace mdform;

static void section(const char* title) {
    std::cout << "\n==================== " << title << " ====================\n";
}

static void checkText(const char* title, const char* text) {
    std::cout << "--- " << title << " ---\n";
    std::istringstream in(text);
    checkMarkdown(in, std::cout);   // при ошибке печатает сообщения в консоль
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
#endif
    std::ios::sync_with_stdio(false);

    std::cout << "Демонстрация библиотеки mdform\n"
              << "Подключена: " << libraryKind() << "\n";

    // --- Задание 1: проверка соответствия языку разметки
    section("Задание 1: проверка Markdown");
    checkText("Корректный Markdown", sampleMarkdown());
    checkText("Markdown с ошибками", sampleBrokenMarkdown());
    checkText("HTML", sampleHtml());
    checkText("AsciiDoc", sampleAsciiDoc());

    // --- Задание 2: поиск заголовков, абзацев, списков
    section("Задание 2: заголовки, абзацы, списки");
    {
        std::istringstream in(sampleMarkdown());
        printMdResult(findMarkdownElements(in), std::cout);
    }

    // --- Задание 3: проверка анкеты
    section("Задание 3: проверка анкеты");
    {
        auto form = parseLine("Сидоров ИваН ПетРОви1ч 2002-03-01 +7996-8432-431 sidorovVV@pochta.ru");
        Form result;
        std::vector<std::string> errors;
        std::cout << "Вход:  " << formToString(*form) << "\n";
        if (normalizeForm(*form, result, errors))
            std::cout << "Выход: " << formToString(result) << "\n";
        else
            for (const auto& e : errors) std::cout << "Ошибка -> " << e << "\n";
    }

    // --- Задание 4: генератор случайных данных + проверка функции
    section("Задание 4: генератор и проверка");
    {
        Dictionaries dict;
        loadDictionaries("data", dict, &std::cout);
        std::cout << "\n";
        int bad = runGeneratorTest(10, dict, std::cout);
        std::cout << (bad == 0 ? "Все проверки пройдены.\n" : "Есть расхождения!\n");
    }

    // --- по желанию: то же самое в виде интерактивного меню
    if (argc > 1 && std::string(argv[1]) == "--menu") {
        section("Интерактивное меню");
        runMenu(std::cin, std::cout, "data");
    } else {
        std::cout << "\n(запустите с параметром --menu, чтобы открыть интерактивное меню)\n";
    }
    return 0;
}
