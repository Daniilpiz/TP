// Задание 3 лабораторной работы №2: проверка данных анкеты.
//   а) функция ввода данных          - inputForm / parseLine
//   б) функция проверки и приведения - normalizeForm (+ normalizeName/Date/Phone/Email)
//   в) генератор случайных данных    - generateRandomForm / runGeneratorTest
#pragma once

#include <iosfwd>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "mdform/export.hpp"

namespace mdform {

struct Form {
    std::string surname, name, patronymic;
    std::string birth;   // после проверки: ДД.ММ.ГГГГ
    std::string phone;   // после проверки: +7XXXXXXXXXX
    std::string email;   // после проверки: в нижнем регистре
};

// ------------------------------------------------------------ Проверка ------
// Каждая функция возвращает значение в шаблонном виде или std::nullopt,
// а причину отказа записывает в err.

MDFORM_API std::optional<std::string> normalizeName(const std::string& raw, std::string& err);
MDFORM_API std::optional<std::string> normalizeDate(const std::string& raw, std::string& err);
MDFORM_API std::optional<std::string> normalizePhone(const std::string& raw, std::string& err);
MDFORM_API std::optional<std::string> normalizeEmail(const std::string& raw, std::string& err);

// Проверяет все поля анкеты. true - все поля приведены к шаблону (результат в out).
MDFORM_API bool normalizeForm(const Form& in, Form& out, std::vector<std::string>& errors);

// Соответствует ли уже готовая анкета шаблону (используется при тестировании)
MDFORM_API bool matchesTemplate(const Form& f);

MDFORM_API std::string formToString(const Form& f);

// -------------------------------------------------------------- Ввод --------

// Ввод по полям: фамилия, имя, отчество, дата рождения, телефон, почта
MDFORM_API Form inputForm(std::istream& in, std::ostream& out);

// Разбор одной строки: Фамилия Имя Отчество Дата Телефон(может содержать пробелы) Почта
MDFORM_API std::optional<Form> parseLine(const std::string& line);

// ---------------------------------------------------------- Генератор -------

struct Dictionaries {
    std::vector<std::string> surnames;      // мужские формы
    std::vector<std::string> maleNames;
    std::vector<std::string> femaleNames;
    std::vector<std::string> patronymics;   // мужские формы
};

// Небольшие встроенные словари (запасной вариант)
MDFORM_API Dictionaries defaultDictionaries();

// Загрузка словарей из каталога dir: surnames.txt, names_male.txt,
// names_female.txt, patronymics.txt (по одному слову в строке).
// Если файл не найден, для него берётся встроенный словарь.
// Возвращает true, если все файлы найдены. Сообщения пишутся в log (если не nullptr).
MDFORM_API bool loadDictionaries(const std::string& dir, Dictionaries& out,
                                 std::ostream* log = nullptr);

// Случайная анкета с "испорченным" регистром, лишними цифрами, разными форматами
// даты и телефона. Если makeInvalid == true, в одно поле вносится неисправимая ошибка.
MDFORM_API Form generateRandomForm(std::mt19937& gen, const Dictionaries& dict,
                                   bool makeInvalid = false);

// Прогоняет count сгенерированных анкет через normalizeForm и проверяет результат.
// Каждая 5-я анкета намеренно некорректна и обязана быть отклонена.
// Возвращает число расхождений с ожиданием (0 - всё работает правильно).
// seed == 0 -> случайное зерно.
MDFORM_API int runGeneratorTest(int count, const Dictionaries& dict, std::ostream& out,
                                unsigned seed = 0);

}  // namespace mdform
