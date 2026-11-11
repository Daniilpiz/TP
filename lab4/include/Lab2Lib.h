#pragma once
#include <string>
#include <vector>

namespace lab2 {

struct Elements {
    std::vector<std::string> headers;
    std::vector<std::string> paragraphs;
    std::vector<std::string> lists;
};

// true — текст соответствует языку разметки, иначе в error пишется причина
bool validate(const std::string& text, std::string& error);

// Извлекает заголовки, абзацы, списки (STL-контейнеры)
Elements extract(const std::string& text);

const char* languageName();   // "Markdown" / "HTML" / "AsciiDoc"

} // namespace lab2