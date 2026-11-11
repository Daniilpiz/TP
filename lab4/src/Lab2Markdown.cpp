#include "Lab2Lib.h"
#include <regex>
#include <sstream>

namespace lab2 {

const char* languageName() { return "Markdown"; }

bool validate(const std::string& text, std::string& error) {
    // 1) если есть HTML-теги — это не Markdown
    static const std::regex htmlTag(
        R"(<\s*(html|body|head|div|p|h[1-6]|ul|ol|li|a)\b[^>]*>)",
        std::regex::icase);
    if (std::regex_search(text, htmlTag)) {
        error = "text looks like HTML, not Markdown";
        return false;
    }

    // 2) если есть AsciiDoc-заголовки вида "= Title" — не Markdown
    static const std::regex asciidocHeader(
        R"(^=\s+\S)", std::regex::multiline);
    if (std::regex_search(text, asciidocHeader)) {
        error = "text looks like AsciiDoc, not Markdown";
        return false;
    }

    return true;
}

Elements extract(const std::string& text) {
    Elements el;

    // ---- Заголовки: # Title, ## Section, ... ----
    static const std::regex headerRe(
        R"(^(#{1,6})\s+(.+?)\s*#*\s*$)", std::regex::multiline);
    for (std::sregex_iterator it(text.begin(), text.end(), headerRe), end;
         it != end; ++it) {
        el.headers.push_back((*it)[2].str());
    }

    // ---- Списки: - item / * item / + item / 1. item ----
    static const std::regex listItemRe(
        R"(^\s*(?:[-*+]|\d+\.)\s+(.+)$)", std::regex::multiline);

    std::istringstream in(text);
    std::string line;
    std::string currentList;
    bool inList = false;

    auto flushList = [&]() {
        if (inList && !currentList.empty()) {
            el.lists.push_back(currentList);
            currentList.clear();
        }
        inList = false;
    };

    while (std::getline(in, line)) {
        std::smatch m;
        if (std::regex_match(line, m, listItemRe)) {
            if (!currentList.empty()) currentList += " | ";
            currentList += m[1].str();
            inList = true;
        } else {
            flushList();
        }
    }
    flushList();

    // ---- Абзацы: блоки, разделённые пустыми строками ----
    static const std::regex headerLine(R"(^#{1,6}\s+)");
    static const std::regex listLine  (R"(^\s*(?:[-*+]|\d+\.)\s+)");

    std::istringstream in2(text);
    std::string para;

    auto flushPara = [&]() {
        while (!para.empty() && (para.back() == '\n' || para.back() == ' '))
            para.pop_back();
        if (!para.empty()) el.paragraphs.push_back(para);
        para.clear();
    };

    while (std::getline(in2, line)) {
        if (line.empty()) { flushPara(); continue; }
        if (std::regex_search(line, headerLine) ||
            std::regex_search(line, listLine)) { flushPara(); continue; }

        if (!para.empty()) para += "\n";
        para += line;
    }
    flushPara();

    return el;
}

} // namespace lab2