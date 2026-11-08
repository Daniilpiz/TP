#include "mdform/form.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

namespace mdform {
namespace {

// ---------------------------------------------------------------- UTF-8 -----

std::u32string toU32(const std::string& s) {
    std::u32string r;
    for (std::size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp;
        int len;
        if (c < 0x80)              { cp = c;        len = 1; }
        else if ((c >> 5) == 0x6)  { cp = c & 0x1F; len = 2; }
        else if ((c >> 4) == 0xE)  { cp = c & 0x0F; len = 3; }
        else if ((c >> 3) == 0x1E) { cp = c & 0x07; len = 4; }
        else { ++i; continue; }  // некорректный байт пропускаем
        for (int k = 1; k < len && i + k < s.size(); ++k)
            cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
        r.push_back(cp);
        i += len;
    }
    return r;
}

std::string toUtf8(const std::u32string& s) {
    std::string r;
    for (char32_t cp : s) {
        if (cp < 0x80) {
            r += static_cast<char>(cp);
        } else if (cp < 0x800) {
            r += static_cast<char>(0xC0 | (cp >> 6));
            r += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            r += static_cast<char>(0xE0 | (cp >> 12));
            r += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            r += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            r += static_cast<char>(0xF0 | (cp >> 18));
            r += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            r += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            r += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return r;
}

// Буквы: латиница и кириллица (включая Ё/ё)
bool isUpperLetter(char32_t c) {
    return (c >= 'A' && c <= 'Z') || (c >= 0x410 && c <= 0x42F) || c == 0x401;
}
bool isLowerLetter(char32_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 0x430 && c <= 0x44F) || c == 0x451;
}
bool isLetter(char32_t c) { return isUpperLetter(c) || isLowerLetter(c); }

char32_t toLower(char32_t c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c == 0x401) return 0x451;
    if (c >= 0x410 && c <= 0x42F) return c + 0x20;
    return c;
}
char32_t toUpper(char32_t c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c == 0x451) return 0x401;
    if (c >= 0x430 && c <= 0x44F) return c - 0x20;
    return c;
}

std::string trim(const std::string& s) {
    std::size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    std::size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
int daysInMonth(int y, int m) {
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && isLeap(y)) ? 29 : d[m - 1];
}

bool endsWith(const std::u32string& s, const std::u32string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

// ============================================================================
//  Проверка и нормализация
// ============================================================================

// ФИО: оставляем буквы и дефис, первая буква заглавная, остальные строчные.
// "ПетРОви1ч" -> "Петрович",  "иВАНОВ-петров" -> "Иванов-Петров"
std::optional<std::string> normalizeName(const std::string& raw, std::string& err) {
    std::u32string out;
    bool startOfPart = true;
    for (char32_t c : toU32(raw)) {
        if (isLetter(c)) {
            out.push_back(startOfPart ? toUpper(c) : toLower(c));
            startOfPart = false;
        } else if (c == '-' && !out.empty() && !startOfPart) {
            out.push_back('-');
            startOfPart = true;
        }
        // цифры, пробелы и прочие символы отбрасываются
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    if (out.empty()) {
        err = "в поле нет ни одной буквы";
        return std::nullopt;
    }
    return toUtf8(out);
}

// Даты: ГГГГ-ММ-ДД, ГГГГ.ММ.ДД, ГГГГ/ММ/ДД, ДД.ММ.ГГГГ, ДД-ММ-ГГГГ, ДД/ММ/ГГГГ -> ДД.ММ.ГГГГ
std::optional<std::string> normalizeDate(const std::string& raw, std::string& err) {
    static const std::regex reYMD(R"(^(\d{4})[-./](\d{1,2})[-./](\d{1,2})$)");
    static const std::regex reDMY(R"(^(\d{1,2})[-./](\d{1,2})[-./](\d{4})$)");

    std::string s;
    for (char c : raw)
        if (!std::isspace(static_cast<unsigned char>(c))) s += c;

    std::smatch m;
    int y, mo, d;
    if (std::regex_match(s, m, reYMD)) {
        y = std::stoi(m[1]); mo = std::stoi(m[2]); d = std::stoi(m[3]);
    } else if (std::regex_match(s, m, reDMY)) {
        d = std::stoi(m[1]); mo = std::stoi(m[2]); y = std::stoi(m[3]);
    } else {
        err = "неизвестный формат даты";
        return std::nullopt;
    }

    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);
    const int today = (now->tm_year + 1900) * 10000 + (now->tm_mon + 1) * 100 + now->tm_mday;

    if (mo < 1 || mo > 12 || d < 1 || d > daysInMonth(y, mo)) {
        err = "такой даты не существует";
        return std::nullopt;
    }
    if (y < 1900 || y * 10000 + mo * 100 + d > today) {
        err = "дата рождения вне допустимого диапазона (с 1900 года до сегодняшнего дня)";
        return std::nullopt;
    }
    char buf[32];
    std::snprintf(buf, sizeof buf, "%02d.%02d.%04d", d, mo, y);
    return std::string(buf);
}

// Телефон: +7XXXXXXXXXX, 7XXXXXXXXXX, 8XXXXXXXXXX, XXXXXXXXXX
// (допускаются пробелы, скобки и дефисы) -> +7XXXXXXXXXX
std::optional<std::string> normalizePhone(const std::string& raw, std::string& err) {
    static const std::regex reAllowed(R"(^\+?[\d\s\-()]+$)");
    static const std::regex reResult(R"(^\+7\d{10}$)");

    std::string s = trim(raw);
    if (!std::regex_match(s, reAllowed)) {
        err = "в номере есть недопустимые символы";
        return std::nullopt;
    }
    std::string digits;
    for (char c : s)
        if (std::isdigit(static_cast<unsigned char>(c))) digits += c;

    std::string result;
    if (digits.size() == 11 && digits[0] == '7') result = "+" + digits;
    else if (digits.size() == 11 && digits[0] == '8' && s[0] != '+') result = "+7" + digits.substr(1);
    else if (digits.size() == 10) result = "+7" + digits;
    else {
        err = "номер должен содержать 10 цифр после кода страны";
        return std::nullopt;
    }
    if (!std::regex_match(result, reResult)) {
        err = "номер не соответствует шаблону +7XXXXXXXXXX";
        return std::nullopt;
    }
    return result;
}

// Почта: приводится к нижнему регистру и проверяется по шаблону
std::optional<std::string> normalizeEmail(const std::string& raw, std::string& err) {
    static const std::regex re(R"(^[a-z0-9._%+\-]+@[a-z0-9\-]+(\.[a-z0-9\-]+)*\.[a-z]{2,}$)");
    std::string s = trim(raw);
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!std::regex_match(s, re)) {
        err = "адрес почты не соответствует шаблону name@domain.tld";
        return std::nullopt;
    }
    return s;
}

bool normalizeForm(const Form& in, Form& out, std::vector<std::string>& errors) {
    errors.clear();
    std::string err;
    auto take = [&](const char* field, std::optional<std::string> v, std::string& dst) {
        if (v) dst = *v;
        else errors.push_back(std::string(field) + ": " + err);
        err.clear();
    };
    take("Фамилия", normalizeName(in.surname, err), out.surname);
    take("Имя", normalizeName(in.name, err), out.name);
    take("Отчество", normalizeName(in.patronymic, err), out.patronymic);
    take("Дата рождения", normalizeDate(in.birth, err), out.birth);
    take("Телефон", normalizePhone(in.phone, err), out.phone);
    take("Почта", normalizeEmail(in.email, err), out.email);
    return errors.empty();
}

static bool nameMatchesTemplate(const std::string& s) {
    std::u32string u = toU32(s);
    if (u.empty()) return false;
    bool start = true;
    for (char32_t c : u) {
        if (c == '-') {
            if (start) return false;
            start = true;
            continue;
        }
        if (!isLetter(c)) return false;
        if (start ? !isUpperLetter(c) : !isLowerLetter(c)) return false;
        start = false;
    }
    return !start;
}

bool matchesTemplate(const Form& f) {
    static const std::regex reDate(R"(^\d{2}\.\d{2}\.\d{4}$)");
    static const std::regex rePhone(R"(^\+7\d{10}$)");
    static const std::regex reMail(R"(^[a-z0-9._%+\-]+@[a-z0-9\-]+(\.[a-z0-9\-]+)*\.[a-z]{2,}$)");
    return nameMatchesTemplate(f.surname) && nameMatchesTemplate(f.name) &&
           nameMatchesTemplate(f.patronymic) && std::regex_match(f.birth, reDate) &&
           std::regex_match(f.phone, rePhone) && std::regex_match(f.email, reMail);
}

std::string formToString(const Form& f) {
    return f.surname + " " + f.name + " " + f.patronymic + " " + f.birth + " " + f.phone + " " +
           f.email;
}

// ============================================================================
//  Ввод
// ============================================================================

Form inputForm(std::istream& in, std::ostream& out) {
    Form f;
    auto ask = [&](const char* label, std::string& dst) {
        out << label << ": ";
        std::getline(in, dst);
        dst = trim(dst);
    };
    ask("Фамилия", f.surname);
    ask("Имя", f.name);
    ask("Отчество", f.patronymic);
    ask("Дата рождения", f.birth);
    ask("Телефон", f.phone);
    ask("Почта", f.email);
    return f;
}

std::optional<Form> parseLine(const std::string& line) {
    std::istringstream ss(line);
    std::vector<std::string> t;
    for (std::string w; ss >> w;) t.push_back(w);
    if (t.size() < 6) return std::nullopt;
    Form f;
    f.surname = t[0];
    f.name = t[1];
    f.patronymic = t[2];
    f.birth = t[3];
    f.email = t.back();
    for (std::size_t i = 4; i + 1 < t.size(); ++i) f.phone += (i > 4 ? " " : "") + t[i];
    return f;
}

// ============================================================================
//  Словари и генератор
// ============================================================================

Dictionaries defaultDictionaries() {
    Dictionaries d;
    d.surnames = {"Иванов", "Петров", "Сидоров", "Смирнов", "Кузнецов", "Попов", "Васильев",
                  "Соколов", "Михайлов", "Новиков", "Фёдоров", "Морозов", "Волков", "Алексеев",
                  "Лебедев", "Козлов", "Орлов", "Никитин", "Захаров", "Зайцев"};
    d.maleNames = {"Александр", "Дмитрий", "Максим", "Сергей", "Андрей", "Алексей", "Артём",
                   "Илья", "Кирилл", "Михаил", "Никита", "Матвей", "Роман", "Егор"};
    d.femaleNames = {"Анна", "Мария", "Елена", "Ольга", "Татьяна", "Наталья", "Ирина",
                     "Екатерина", "Светлана", "Юлия", "Дарья", "Полина", "Виктория", "Алина"};
    d.patronymics = {"Александрович", "Дмитриевич", "Сергеевич", "Андреевич", "Алексеевич",
                     "Иванович", "Петрович", "Михайлович", "Владимирович", "Николаевич"};
    return d;
}

static bool loadLines(const std::string& path, std::vector<std::string>& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::vector<std::string> res;
    std::string line;
    bool first = true;
    while (std::getline(f, line)) {
        if (first && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF)
            line.erase(0, 3);
        first = false;
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        res.push_back(line);
    }
    if (res.empty()) return false;
    out = std::move(res);
    return true;
}

bool loadDictionaries(const std::string& dir, Dictionaries& out, std::ostream* log) {
    out = defaultDictionaries();
    const std::string base = dir.empty() ? "" : dir + "/";
    struct Item { const char* file; std::vector<std::string>* target; };
    const Item items[] = {{"surnames.txt", &out.surnames},
                          {"names_male.txt", &out.maleNames},
                          {"names_female.txt", &out.femaleNames},
                          {"patronymics.txt", &out.patronymics}};
    bool all = true;
    for (const auto& it : items) {
        if (loadLines(base + it.file, *it.target)) {
            if (log) *log << "Словарь " << it.file << ": " << it.target->size() << " записей\n";
        } else {
            all = false;
            if (log)
                *log << "Словарь " << base << it.file
                     << " не найден, используется встроенный (" << it.target->size()
                     << " записей)\n";
        }
    }
    return all;
}

namespace {

template <class T>
const T& pick(const std::vector<T>& v, std::mt19937& g) {
    return v[std::uniform_int_distribution<std::size_t>(0, v.size() - 1)(g)];
}

int rnd(std::mt19937& g, int lo, int hi) {
    return std::uniform_int_distribution<int>(lo, hi)(g);
}

// Женские формы из мужских
std::string femaleSurname(const std::string& s) {
    std::u32string u = toU32(s);
    if (endsWith(u, U"ий") || endsWith(u, U"ой")) {
        u.resize(u.size() - 2);
        u += U"ая";
    } else if (endsWith(u, U"ов") || endsWith(u, U"ев") || endsWith(u, U"ёв") ||
               endsWith(u, U"ин") || endsWith(u, U"ын")) {
        u += U'а';
    }
    return toUtf8(u);
}

std::string femalePatronymic(const std::string& s) {
    std::u32string u = toU32(s);
    if (endsWith(u, U"ич")) {
        u.resize(u.size() - 2);
        u += U"на";
    }
    return toUtf8(u);
}

// Случайно портит регистр и подмешивает цифру ("ПетРОви1ч")
std::string messUpName(const std::string& s, std::mt19937& g) {
    std::u32string u = toU32(s);
    for (char32_t& c : u) c = rnd(g, 0, 1) ? toUpper(c) : toLower(c);
    if (!u.empty() && rnd(g, 0, 2) == 0)
        u.insert(u.begin() + rnd(g, 0, static_cast<int>(u.size())),
                 static_cast<char32_t>('0' + rnd(g, 0, 9)));
    return toUtf8(u);
}

}  // namespace

Form generateRandomForm(std::mt19937& g, const Dictionaries& dict, bool makeInvalid) {
    Dictionaries d = dict;
    const Dictionaries def = defaultDictionaries();
    if (d.surnames.empty()) d.surnames = def.surnames;
    if (d.maleNames.empty()) d.maleNames = def.maleNames;
    if (d.femaleNames.empty()) d.femaleNames = def.femaleNames;
    if (d.patronymics.empty()) d.patronymics = def.patronymics;

    const bool female = rnd(g, 0, 1) == 1;
    std::string surname = pick(d.surnames, g);
    std::string name = female ? pick(d.femaleNames, g) : pick(d.maleNames, g);
    std::string patr = pick(d.patronymics, g);
    if (female) {
        surname = femaleSurname(surname);
        patr = femalePatronymic(patr);
    }

    Form f;
    f.surname = messUpName(surname, g);
    f.name = messUpName(name, g);
    f.patronymic = messUpName(patr, g);

    // дата в одном из форматов
    const int y = rnd(g, 1950, 2008);
    const int m = rnd(g, 1, 12);
    const int dd = rnd(g, 1, daysInMonth(y, m));
    char b[48];
    switch (rnd(g, 0, 4)) {
        case 0: std::snprintf(b, sizeof b, "%04d-%02d-%02d", y, m, dd); break;
        case 1: std::snprintf(b, sizeof b, "%02d.%02d.%04d", dd, m, y); break;
        case 2: std::snprintf(b, sizeof b, "%02d/%02d/%04d", dd, m, y); break;
        case 3: std::snprintf(b, sizeof b, "%04d.%02d.%02d", y, m, dd); break;
        default: std::snprintf(b, sizeof b, "%d-%d-%04d", dd, m, y); break;
    }
    f.birth = b;

    // телефон в одном из форматов
    std::string num = "9";
    for (int i = 1; i < 10; ++i) num += static_cast<char>('0' + rnd(g, 0, 9));
    switch (rnd(g, 0, 5)) {
        case 0: f.phone = "+7" + num; break;
        case 1: f.phone = "8" + num; break;
        case 2:
            f.phone = "+7 (" + num.substr(0, 3) + ") " + num.substr(3, 3) + "-" +
                      num.substr(6, 2) + "-" + num.substr(8);
            break;
        case 3:
            f.phone = "+7" + num.substr(0, 3) + "-" + num.substr(3, 4) + "-" + num.substr(7);
            break;
        case 4:
            f.phone = "8(" + num.substr(0, 3) + ")" + num.substr(3, 3) + num.substr(6);
            break;
        default: f.phone = num; break;
    }

    // почта со случайным регистром
    static const std::vector<std::string> domains = {"mail.ru", "pochta.ru", "gmail.com",
                                                     "yandex.ru", "example.org", "inbox.ru"};
    std::string login;
    const int len = rnd(g, 5, 10);
    for (int i = 0; i < len; ++i) {
        char c = static_cast<char>('a' + rnd(g, 0, 25));
        login += (rnd(g, 0, 2) == 0) ? static_cast<char>(std::toupper(c)) : c;
    }
    if (rnd(g, 0, 3) == 0) login += static_cast<char>('0' + rnd(g, 0, 9));
    f.email = login + "@" + pick(domains, g);

    if (makeInvalid) {  // вносим одну неисправимую ошибку
        switch (rnd(g, 0, 3)) {
            case 0: f.birth = "31.02." + std::to_string(y); break;     // нет такой даты
            case 1: f.phone = "12345"; break;                          // слишком короткий
            case 2: f.email = login + "." + pick(domains, g); break;   // нет '@'
            default: f.name = "1234"; break;                           // нет букв
        }
    }
    return f;
}

int runGeneratorTest(int count, const Dictionaries& dict, std::ostream& out, unsigned seed) {
    std::mt19937 gen(seed != 0 ? seed : std::random_device{}());
    const bool verbose = count <= 30;
    int mismatches = 0, acceptedOk = 0, rejectedOk = 0;

    for (int i = 0; i < count; ++i) {
        const bool invalid = (i % 5 == 4);
        Form in = generateRandomForm(gen, dict, invalid);
        Form res;
        std::vector<std::string> errs;
        const bool accepted = normalizeForm(in, res, errs);
        const bool good = invalid ? !accepted : (accepted && matchesTemplate(res));
        if (!good) ++mismatches;
        else if (invalid) ++rejectedOk;
        else ++acceptedOk;

        if (verbose || !good) {
            out << (good ? "[OK] " : "[!!] ") << (invalid ? "(заведомо неверная) " : "")
                << "\n  Вход:  " << formToString(in) << "\n";
            if (accepted) out << "  Выход: " << formToString(res) << "\n";
            else for (const auto& e : errs) out << "  Ошибка -> " << e << "\n";
        }
    }
    out << "\nИтог: проверено " << count << " анкет; корректные приведены к шаблону: "
        << acceptedOk << "; заведомо неверные отклонены: " << rejectedOk
        << "; расхождений: " << mismatches << "\n";
    return mismatches;
}

}  // namespace mdform
