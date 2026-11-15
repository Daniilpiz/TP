#pragma once
#include <string>
#include <vector>
#include <utility>

// Only declarations are public. All algorithms are compiled into the library.
// The DLL and its client must use the same compiler and shared C++ runtime.
#if defined(_WIN32) && !defined(LAB2_STATIC)
#  if defined(LAB2_BUILD)
#    define LAB2_API __declspec(dllexport)
#  else
#    define LAB2_API __declspec(dllimport)
#  endif
#else
#  define LAB2_API
#endif

struct UserData {
    std::string surname, name, patronymic, birthDate, phone, email;
};

struct Document {
    std::vector<std::pair<int, std::string>> headers;   // (level, text)
    std::vector<std::string> paragraphs;
    std::vector<std::vector<std::string>> lists;        // each list = a set of items
};

struct NameDictionaries {
    std::vector<std::string> surnamesM, namesM, patronymicsM;
    std::vector<std::string> surnamesF, namesF, patronymicsF;
};

namespace lab2 {
LAB2_API const char* libraryKind();
LAB2_API void setupConsole();
LAB2_API bool validateMarkupLanguage(const std::string& text, bool verbose = true);
LAB2_API Document parseMarkdownElements(const std::string& text);
LAB2_API void printDocument(const Document& doc);
LAB2_API UserData inputUserData();
LAB2_API std::string cleanName(const std::string& value);
LAB2_API std::string cleanDate(const std::string& value);
LAB2_API std::string cleanPhone(const std::string& value);
LAB2_API std::string cleanEmail(const std::string& value);
LAB2_API UserData validateAndFormatUserData(const UserData& raw);
LAB2_API bool reportUserDataErrors(const UserData& data);
LAB2_API bool loadNameDictionaries(const std::string& directory, NameDictionaries& dictionaries);
LAB2_API UserData generateRandomUserData(const NameDictionaries& dictionaries);
LAB2_API std::string generateMarkdownForm(const UserData& data);
LAB2_API bool extractUserDataFromDocument(const Document& doc, UserData& out, bool verbose = true);
LAB2_API void printUser(const UserData& data);
LAB2_API bool readTextFile(const std::string& path, std::string& out);
LAB2_API bool writeTextFile(const std::string& path, const std::string& text);
// The Lab 2 console workflow, including streaming processing and preview.
LAB2_API int run(int argc, char** argv);
}
