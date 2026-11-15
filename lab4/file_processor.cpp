#include "file_processor.h"
// Include the Lab 2/3 algorithm implementations only in this translation unit.
#include "legacy/markdown.h"
#include "legacy/userdata.h"
#include <array>

ProcessingStats processFile(const std::filesystem::path& input,
                            const std::filesystem::path& partial,
                            const std::atomic<bool>& cancel,
                            std::atomic<std::uint64_t>& bytes) {
    std::ifstream in(input, std::ios::binary);
    std::ofstream out(partial, std::ios::binary | std::ios::trunc);
    if (!in || !out) throw std::runtime_error("Не удалось открыть входной файл или результат");
    StreamingMarkdown parser;
    ProcessingStats stats;
    bool hasForm = false, firstLine = true;
    std::size_t formBytes = 0;
    constexpr std::size_t limit = 1024 * 1024;
    auto check = [&] { if (cancel.load()) throw OperationCancelled(); };
    auto finishForm = [&] {
        if (!hasForm) return;
        check(); parser.finish();
        UserData raw;
        if (!parser.markupOK || !extractUserDataFromDocument(parser.doc, raw, false)) {
            ++stats.invalid; return;
        }
        const auto clean = validateAndFormatUserData(raw);
        if (clean.surname.empty() || clean.name.empty() || clean.patronymic.empty() ||
            clean.birthDate == "INVALID_DATE" || clean.phone == "INVALID_PHONE" ||
            clean.email == "INVALID_EMAIL") { ++stats.invalid; return; }
        out << generateMarkdownForm(clean) << '\n';
        if (!out) throw std::runtime_error("Ошибка записи результата");
        ++stats.valid;
    };
    auto consume = [&](std::string line) {
        check();
        if (firstLine && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
        firstLine = false;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!parser.fence.marker && line == "# Анкета пользователя") {
            finishForm(); parser.reset(); hasForm = true; formBytes = 0;
        }
        if (!hasForm) {
            if (!trimView(line).empty()) throw std::runtime_error("Ожидался заголовок # Анкета пользователя");
            return;
        }
        formBytes += line.size() + 1;
        if (formBytes > limit) throw std::runtime_error("Анкета превышает 1 МиБ");
        parser.consume(line);
    };
    // Keep memory usage bounded even for multi-gigabyte files and very long lines.
    std::array<char, 64 * 1024> buffer;
    std::string pending;
    while (in) {
        check(); in.read(buffer.data(), buffer.size());
        const auto count = in.gcount();
        if (in.bad() || (in.fail() && !in.eof())) throw std::runtime_error("Ошибка чтения");
        for (std::streamsize i = 0; i < count; ++i) {
            if (buffer[i] == '\n') { consume(std::move(pending)); pending.clear(); }
            else {
                if (pending.size() >= limit) throw std::runtime_error("Строка превышает 1 МиБ");
                pending += buffer[i];
            }
        }
        bytes.fetch_add(static_cast<std::uint64_t>(count));
    }
    if (!pending.empty()) consume(std::move(pending));
    finishForm(); check();
    if (!hasForm) throw std::runtime_error("Анкеты не найдены");
    out.close();
    if (!out) throw std::runtime_error("Ошибка закрытия результата");
    return stats;
}
