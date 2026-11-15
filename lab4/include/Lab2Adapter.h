// Адаптер функционала лабораторной работы №2 (библиотека mdform, вариант Markdown).
//
// Инкапсулирует ВСЕ задания ЛР2:
//   Задание 1 - проверка текста на соответствие языку разметки
//               (mdform::validateMarkdown / checkMarkdown, сообщения об ошибках);
//   Задание 2 - поиск заголовков, абзацев и списков в контейнерах STL
//               (mdform::findMarkdownElements);
//   Задание 3 - проверка данных анкеты (ФИО, дата, телефон, почта)
//               (mdform::normalizeForm / inputForm / parseLine),
//               генератор случайных анкет (generateRandomForm / runGeneratorTest).
//
// Все методы потокобезопасны (работают только с локальными данными и const-данными).
#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "mdform/form.hpp"
#include "mdform/markdown.hpp"

namespace lab4 {

class Lab2Adapter {
public:
    explicit Lab2Adapter(std::string dataDir = "data");

    // ------------------------------------------------ Задания 1 и 2 (файл) --
    struct FileReport {
        std::string path;
        bool opened = false;
        mdform::ValidationResult validation;      // задание 1
        mdform::MdResult elements;                // задание 2 (STL-контейнеры)
    };

    // Читает файл ПОСТРОЧНО (подходит для больших файлов), проверяет разметку
    // и извлекает заголовки/абзацы/списки.
    FileReport process(const std::string& path, std::size_t limit = 1000) const;

    // Краткая однострочная сводка (для результатов движка/асинхронных задач).
    static std::string briefSummary(const FileReport& rep);

    // Подробный текстовый отчёт (для меню): ошибки валидации + найденные элементы.
    static std::string fullReport(const FileReport& rep, std::size_t showLimit = 15);

    // --------------------------------------------------- Задание 3 (анкета) --
    using Form = mdform::Form;

    // Ввод анкеты по полям из потока.
    Form inputForm(std::istream& in, std::ostream& out) const;

    // Разбор одной строки «Фамилия Имя Отчество Дата Телефон Почта».
    std::optional<Form> parseLine(const std::string& line) const;

    // Проверка и нормализация всех полей; причины ошибок - в errors.
    bool normalizeForm(const Form& in, Form& out, std::vector<std::string>& errors) const;

    // Генерация случайной анкеты по словарям (из dataDir либо встроенным).
    Form generateRandom(std::uint_fast64_t seed, bool makeInvalid = false) const;

    // Прогон count сгенерированных анкет через проверку. Возвращает число расхождений.
    int runGeneratorTest(int count, std::ostream& out, unsigned seed = 0) const;

    const mdform::Dictionaries& dictionaries() const { return dict_; }

private:
    std::string dataDir_;
    mdform::Dictionaries dict_;
};

} // namespace lab4
