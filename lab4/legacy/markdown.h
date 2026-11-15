#pragma once
#include "common.h"


using namespace std;

// Laboratory Markdown profile: paragraphs, ATX/Setext headings, lists and code.
// Raw HTML/XML, LaTeX, BBCode and MediaWiki outside code are rejected.
// Fenced code blocks must be closed. See MARKDOWN_RULES.md.
static const regex RE_HEADER(R"(^ {0,3}(#{1,6})([ \t]+(.*)|[ \t]*)$)");
static const regex RE_LIST_ITEM(R"(^ {0,3}([-*+]|[0-9]{1,9}[.)])([ \t]+(.*)|[ \t]*)$)");

inline vector<string> markdownLines(const string& text) {
    istringstream input(text);
    vector<string> lines;
    string line;
    while (getline(input, line)) {
        if (lines.empty() && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

struct CodeFence {
    char marker = 0;
    size_t length = 0;

    // Returns true for a fence boundary and for lines inside the block.
    bool consume(const string& line) {
        size_t p = line.find_first_not_of(' ');
        if (p == string::npos || p > 3) return marker != 0;
        char c = line[p];
        size_t end = p;
        while (end < line.size() && line[end] == c) ++end;
        size_t count = end - p;
        if (marker) {
            if (c == marker && count >= length && trim(line.substr(end)).empty()) {
                marker = 0; length = 0;
            }
            return true;
        }
        if ((c == '`' || c == '~') && count >= 3 &&
            (c != '`' || line.find('`', end) == string::npos)) {
            marker = c; length = count;
            return true;
        }
        return false;
    }
};

// Hide paired inline code spans before checking for other markup languages.
inline string withoutInlineCode(string line) {
    for (size_t p = 0; p < line.size();) {
        if (line[p] == '\\' && p + 1 < line.size()) { p += 2; continue; }
        if (line[p] != '`') { ++p; continue; }
        size_t end = p;
        while (end < line.size() && line[end] == '`') ++end;
        size_t count = end - p, close = end;
        bool found = false;
        while ((close = line.find('`', close)) != string::npos) {
            size_t next = close;
            while (next < line.size() && line[next] == '`') ++next;
            if (next - close == count) {
                fill(line.begin() + p, line.begin() + next, ' ');
                p = next; found = true; break;
            }
            close = next;
        }
        if (!found) p = end;
    }
    return line;
}

inline bool validateMarkupLanguage(const string& text, bool verbose = true) {
    static const regex foreignTags(
        R"(<!DOCTYPE\b|<\?xml\b|<!--[\s\S]*|</?[a-z_][a-z0-9_.:-]*([ \t]+[^<>]*)?/?>)", regex::icase);
    static const regex latex(R"(\\(documentclass|begin|end|section|subsection|usepackage)\b)");
    static const regex bbcode(R"(\[/?(b|i|u|url|img|quote|code|list)(=[^\]]*)?\])", regex::icase);
    static const regex wiki(R"(^[ \t]*={2,}[^=]+={2,}[ \t]*$)");
    CodeFence fence;
    auto error = [&](size_t line, const string& message) {
        if (verbose) cout << "[ОШИБКА] Строка " << line << ": " << message << "\n";
        return false;
    };
    auto lines = markdownLines(text);
    for (size_t i = 0; i < lines.size(); ++i) {
        const string& line = lines[i];
        if (fence.consume(line)) continue;
        if (line.compare(0, 4, "    ") == 0 || (!line.empty() && line[0] == '\t')) continue;
        string visible = withoutInlineCode(line);
        if (regex_search(visible, foreignTags)) return error(i + 1, "обнаружен HTML/XML вне блока кода");
        if (regex_search(visible, latex)) return error(i + 1, "обнаружен LaTeX вне блока кода");
        if (regex_search(visible, bbcode)) return error(i + 1, "обнаружен BBCode вне блока кода");
        if (regex_match(visible, wiki)) return error(i + 1, "обнаружен MediaWiki");
    }
    if (fence.marker) return error(lines.size(), "не закрыт блок кода");
    // Plain text is a valid Markdown paragraph, including '#text'.
    return true;
}

inline int setextLevel(const string& line) {
    static const regex underline(R"(^ {0,3}(=+|-+)[ \t]*$)");
    smatch match;
    if (!regex_match(line, match, underline)) return 0;
    return match[1].str()[0] == '=' ? 1 : 2;
}

inline bool thematicBreak(const string& line) {
    static const regex rule(R"(^ {0,3}((\*[ \t]*){3,}|(-[ \t]*){3,}|(_[ \t]*){3,})$)");
    return regex_match(line, rule);
}

inline string headingText(string text) {
    text = trim(text);
    size_t last = text.size();
    while (last && text[last - 1] == '#') --last;
    if (last < text.size() && (last == 0 || text[last - 1] == ' ' || text[last - 1] == '\t'))
        text = trim(text.substr(0, last));
    return text;
}

inline Document parseMarkdownElements(const string& text) {
    Document doc;
    string para;
    vector<string> currentList;
    string listType;
    size_t listIndent = 0;
    CodeFence fence;
    auto flushPara = [&] { if (!para.empty()) { doc.paragraphs.push_back(para); para.clear(); } };
    auto flushList = [&] { if (!currentList.empty()) { doc.lists.push_back(currentList); currentList.clear(); } };

    for (const auto& line : markdownLines(text)) {
        if (fence.consume(line)) { flushPara(); flushList(); continue; }
        if (line.compare(0, 4, "    ") == 0 || (!line.empty() && line[0] == '\t')) {
            flushPara(); flushList(); continue;
        }
        if (trim(line).empty()) { flushPara(); flushList(); continue; }
        int level = setextLevel(line);
        if (level && !para.empty()) {
            flushList();
            doc.headers.emplace_back(level, para); para.clear();
            continue;
        }
        smatch match;
        if (regex_match(line, match, RE_HEADER)) {
            flushPara(); flushList();
            doc.headers.emplace_back(static_cast<int>(match[1].length()), headingText(match[3]));
        } else if (thematicBreak(line)) {
            flushPara(); flushList();
        } else if (regex_match(line, match, RE_LIST_ITEM)) {
            flushPara();
            string marker = match[1];
            string type = isdigit(static_cast<unsigned char>(marker[0])) ? string("ordered") + marker.back() : marker;
            size_t indent = line.find_first_not_of(' ');
            if (!currentList.empty() && (type != listType || indent != listIndent)) flushList();
            listType = type; listIndent = indent;
            currentList.push_back(trim(match[3]));
        } else {
            flushList();
            if (!para.empty()) para += ' ';
            para += trim(line);
        }
    }
    flushPara(); flushList();
    return doc;
}

inline void printDocument(const Document& doc) {
    cout << "Заголовки (" << doc.headers.size() << "):\n";
    for (const auto& h : doc.headers) cout << "  [h" << h.first << "] " << h.second << "\n";
    cout << "Абзацы (" << doc.paragraphs.size() << "):\n";
    for (const auto& p : doc.paragraphs) cout << "  " << p << "\n";
    cout << "Списки (" << doc.lists.size() << "):\n";
    for (size_t i = 0; i < doc.lists.size(); ++i) {
        cout << "  Список " << i + 1 << ":\n";
        for (const auto& it : doc.lists[i]) cout << "    * " << it << "\n";
    }
}



// Streaming parser: every line and every form is checked again, without a cache.
#include <string_view>
using std::string_view;

inline string_view trimView(string_view s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string_view::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

struct StreamingMarkdown {
    Document doc;
    CodeFence fence;
    bool markupOK = true;
    string error;
    string para;
    size_t hc = 0, pc = 0, lc = 0, itemCount = 0;
    bool listActive = false;
    char listType = 0;
    size_t listIndent = 0;

    void reset() {
        fence = CodeFence(); markupOK = true; error.clear(); para.clear();
        hc = pc = lc = itemCount = 0; listActive = false; listType = 0; listIndent = 0;
    }
    void header(int level, string_view text) {
        if (hc == doc.headers.size()) doc.headers.emplace_back();
        doc.headers[hc].first = level;
        doc.headers[hc++].second.assign(text.empty() ? "" : text.data(), text.size());
    }
    void flushPara() {
        if (para.empty()) return;
        if (pc == doc.paragraphs.size()) doc.paragraphs.emplace_back();
        doc.paragraphs[pc++].assign(para);
        para.clear();
    }
    void flushList() {
        if (!listActive) return;
        doc.lists[lc - 1].resize(itemCount);
        listActive = false; itemCount = 0;
    }
    static bool horizontal(string_view s) {
        if (s.empty() || (s[0] != '-' && s[0] != '*' && s[0] != '_')) return false;
        char marker = s[0]; size_t count = 0;
        for (char c : s) {
            if (c == marker) ++count;
            else if (c != ' ' && c != '\t') return false;
        }
        return count >= 3;
    }
    void consume(string_view line) {
        size_t indent = 0;
        while (indent < line.size() && line[indent] == ' ') ++indent;
        if (fence.marker || (indent <= 3 && indent < line.size() &&
            (line[indent] == '`' || line[indent] == '~'))) {
            if (fence.consume(string(line))) { flushPara(); flushList(); return; }
        }
        if (indent >= 4 || (!line.empty() && line[0] == '\t')) {
            flushPara(); flushList(); return;
        }
        string_view s = trimView(line);
        if (s.empty()) { flushPara(); flushList(); return; }
        // Only candidate characters require the original foreign-markup regexes.
        // This is a syntax prefilter; it is applied to every line, not reused across forms.
        if (markupOK && line.find_first_of("<\\[=`") != string_view::npos &&
            !validateMarkupLanguage(string(line), false)) {
            markupOK = false; error = "[ОШИБКА] Обнаружена запрещённая разметка.\n";
        }
        string_view body = line.substr(indent);
        if (!para.empty() && (body[0] == '=' || body[0] == '-')) {
            size_t n = 0;
            while (n < body.size() && body[n] == body[0]) ++n;
            if (trimView(body.substr(n)).empty()) {
                flushList(); header(body[0] == '=' ? 1 : 2, para); para.clear(); return;
            }
        }
        if (body[0] == '#') {
            size_t n = 0;
            while (n < body.size() && body[n] == '#') ++n;
            if (n <= 6 && (n == body.size() || body[n] == ' ' || body[n] == '\t')) {
                flushPara(); flushList();
                string_view title = trimView(body.substr(n));
                size_t e = title.size();
                while (e && title[e - 1] == '#') --e;
                if (e < title.size() && (!e || title[e - 1] == ' ' || title[e - 1] == '\t'))
                    title = trimView(title.substr(0, e));
                header(static_cast<int>(n), title); return;
            }
        }
        if (horizontal(body)) { flushPara(); flushList(); return; }
        size_t markerEnd = 0;
        char type = 0;
        if (body[0] == '-' || body[0] == '*' || body[0] == '+') {
            markerEnd = 1; type = body[0];
        } else if (body[0] >= '0' && body[0] <= '9') {
            size_t n = 0;
            while (n < body.size() && body[n] >= '0' && body[n] <= '9') ++n;
            if (n <= 9 && n < body.size() && (body[n] == '.' || body[n] == ')')) {
                markerEnd = n + 1; type = body[n];
            }
        }
        if (markerEnd && (markerEnd == body.size() || body[markerEnd] == ' ' || body[markerEnd] == '\t')) {
            flushPara();
            if (listActive && (type != listType || indent != listIndent)) flushList();
            if (!listActive) {
                if (lc == doc.lists.size()) doc.lists.emplace_back();
                ++lc; listActive = true; itemCount = 0; listType = type; listIndent = indent;
            }
            auto& list = doc.lists[lc - 1];
            if (itemCount == list.size()) list.emplace_back();
            string_view value = trimView(body.substr(markerEnd));
            list[itemCount++].assign(value.empty() ? "" : value.data(), value.size());
        } else {
            flushList();
            if (!para.empty()) para += ' ';
            para.append(s.data(), s.size());
        }
    }
    void finish() {
        flushPara(); flushList();
        doc.headers.resize(hc); doc.paragraphs.resize(pc); doc.lists.resize(lc);
        if (fence.marker) { markupOK = false; error = "[ОШИБКА] Не закрыт блок кода.\n"; }
    }
};
