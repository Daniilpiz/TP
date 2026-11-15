#pragma once
#include "common.h"


#include <ctime>

using namespace std;

// TASK 3a: USER DATA INPUT

inline string prompt(const string& label) {
    cout << label;
    string s;
    getline(cin, s);
    return s;
}

inline UserData inputUserData() {
    UserData d;
    d.surname    = prompt("Фамилия: ");
    d.name       = prompt("Имя: ");
    d.patronymic = prompt("Отчество: ");
    d.birthDate  = prompt("Дата рождения: ");
    d.phone      = prompt("Телефон: ");
    d.email      = prompt("Почта: ");
    return d;
}

// =====================================================================
// TASK 3b: VALIDATE DATA AND BRING IT TO THE TEMPLATE
// =====================================================================

// Full name parts: keep letters only, first letter upper case, the rest lower case
inline string cleanName(const string& name) {
    string out;out.reserve(name.size());
    for(size_t i=0;i<name.size();) {
        unsigned char a=static_cast<unsigned char>(name[i]);char32_t cp;
        if(a<128){cp=a;++i;}
        else if((a==0xd0||a==0xd1)&&i+1<name.size()&&
                static_cast<unsigned char>(name[i+1])>=0x80&&static_cast<unsigned char>(name[i+1])<=0xbf){
            cp=((a&0x1f)<<6)|(static_cast<unsigned char>(name[i+1])&0x3f);i+=2;
        } else {
            u32string src=utf8ToU32(name),res;
            for(char32_t ch:src)if(isLetterCp(ch))res.push_back(res.empty()?toUpperCp(ch):toLowerCp(ch));
            return u32ToUtf8(res);
        }
        if(!isLetterCp(cp))continue;
        cp=out.empty()?toUpperCp(cp):toLowerCp(cp);
        if(cp<128)out+=static_cast<char>(cp);
        else{out+=static_cast<char>(0xc0|(cp>>6));out+=static_cast<char>(0x80|(cp&0x3f));}
    }
    return out;
}

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

inline int daysInMonth(int m, int y) {
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && isLeap(y)) ? 29 : d[m - 1];
}

// Supports: 2002-03-01, 2002.03.01, 01.03.2002, 1/3/2002, 01 03 2002, 20020301, 01032002
inline bool validBirthDate(int y, int mo, int d) {
    if (y < 1900 || mo < 1 || mo > 12 || d < 1 || d > daysInMonth(mo, y)) return false;
    time_t now = time(nullptr);
    tm today{};
#ifdef _WIN32
    if (localtime_s(&today, &now) != 0) return false;
#else
    if (!localtime_r(&now, &today)) return false;
#endif
    int currentYear = today.tm_year + 1900, currentMonth = today.tm_mon + 1;
    return y < currentYear || (y == currentYear &&
        (mo < currentMonth || (mo == currentMonth && d <= today.tm_mday)));
}

inline string cleanDate(const string& raw) {
    size_t begin = raw.find_first_not_of(" \t\r\n\v\f");
    if (begin == string::npos) return "INVALID_DATE";
    size_t end = raw.find_last_not_of(" \t\r\n\v\f") + 1;
    int values[3] = {}, lengths[3] = {};
    auto digit = [](char c) { return c >= '0' && c <= '9'; };
    auto space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; };
    int y = 0, mo = 0, d = 0;
    bool compact = end - begin == 8;
    for (size_t i = begin; compact && i < end; ++i) compact = digit(raw[i]);
    if (compact) {
        auto number = [&](size_t start, size_t n) { int v=0; for (size_t i=0;i<n;++i) v=v*10+raw[start+i]-'0'; return v; };
        int y1=number(begin,4), m1=number(begin+4,2), d1=number(begin+6,2);
        int y2=number(begin+4,4), m2=number(begin+2,2), d2=number(begin,2);
        bool a=validBirthDate(y1,m1,d1), b=validBirthDate(y2,m2,d2);
        if (a && b && (y1!=y2 || m1!=m2 || d1!=d2)) return "INVALID_DATE";
        if (a) {y=y1;mo=m1;d=d1;} else if(b) {y=y2;mo=m2;d=d2;} else return "INVALID_DATE";
    } else {
        size_t p=begin;
        for(int field=0;field<3;++field) {
            while(p<end && digit(raw[p])) { if(++lengths[field]>(field==1?2:4)) return "INVALID_DATE"; values[field]=values[field]*10+raw[p++]-'0'; }
            if(!lengths[field]) return "INVALID_DATE";
            if(field<2) {
                size_t before=p;
                while(p<end && space(raw[p])) ++p;
                if(p<end && (raw[p]=='-' || raw[p]=='.' || raw[p]=='/')) {++p;while(p<end && space(raw[p])) ++p;}
                else if(p==before) return "INVALID_DATE";
            }
        }
        if(p!=end) return "INVALID_DATE";
        if(lengths[0]==4) {y=values[0];mo=values[1];d=values[2];}
        else if(lengths[2]==4) {d=values[0];mo=values[1];y=values[2];}
        else return "INVALID_DATE";
    }
    if(!validBirthDate(y,mo,d)) return "INVALID_DATE";
    string s="00.00.0000";
    s[0]=static_cast<char>('0'+d/10);s[1]=static_cast<char>('0'+d%10);
    s[3]=static_cast<char>('0'+mo/10);s[4]=static_cast<char>('0'+mo%10);
    for(int i=9;i>=6;--i){s[i]=static_cast<char>('0'+y%10);y/=10;}
    return s;
}

// Result: +7XXXXXXXXXX. Accepts +7..., 7..., 8..., and 10 digits without the country code
inline string cleanPhone(const string& raw) {
    string s = trim(raw);
    if (s.empty()) return "INVALID_PHONE";
    string digits;
    bool plus = s[0] == '+', inGroup = false, hadGroup = false;
    int groupDigits = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            digits += c;
            if (inGroup) ++groupDigits;
        } else if (c == '+' && i == 0) {
            continue;
        } else if (c == '(' && !inGroup && !hadGroup) {
            inGroup = true; hadGroup = true;
        } else if (c == ')' && inGroup && groupDigits == 3) {
            inGroup = false;
        } else if (c == ' ' || c == '\t' || c == '-') {
            if (inGroup) return "INVALID_PHONE";
            if (c == '-' && (i == 0 || i + 1 == s.size() || s[i - 1] < '0' || s[i - 1] > '9'
                || s[i + 1] < '0' || s[i + 1] > '9')) return "INVALID_PHONE";
        } else return "INVALID_PHONE";
    }
    if (inGroup || (plus && (digits.size() != 11 || digits[0] != '7'))) return "INVALID_PHONE";

    if (digits.size() == 11 && (digits[0] == '7' || digits[0] == '8')) return "+7" + digits.substr(1);
    if (digits.size() == 10) return "+7" + digits;
    return "INVALID_PHONE";
}

inline string cleanEmail(const string& raw) {
    string s=trim(raw);
    for(char& c:s) if(c>='A' && c<='Z') c=static_cast<char>(c+32);
    if(s.compare(0,7,"mailto:")==0) s.erase(0,7);
    size_t at=s.find('@');
    if(s.size()>254 || at==string::npos || at==0 || at>64 || at+1>=s.size()) return "INVALID_EMAIL";
    if(s[0]=='.' || s[at-1]=='.') return "INVALID_EMAIL";
    for(size_t i=0;i<at;++i) {
        unsigned char c=static_cast<unsigned char>(s[i]);
        if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='%'||c=='+'||c=='-')) return "INVALID_EMAIL";
        if(c=='.' && i && s[i-1]=='.') return "INVALID_EMAIL";
    }
    size_t label=at+1,lastDot=string::npos;
    for(size_t i=at+1;i<=s.size();++i) {
        if(i==s.size() || s[i]=='.') {
            size_t n=i-label;
            if(!n || n>63 || s[label]=='-' || s[i-1]=='-') return "INVALID_EMAIL";
            if(i<s.size()) {lastDot=i;label=i+1;}
        } else if(!((s[i]>='a'&&s[i]<='z')||(s[i]>='0'&&s[i]<='9')||s[i]=='-')) return "INVALID_EMAIL";
    }
    if(lastDot==string::npos || s.size()-lastDot-1<2) return "INVALID_EMAIL";
    for(size_t i=lastDot+1;i<s.size();++i) if(s[i]<'a'||s[i]>'z') return "INVALID_EMAIL";
    return s;
}

inline UserData validateAndFormatUserData(const UserData& raw) {
    UserData c;
    c.surname    = cleanName(raw.surname);
    c.name       = cleanName(raw.name);
    c.patronymic = cleanName(raw.patronymic);
    c.birthDate  = cleanDate(raw.birthDate);
    c.phone      = cleanPhone(raw.phone);
    c.email      = cleanEmail(raw.email);
    return c;
}

inline bool reportUserDataErrors(const UserData& data) {
    bool ok = true;
    auto check = [&](bool valid, const string& field) {
        if (!valid) { cout << "[ОШИБКА] Некорректное поле: " << field << "\n"; ok = false; }
    };
    check(!data.surname.empty(), "Фамилия");
    check(!data.name.empty(), "Имя");
    check(!data.patronymic.empty(), "Отчество");
    check(data.birthDate != "INVALID_DATE", "Дата рождения");
    check(data.phone != "INVALID_PHONE", "Телефон");
    check(data.email != "INVALID_EMAIL", "Почта");
    return ok;
}

// TASK 3c: RANDOM DATA GENERATOR

inline mt19937 rng{random_device{}()};
inline int randInt(int a, int b) { return uniform_int_distribution<>(a, b)(rng); }

// Read a dictionary: each line contains a word and a space-separated gender marker.
// Male words go to male, female words go to female. Empty lines are skipped
inline void loadDictionary(const string& path, vector<string>& male, vector<string>& female) {
    string text;
    if (!readTextFile(path, text)) return;
    istringstream iss(text);
    string line;
    while (getline(iss, line)) {
        istringstream ls(line);
        string word, gender;
        ls >> word >> gender;
        if (word.empty()) continue;
        if (gender == "ж") female.push_back(word);
        else male.push_back(word);
    }
}

// Loads 3 dictionaries (surnames.txt, names.txt, patronymics.txt) from the dir folder (e.g. "dict").
// Returns false if any file is missing or empty
inline bool loadNameDictionaries(const string& dir, NameDictionaries& d) {
    d = NameDictionaries();
    loadDictionary(dir + "/surnames.txt",    d.surnamesM,    d.surnamesF);
    loadDictionary(dir + "/names.txt",       d.namesM,       d.namesF);
    loadDictionary(dir + "/patronymics.txt", d.patronymicsM, d.patronymicsF);
    return !d.surnamesM.empty() && !d.namesM.empty() && !d.patronymicsM.empty() &&
           !d.surnamesF.empty() && !d.namesF.empty() && !d.patronymicsF.empty();
}

// Transliterates Cyrillic to Latin (for the e-mail login)
inline string translit(const string& s) {
    static const char* const table[32] = {
        "a", "b", "v", "g", "d", "e", "zh", "z", "i", "y", "k", "l", "m", "n", "o", "p",
        "r", "s", "t", "u", "f", "h", "ts", "ch", "sh", "sch", "", "y", "", "e", "yu", "ya"};
    string out;
    for (char32_t c : utf8ToU32(s)) {
        c = toLowerCp(c);
        if (c >= 0x430 && c <= 0x44F) out += table[c - 0x430];
        else if (c == 0x451) out += "e";
        else if (c < 0x80) out += static_cast<char>(c);
    }
    return out;
}

inline const string& randomItem(const vector<string>& v) {
    return v[randInt(0, static_cast<int>(v.size()) - 1)];
}

// Corrupts a name: random case + (sometimes) a digit/hyphen. Insertion works on CHARACTERS, not bytes
inline string addNameErrors(const string& s) {
    u32string u = utf8ToU32(s);
    for (char32_t& c : u) c = randInt(0, 1) ? toUpperCp(c) : toLowerCp(c);
    if (randInt(0, 1))
        u.insert(u.begin() + randInt(0, static_cast<int>(u.size())), static_cast<char32_t>(U'1' + randInt(0, 8)));
    if (randInt(0, 3) == 0)
        u.insert(u.begin() + u.size() / 2, U'-');
    return u32ToUtf8(u);
}

inline string two(int v) { return (v < 10 ? "0" : "") + to_string(v); }

inline string randomDate() {
    int y = randInt(1980, 2005), m = randInt(1, 12), d = randInt(1, daysInMonth(m, y));
    switch (randInt(0, 5)) {
        case 0:  return to_string(y) + "-" + two(m) + "-" + two(d);
        case 1:  return to_string(y) + "." + two(m) + "." + two(d);
        case 2:  return two(d) + "." + two(m) + "." + to_string(y);
        case 3:  return to_string(d) + "/" + to_string(m) + "/" + to_string(y);
        case 4:  return two(d) + " " + two(m) + " " + to_string(y);
        default: return two(d) + "-" + two(m) + "-" + to_string(y);
    }
}

inline string randomPhone() {
    string t = "9";                                   // 10 digits: 9 + 9 more random ones
    for (int i = 0; i < 9; ++i) t += static_cast<char>('0' + randInt(0, 9));
    switch (randInt(0, 5)) {
        case 0:  return "+7" + t.substr(0, 3) + "-" + t.substr(3, 4) + "-" + t.substr(7);              // +7996-8432-431
        case 1:  return "+7 (" + t.substr(0, 3) + ") " + t.substr(3, 3) + "-" + t.substr(6, 2) + "-" + t.substr(8);
        case 2:  return "8 " + t.substr(0, 3) + " " + t.substr(3, 3) + " " + t.substr(6, 2) + " " + t.substr(8);
        case 3:  return "8" + t;
        case 4:  return t;
        default: return "7-" + t.substr(0, 3) + "-" + t.substr(3, 3) + "-" + t.substr(6, 2) + "-" + t.substr(8);
    }
}

inline string randomEmail(const string& surname) {
    static const vector<string> domains = {"pochta.ru", "mail.ru", "yandex.ru", "gmail.com"};
    string login = translit(surname);
    if (randInt(0, 1)) login += string(1, static_cast<char>('a' + randInt(0, 25)));
    login += to_string(randInt(0, 99));
    string e = login + "@" + domains[randInt(0, static_cast<int>(domains.size()) - 1)];
    for (char& c : e) c = randInt(0, 1) ? static_cast<char>(toupper(static_cast<unsigned char>(c)))
                                        : static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return e;
}

// Names come from the dictionaries (male or female forms are consistent with each other)
inline UserData generateRandomUserData(const NameDictionaries& dict) {
    bool male = randInt(0, 1) == 0;
    string surname    = male ? randomItem(dict.surnamesM)    : randomItem(dict.surnamesF);
    string name       = male ? randomItem(dict.namesM)       : randomItem(dict.namesF);
    string patronymic = male ? randomItem(dict.patronymicsM) : randomItem(dict.patronymicsF);

    UserData d;
    d.surname    = addNameErrors(surname);
    d.name       = addNameErrors(name);
    d.patronymic = addNameErrors(patronymic);
    d.birthDate  = randomDate();
    d.phone      = randomPhone();
    d.email      = randomEmail(surname);
    return d;
}

// MARKDOWN FORM GENERATION

inline string generateMarkdownForm(const UserData& d) {
    string md;
    md.reserve(512+d.surname.size()+d.name.size()+d.patronymic.size()+d.email.size());
    md += "# Анкета пользователя\n\nДанные для проверки и приведения к единому формату.\n\n## Личные данные\n- Фамилия: ";
    md += d.surname; md += "\n- Имя: "; md += d.name; md += "\n- Отчество: ";md += d.patronymic;
    md += "\n\n## Контактная информация\n- Дата рождения: ";md += d.birthDate;
    md += "\n- Телефон: ";md += d.phone;md += "\n- Почта: ";md += d.email;md += '\n';
    return md;
}

inline void printUser(const UserData& d) {
    cout << d.surname << " " << d.name << " " << d.patronymic << " "
              << d.birthDate << " " << d.phone << " " << d.email << "\n";
}

// TASK 3: EXTRACT FORM DATA FROM PARSED MARKDOWN

// Extract the full name from the first paragraph (three words) or the surname, name, and patronymic list items.
// The other fields come from list items of the form "Key: value".
// Returns false if any field is missing (the list of missing fields is printed to the console).
inline bool extractUserDataFromDocument(const Document& doc, UserData& out, bool verbose = true) {
    out.surname.clear(); out.name.clear(); out.patronymic.clear();
    out.birthDate.clear(); out.phone.clear(); out.email.clear();
    bool gotSurname = false, gotName = false, gotPatr = false;
    bool gotDate = false, gotPhone = false, gotEmail = false;
    bool labeledName = false;

    for (const auto& list : doc.lists) {
        for (const auto& item : list) {
            size_t colon = item.find(':');
            if (colon == string::npos) continue;
            std::string_view keyView(item.data(),colon);
            size_t kb=keyView.find_first_not_of(" \t\r\n");
            if(kb==std::string_view::npos)continue;
            keyView=keyView.substr(kb,keyView.find_last_not_of(" \t\r\n")-kb+1);
            string key=lowerUtf8View(keyView);
            std::string_view val(item.data()+colon+1,item.size()-colon-1);
            size_t vb=val.find_first_not_of(" \t\r\n");
            if(vb==std::string_view::npos)val={};
            else val=val.substr(vb,val.find_last_not_of(" \t\r\n")-vb+1);

            if (key == "дата рождения" || key == "дата")                 { out.birthDate = val;  gotDate = !val.empty(); }
            else if (key == "телефон" || key == "тел.")                  { out.phone = val;      gotPhone = !val.empty(); }
            else if (key == "почта" || key == "email" || key == "e-mail"){ out.email = val;      gotEmail = !val.empty(); }
            else if (key == "фамилия") { out.surname = val; gotSurname = !val.empty(); labeledName = true; }
            else if (key == "имя") { out.name = val; gotName = !val.empty(); labeledName = true; }
            else if (key == "отчество") { out.patronymic = val; gotPatr = !val.empty(); labeledName = true; }
        }
    }

    // Full name in one paragraph: "Surname Name Patronymic".
    if (!labeledName && !doc.paragraphs.empty()) {
        istringstream iss(doc.paragraphs[0]);
        string a, b, c, extra;
        if ((iss >> a >> b >> c) && !(iss >> extra)) {
            out.surname = a; gotSurname = true;
            out.name = b; gotName = true;
            out.patronymic = c; gotPatr = true;
        }
    }

    bool ok = true;
    auto check = [&](bool got, const string& field) {
        if (!got) { if (verbose) cout << "[ОШИБКА] В анкете не найдено поле: " << field << "\n"; ok = false; }
    };
    check(gotSurname, "Фамилия");
    check(gotName, "Имя");
    check(gotPatr, "Отчество");
    check(gotDate, "Дата рождения");
    check(gotPhone, "Телефон");
    check(gotEmail, "Почта");
    return ok;
}
