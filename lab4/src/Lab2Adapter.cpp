#include "Lab2Adapter.h"

#include <algorithm>
#include <fstream>
#include <random>
#include <sstream>

namespace lab4 {

Lab2Adapter::Lab2Adapter(std::string dataDir) : dataDir_(std::move(dataDir)) {
    // Словари для генератора (задание 3в). Если каталог не найден -
    // loadDictionaries подставит встроенные словари для отсутствующих файлов.
    mdform::loadDictionaries(dataDir_, dict_, nullptr);
}

// ------------------------------------------------------ Задания 1 и 2 ------

Lab2Adapter::FileReport Lab2Adapter::process(const std::string& path,
                                             std::size_t limit) const {
    FileReport rep;
    rep.path = path;

    std::ifstream in(path, std::ios::binary);
    if (!in) return rep;   // opened == false
    rep.opened = true;

    // Задание 1: проверка на соответствие языку разметки (построчное чтение).
    rep.validation = mdform::validateMarkdown(in);

    // Задание 2: поиск заголовков, абзацев и списков (STL-контейнеры).
    in.clear();
    in.seekg(0, std::ios::beg);
    rep.elements = mdform::findMarkdownElements(in, limit);

    return rep;
}

std::string Lab2Adapter::briefSummary(const FileReport& rep) {
    std::ostringstream out;
    out << "[" << mdform::markupKindName(rep.validation.detected) << "] ";
    if (!rep.opened) { out << "ERROR: cannot open file"; return out.str(); }
    if (!rep.validation.ok) {
        out << "INVALID";
        if (!rep.validation.errors.empty())
            out << ": " << rep.validation.errors.front()
                << (rep.validation.errors.size() > 1 ? " ..." : "");
        return out.str();
    }
    const auto& e = rep.elements;
    out << "OK lines=" << e.lineCount
        << " headings=" << e.headingCount
        << " paragraphs=" << e.paragraphCount
        << " lists=" << e.listCount
        << " items=" << e.itemCount;
    return out.str();
}

std::string Lab2Adapter::fullReport(const FileReport& rep, std::size_t showLimit) {
    std::ostringstream out;
    out << "File: " << rep.path << "\n";
    if (!rep.opened) { out << "ERROR: cannot open file\n"; return out.str(); }

    const auto& v = rep.validation;
    out << "Detected markup: " << mdform::markupKindName(v.detected) << "\n";
    out << "Validation: " << (v.ok ? "OK (valid Markdown)" : "FAILED") << "\n";
    for (std::size_t i = 0; i < v.errors.size() && i < showLimit; ++i)
        out << "  error: " << v.errors[i] << "\n";
    if (v.errors.size() > showLimit)
        out << "  ... and " << (v.errors.size() - showLimit) << " more errors\n";

    const auto& e = rep.elements;
    out << "Lines processed: " << e.lineCount << "\n";

    out << "Headings (" << e.headingCount << "):\n";
    for (std::size_t i = 0; i < e.headings.size() && i < showLimit; ++i)
        out << "  H" << e.headings[i].level << "  " << e.headings[i].text << "\n";
    if (e.headingCount > e.headings.size())
        out << "  ... and " << (e.headingCount - e.headings.size()) << " more\n";

    out << "Paragraphs (" << e.paragraphCount << "):\n";
    for (std::size_t i = 0; i < e.paragraphs.size() && i < showLimit; ++i) {
        std::string p = e.paragraphs[i];
        if (p.size() > 70) p = p.substr(0, 67) + "...";
        std::replace(p.begin(), p.end(), '\n', ' ');
        out << "  #" << (i + 1) << "  " << p << "\n";
    }
    if (e.paragraphCount > e.paragraphs.size())
        out << "  ... and " << (e.paragraphCount - e.paragraphs.size()) << " more\n";

    out << "Lists (" << e.listCount << ", items " << e.itemCount << "):\n";
    for (std::size_t i = 0; i < e.lists.size() && i < showLimit; ++i) {
        const auto& lst = e.lists[i];
        out << "  list #" << (i + 1) << (lst.ordered ? " (ordered)" : " (unordered)")
            << ":\n";
        for (const auto& it : lst.items)
            out << "    " << std::string(it.depth * 2u + 2u, ' ') << "- "
                << it.text << "\n";
    }
    return out.str();
}

// ------------------------------------------------------- Задание 3 ---------

Lab2Adapter::Form Lab2Adapter::inputForm(std::istream& in, std::ostream& out) const {
    return mdform::inputForm(in, out);
}

std::optional<Lab2Adapter::Form> Lab2Adapter::parseLine(const std::string& line) const {
    return mdform::parseLine(line);
}

bool Lab2Adapter::normalizeForm(const Form& in, Form& out,
                                std::vector<std::string>& errors) const {
    return mdform::normalizeForm(in, out, errors);
}

Lab2Adapter::Form Lab2Adapter::generateRandom(std::uint_fast64_t seed,
                                              bool makeInvalid) const {
    std::mt19937 gen(static_cast<unsigned>(seed));
    return mdform::generateRandomForm(gen, dict_, makeInvalid);
}

int Lab2Adapter::runGeneratorTest(int count, std::ostream& out, unsigned seed) const {
    return mdform::runGeneratorTest(count, dict_, out, seed);
}

} // namespace lab4
