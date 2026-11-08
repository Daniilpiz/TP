#include "mdform/markdown.hpp"

#include <algorithm>
#include <cctype>
#include <istream>
#include <ostream>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace mdform {
namespace {

constexpr std::size_t kMaxRegexLine = 4096;  // защита std::regex от гигантских строк
constexpr std::size_t kMaxErrors = 20;       // сколько ошибок показывать

// ------------------------------------------------------------ Markdown ------
const std::regex reHeading(R"(^\s{0,3}(#{1,6})\s+(.*?)(?:\s+#+)?\s*$)");
const std::regex reHeadingTooDeep(R"(^\s{0,3}#{7,}\s.*$)");
const std::regex reHeadingNoSpace(R"(^\s{0,3}#{1,6}[^#\s].*$)");
const std::regex reBullet(R"(^(\s*)[-*+]\s+(.*)$)");
const std::regex reNumber(R"(^(\s*)\d{1,9}[.)]\s+(.*)$)");
const std::regex reRule(R"(^\s*([-*_])(\s*\1){2,}\s*$)");
const std::regex reQuote(R"(^\s{0,3}>\s?\S.*$)");
const std::regex reLinkOk(R"(\[[^\]]*\]\([^)\s]+(?:\s+"[^"]*")?\))");
const std::regex reRefDef(R"(^\s{0,3}\[[^\]]+\]:\s*\S+.*$)");
const std::regex reTableSep(R"(^\s*\|?\s*:?-{3,}:?\s*(?:\|\s*(?::?-{3,}:?\s*)?)*$)");

// ------------------------------------------------------------ AsciiDoc ------
const std::regex reAdocHeading(R"(^={1,6}\s+\S.*$)");
const std::regex reAdocAttr(R"(^:[A-Za-z0-9_][A-Za-z0-9_-]*!?:(\s.*)?$)");
const std::regex reAdocBlockAttr(
    R"(^\[(source|NOTE|TIP|IMPORTANT|WARNING|CAUTION|quote|verse|cols|options|abstract|sidebar|example|listing|literal|appendix|glossary|bibliography|partintro)[^\]]*\]\s*$)");
const std::regex reAdocTable(R"(^\|={3,}\s*$)");
const std::regex reAdocMacro(R"(^(include|image|video|audio)::\S*\[.*\]\s*$)");
const std::regex reAdocNested(R"(^\*{2,5}\s+\S.*$)");
const std::regex reAdocOrdered(R"(^\.{1,2}\s+\S.*$)");
const std::regex reAdocAdmonition(R"(^(NOTE|TIP|IMPORTANT|WARNING|CAUTION):\s+\S.*$)");

// ---------------------------------------------------------------- HTML ------
const std::regex reHtmlTag(
    R"(<\s*/?\s*(?:!DOCTYPE|html|head|body|div|span|p|h[1-6]|ul|ol|li|table|thead|tbody|tr|td|th|a|img|br|hr|script|style|meta|link|title|form|input|section|article|nav|header|footer|b|i|em|strong|pre|code)\b[^>]*>)",
    std::regex::icase);

// Подготовка строки: убрать \r, BOM, ограничить длину
void prepareLine(std::string& line, std::size_t lineNo) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (lineNo == 1 && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF)
        line.erase(0, 3);
    if (line.size() > kMaxRegexLine) line.resize(kMaxRegexLine);
}

bool isFenceLine(const std::string& s, std::size_t p, char c) {
    return p <= 3 && s.size() >= p + 3 && s[p + 1] == c && s[p + 2] == c;
}

// ============================================================================
//  Задание 1: потоковый валидатор
// ============================================================================
class Validator {
public:
    void feed(std::string line) {
        ++lines_;
        prepareLine(line, lines_);
        const std::size_t p = line.find_first_not_of(" \t");
        if (p == std::string::npos) return;  // пустая строка
        ++nonEmpty_;
        const char c = line[p];

        // --- блоки кода ``` и ~~~
        if ((c == '`' || c == '~') && isFenceLine(line, p, c) && (!inFence_ || c == fenceChar_)) {
            if (!inFence_) {
                inFence_ = true;
                fenceChar_ = c;
                fenceLine_ = lines_;
                ++strongMd_;
            } else {
                inFence_ = false;
            }
            return;
        }
        if (inFence_) return;  // внутри блока кода разметку не анализируем

        // --- признаки AsciiDoc
        if (looksLikeAsciiDoc(line, c)) {
            if (adoc_ == 0) firstAdoc_ = lines_;
            ++adoc_;
        }

        // --- признаки HTML
        if (line.find('<') != std::string::npos && std::regex_search(line, reHtmlTag)) {
            if (html_ == 0) firstHtml_ = lines_;
            ++html_;
        }

        // --- признаки Markdown и типичные ошибки
        checkMarkdownLine(line, c);
    }

    ValidationResult finish() {
        ValidationResult r;
        r.lines = lines_;

        const std::size_t foreign = std::max(adoc_, html_);
        if (nonEmpty_ == 0) {
            r.detected = MarkupKind::Empty;
            r.errors.push_back("текст пуст, проверять нечего");
        } else if (foreign > strongMd_) {
            if (adoc_ >= html_) {
                r.detected = MarkupKind::AsciiDoc;
                r.errors.push_back("текст является AsciiDoc, а не Markdown (признаков AsciiDoc: " +
                                   std::to_string(adoc_) + ", первый в строке " +
                                   std::to_string(firstAdoc_) + ")");
            } else {
                r.detected = MarkupKind::Html;
                r.errors.push_back("текст является HTML, а не Markdown (признаков HTML: " +
                                   std::to_string(html_) + ", первый в строке " +
                                   std::to_string(firstHtml_) + ")");
            }
        } else if (strongMd_ == 0 && weakMd_ == 0) {
            r.detected = MarkupKind::Plain;
            r.errors.push_back("в тексте нет ни одного элемента разметки Markdown");
        } else {
            r.detected = MarkupKind::Markdown;
            if (inFence_)
                addPending(fenceLine_, "блок кода, открытый в этой строке, не закрыт");
            std::stable_sort(pending_.begin(), pending_.end(),
                             [](const auto& a, const auto& b) { return a.first < b.first; });
            for (const auto& e : pending_)
                r.errors.push_back("строка " + std::to_string(e.first) + ": " + e.second);
            if (droppedErrors_ > 0)
                r.errors.push_back("... и ещё ошибок: " + std::to_string(droppedErrors_));
        }
        r.ok = (r.detected == MarkupKind::Markdown && r.errors.empty());
        return r;
    }

private:
    static bool looksLikeAsciiDoc(const std::string& s, char c) {
        switch (c) {
            case '=': return std::regex_match(s, reAdocHeading);
            case ':': return std::regex_match(s, reAdocAttr);
            case '[': return std::regex_match(s, reAdocBlockAttr);
            case '|': return std::regex_match(s, reAdocTable);
            case '*': return std::regex_match(s, reAdocNested);
            case '.': return std::regex_match(s, reAdocOrdered);
            case 'i': case 'v': case 'a': return std::regex_match(s, reAdocMacro);
            case 'N': case 'T': case 'I': case 'W': case 'C':
                return std::regex_match(s, reAdocAdmonition);
            default: return false;
        }
    }

    void checkMarkdownLine(const std::string& line, char c) {
        if (c == '#') {
            if (std::regex_match(line, reHeading)) ++strongMd_;
            else if (std::regex_match(line, reHeadingTooDeep))
                addPending(lines_, "заголовок содержит больше 6 символов '#'");
            else if (std::regex_match(line, reHeadingNoSpace))
                addPending(lines_, "после '#' в заголовке нет пробела");
        } else if (c == '>') {
            if (std::regex_match(line, reQuote)) ++strongMd_;
        } else if (c == '-' || c == '*' || c == '+') {
            if (std::regex_match(line, reBullet)) ++weakMd_;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            if (std::regex_match(line, reNumber)) ++weakMd_;
        } else if (c == '[') {
            if (std::regex_match(line, reRefDef)) ++strongMd_;
        }

        // ссылки и изображения [текст](адрес), ![alt](адрес)
        if (line.find("](") != std::string::npos) {
            if (std::regex_search(line, reLinkOk)) ++strongMd_;
            else addPending(lines_, "некорректная ссылка: не закрыта скобка или пустой адрес");
        }

        // таблицы (разделитель заголовка)
        if (line.find('|') != std::string::npos && line.find("---") != std::string::npos &&
            std::regex_match(line, reTableSep))
            ++strongMd_;
    }

    void addPending(std::size_t line, std::string msg) {
        if (pending_.size() < kMaxErrors) pending_.emplace_back(line, std::move(msg));
        else ++droppedErrors_;
    }

    std::size_t lines_ = 0, nonEmpty_ = 0;
    std::size_t strongMd_ = 0, weakMd_ = 0, adoc_ = 0, html_ = 0;
    std::size_t firstAdoc_ = 0, firstHtml_ = 0;
    bool inFence_ = false;
    char fenceChar_ = '`';
    std::size_t fenceLine_ = 0;
    std::vector<std::pair<std::size_t, std::string>> pending_;
    std::size_t droppedErrors_ = 0;
};

// ============================================================================
//  Задание 2: потоковый поиск заголовков, абзацев и списков
// ============================================================================
class Parser {
public:
    Parser(MdResult& res, std::size_t limit) : res_(res), limit_(limit) {}

    void feed(std::string line) {
        ++res_.lineCount;
        prepareLine(line, res_.lineCount);

        const std::size_t p = line.find_first_not_of(" \t");
        if (p == std::string::npos) {  // пустая строка завершает абзац и список
            if (!inFence_) { flushParagraph(); flushList(); }
            return;
        }
        const char c = line[p];
        std::smatch m;

        // блоки кода пропускаем
        if ((c == '`' || c == '~') && isFenceLine(line, p, c) && (!inFence_ || c == fenceChar_)) {
            flushParagraph();
            flushList();
            inFence_ = !inFence_;
            fenceChar_ = c;
            return;
        }
        if (inFence_) return;

        // заголовок, подчёркнутый === или ---  (setext)
        if (!paragraph_.empty() && (c == '=' || c == '-') && isUnderline(line, p, c)) {
            addHeading(c == '=' ? 1 : 2, paragraph_);
            paragraph_.clear();
            return;
        }
        // заголовок вида  ## Текст
        if (c == '#' && std::regex_match(line, m, reHeading)) {
            flushParagraph();
            flushList();
            addHeading(static_cast<int>(m[1].length()), m[2].str());
            return;
        }
        // горизонтальная линия
        if ((c == '-' || c == '*' || c == '_') && std::regex_match(line, reRule)) {
            flushParagraph();
            flushList();
            return;
        }
        // пункт маркированного списка
        if ((c == '-' || c == '*' || c == '+') && std::regex_match(line, m, reBullet)) {
            flushParagraph();
            addItem(false, indentWidth(m[1].str()), m[2].str());
            return;
        }
        // пункт нумерованного списка
        if (std::isdigit(static_cast<unsigned char>(c)) && std::regex_match(line, m, reNumber)) {
            flushParagraph();
            addItem(true, indentWidth(m[1].str()), m[2].str());
            return;
        }

        // обычная строка текста
        if (listOpen_ && p > 0) {  // продолжение пункта списка (с отступом)
            if (!curList_.items.empty()) curList_.items.back().text += " " + line.substr(p);
            return;
        }
        flushList();
        if (!paragraph_.empty()) paragraph_ += ' ';
        paragraph_ += line.substr(p);
    }

    void finish() {
        flushParagraph();
        flushList();
    }

private:
    static bool isUnderline(const std::string& s, std::size_t p, char c) {
        std::size_t e = s.find_last_not_of(" \t");
        std::size_t n = e - p + 1;
        if (n < (c == '=' ? 1u : 2u)) return false;
        for (std::size_t i = p; i <= e; ++i)
            if (s[i] != c) return false;
        return true;
    }

    static int indentWidth(const std::string& ws) {
        int w = 0;
        for (char ch : ws) w += (ch == '\t') ? 4 : 1;
        return w;
    }

    void addHeading(int level, const std::string& text) {
        ++res_.headingCount;
        if (res_.headings.size() < limit_) res_.headings.push_back({level, text});
    }

    void addItem(bool ordered, int indent, const std::string& text) {
        // вычисляем уровень вложенности по стеку отступов
        while (!indents_.empty() && indent < indents_.back()) indents_.pop_back();
        if (indents_.empty() || indent > indents_.back()) indents_.push_back(indent);
        const int depth = static_cast<int>(indents_.size()) - 1;

        if (listOpen_ && depth == 0 && curList_.ordered != ordered) flushList();
        if (!listOpen_) {
            listOpen_ = true;
            curList_ = MdList{};
            curList_.ordered = ordered;
            indents_.assign(1, indent);
        }
        ++res_.itemCount;
        if (res_.itemCount <= limit_ * 10) curList_.items.push_back({depth, text});
    }

    void flushParagraph() {
        if (paragraph_.empty()) return;
        ++res_.paragraphCount;
        if (res_.paragraphs.size() < limit_) res_.paragraphs.push_back(paragraph_);
        paragraph_.clear();
    }

    void flushList() {
        if (!listOpen_) return;
        ++res_.listCount;
        if (res_.lists.size() < limit_) res_.lists.push_back(curList_);
        curList_ = MdList{};
        indents_.clear();
        listOpen_ = false;
    }

    MdResult& res_;
    std::size_t limit_;
    bool inFence_ = false;
    char fenceChar_ = '`';
    bool listOpen_ = false;
    MdList curList_;
    std::vector<int> indents_;
    std::string paragraph_;
};

}  // namespace

// ============================================================================
//  Публичный интерфейс
// ============================================================================

const char* markupKindName(MarkupKind kind) {
    switch (kind) {
        case MarkupKind::Empty: return "пустой текст";
        case MarkupKind::Plain: return "обычный текст";
        case MarkupKind::Markdown: return "Markdown";
        case MarkupKind::Html: return "HTML";
        case MarkupKind::AsciiDoc: return "AsciiDoc";
    }
    return "?";
}

ValidationResult validateMarkdown(std::istream& in) {
    Validator v;
    std::string line;
    while (std::getline(in, line)) v.feed(std::move(line));
    return v.finish();
}

bool checkMarkdown(std::istream& in, std::ostream& console, ValidationResult* result) {
    ValidationResult r = validateMarkdown(in);
    if (r.ok) {
        console << "OK: текст соответствует Markdown (строк: " << r.lines << ")\n";
    } else {
        console << "Определено как: " << markupKindName(r.detected) << " (строк: " << r.lines
                << ")\n";
        for (const auto& e : r.errors) console << "Ошибка: " << e << "\n";
    }
    if (result) *result = std::move(r);
    return result ? result->ok : r.ok;
}

MdResult findMarkdownElements(std::istream& in, std::size_t limit) {
    MdResult res;
    Parser parser(res, limit);
    std::string line;
    while (std::getline(in, line)) parser.feed(std::move(line));
    parser.finish();
    return res;
}

void printMdResult(const MdResult& r, std::ostream& out) {
    out << "=== Итоги ===\n"
        << "Строк обработано: " << r.lineCount << "\n"
        << "Заголовков: " << r.headingCount << "\n"
        << "Абзацев:    " << r.paragraphCount << "\n"
        << "Списков:    " << r.listCount << " (пунктов: " << r.itemCount << ")\n\n";

    out << "=== Заголовки (показано " << r.headings.size() << ") ===\n";
    for (const auto& h : r.headings)
        out << "  [h" << h.level << "] " << h.text << "\n";

    out << "\n=== Абзацы (показано " << r.paragraphs.size() << ") ===\n";
    for (const auto& p : r.paragraphs)
        out << "  - " << (p.size() > 100 ? p.substr(0, 100) + "..." : p) << "\n";

    out << "\n=== Списки (показано " << r.lists.size() << ") ===\n";
    for (const auto& l : r.lists) {
        out << "  " << (l.ordered ? "нумерованный" : "маркированный") << " список:\n";
        for (const auto& it : l.items)
            out << "    " << std::string(static_cast<std::size_t>(it.depth) * 2, ' ') << "* "
                << it.text << "\n";
    }
}

// ------------------------------------------------------------ Примеры -------

const char* sampleMarkdown() {
    return R"(# Заголовок первого уровня

Это первый абзац,
который занимает две строки. Есть [ссылка](https://example.com).

## Список покупок

- молоко
- хлеб
  с отрубями
  - вложенный пункт
* яйца

1. первый
2. второй
3) третий

Подзаголовок через подчёркивание
--------------------------------

```cpp
# это код, а не заголовок
- и не список
```

### Последний раздел ###

Завершающий абзац.
)";
}

const char* sampleBrokenMarkdown() {
    return R"(#Заголовок без пробела

Текст со [сломанной ссылкой](http://example.com

####### слишком глубокий заголовок

- пункт списка

```
код, который никто не закрыл
)";
}

const char* sampleHtml() {
    return R"(<!DOCTYPE html>
<html>
<head><title>Пример</title></head>
<body>
<h1>Заголовок</h1>
<p>Абзац текста.</p>
<ul><li>Пункт</li></ul>
</body>
</html>
)";
}

const char* sampleAsciiDoc() {
    return R"(= Заголовок документа
Имя Автора
:toc:

== Раздел

NOTE: Это замечание.

[source,cpp]
----
int main() {}
----

* пункт 1
** вложенный пункт

. шаг первый
. шаг второй
)";
}

}  // namespace mdform
