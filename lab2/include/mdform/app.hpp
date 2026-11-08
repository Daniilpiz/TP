// Консольное приложение лабораторной работы №2 (меню и разбор аргументов).
// Вынесено в библиотеку, чтобы в ЛР3 все реализованные функции были в .lib и .dll.
#pragma once

#include <iosfwd>
#include <string>

#include "mdform/export.hpp"

namespace mdform {

// Какая это сборка: "статическая библиотека (.lib)" или "динамическая библиотека (.dll)"
MDFORM_API const char* libraryKind();

// Интерактивное меню. dataDir - каталог со словарями (surnames.txt и т.д.)
MDFORM_API void runMenu(std::istream& in, std::ostream& out, const std::string& dataDir = "data");

// Разбор аргументов командной строки:
//   (без аргументов)                  - интерактивное меню
//   check <файл.md>                   - проверить файл на Markdown
//   find  <файл.md> [лимит] [--skip-check] - найти заголовки, абзацы, списки
// Возвращает код завершения процесса.
MDFORM_API int runCli(int argc, char** argv, std::istream& in, std::ostream& out);

}  // namespace mdform
