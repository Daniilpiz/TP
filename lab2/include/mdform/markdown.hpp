// Задания 1 и 2 лабораторной работы №2 (вариант: Markdown)
//   1) проверка текста на соответствие языку разметки Markdown;
//   2) поиск заголовков, абзацев и списков (результаты - в контейнерах STL).
//
// Обе функции работают с std::istream и читают текст ПОСТРОЧНО, поэтому
// подходят для очень больших файлов (десятки гигабайт).
#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include "mdform/export.hpp"

namespace mdform {

// ----------------------------------------------------------------- Задание 1

enum class MarkupKind {
    Empty,     // пустой текст
    Plain,     // обычный текст без разметки
    Markdown,
    Html,
    AsciiDoc
};

struct ValidationResult {
    bool ok = false;                    // true: корректный Markdown без ошибок
    MarkupKind detected = MarkupKind::Empty;
    std::vector<std::string> errors;    // сообщения об ошибках (по-русски)
    std::size_t lines = 0;              // сколько строк прочитано
};

MDFORM_API const char* markupKindName(MarkupKind kind);

// Проверяет текст из потока. Ничего не печатает.
MDFORM_API ValidationResult validateMarkdown(std::istream& in);

// Проверяет текст и ВЫВОДИТ сообщения об ошибках в консоль (поток console).
// Возвращает true, если текст корректен. Заполняет result, если он не nullptr.
MDFORM_API bool checkMarkdown(std::istream& in, std::ostream& console,
                              ValidationResult* result = nullptr);

// ----------------------------------------------------------------- Задание 2

struct Heading {
    int level;           // 1..6
    std::string text;
};

struct ListItem {
    int depth;           // 0 - верхний уровень, 1 - вложенный и т.д.
    std::string text;
};

struct MdList {
    bool ordered = false;
    std::vector<ListItem> items;
};

struct MdResult {
    // Сохранённые элементы (не более limit каждого вида)
    std::vector<Heading> headings;
    std::vector<std::string> paragraphs;
    std::vector<MdList> lists;
    // Счётчики учитывают ВСЕ найденные элементы, даже не сохранённые
    std::size_t headingCount = 0;
    std::size_t paragraphCount = 0;
    std::size_t listCount = 0;
    std::size_t itemCount = 0;
    std::size_t lineCount = 0;
};

// Ищет заголовки (# и подчёркнутые === / ---), абзацы и списки
// (маркированные и нумерованные, с вложенностью).
// limit - сколько элементов каждого вида хранить в памяти.
MDFORM_API MdResult findMarkdownElements(std::istream& in, std::size_t limit = 1000);

MDFORM_API void printMdResult(const MdResult& result, std::ostream& out);

// ----------------------------------------------------------- Примеры текстов

MDFORM_API const char* sampleMarkdown();
MDFORM_API const char* sampleBrokenMarkdown();
MDFORM_API const char* sampleHtml();
MDFORM_API const char* sampleAsciiDoc();

}  // namespace mdform
