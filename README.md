# Лабораторная работа №3. Статические и динамические библиотеки

Весь код ЛР2 перенесён в библиотеку **mdform** (`library/`). Исходники библиотеки совпадают с ЛР2, добавлен только макрос `MDFORM_API` перед экспортируемыми функциями (`include/mdform/export.hpp`).

## Состав решения

| Проект | Результат | Что это |
|---|---|---|
| `mdform_static` | `mdform_static.lib` | 1. Статическая библиотека |
| `mdform_dyn` | `mdform_dyn.dll` + `mdform_dyn.lib` | 2. Динамическая библиотека (`.lib` здесь только библиотека импорта) |
| `demo_static` | `demo_static.exe` | 3. Демо со статической библиотекой |
| `demo_dynamic` | `demo_dynamic.exe` | 3. Демо с динамической библиотекой |

Оба демо компилируются из одного файла `demo/demo_main.cpp` и вызывают функции из ЛР2: проверка Markdown, поиск элементов, проверка анкеты, генератор. Первой строкой демо выводит, какая библиотека подключена (`libraryKind()`).

## Сборка

### Visual Studio 2019/2022
```
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
build\bin\Release\demo_static.exe
build\bin\Release\demo_dynamic.exe
```
(или `cmake -S . -B build` и открыть `build\lab3.sln`). Файлы `.exe`, `.dll` и каталог `data` попадают в одну папку, поэтому exe находит dll.
Файлы `.lib` лежат в `build\lib\Release`.

### Linux
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/demo_static
./build/bin/demo_dynamic --menu
```
(на Linux вместо `.lib` / `.dll` получатся `.a` / `.so`).

## Как устроен экспорт из DLL

`include/mdform/export.hpp`:
- при сборке DLL заданы `MDFORM_SHARED` и `MDFORM_BUILDING`, и `MDFORM_API` превращается в `__declspec(dllexport)`;
- в проекте-потребителе задан только `MDFORM_SHARED`, и это `__declspec(dllimport)`;
- в статической библиотеке макрос пустой.

В CMake это строка `target_compile_definitions(mdform_dyn PUBLIC MDFORM_SHARED PRIVATE MDFORM_BUILDING)`.

## То же самое вручную в Visual Studio (без CMake)

**Статическая библиотека**
1. Новый проект «Статическая библиотека (C++)», имя `mdform_static`.
2. Добавить `library/src/*.cpp` и `library/include/mdform/*.hpp`.
3. Свойства -> C/C++ -> Общие -> Дополнительные каталоги включаемых файлов: `library\include`.
4. Свойства -> C/C++ -> Язык -> Стандарт языка: **C++17**. Командная строка -> `/utf-8`.

**Динамическая библиотека**
1. Новый проект «Библиотека динамической компоновки (DLL)», имя `mdform_dyn`; те же исходники и настройки.
2. Препроцессор: `MDFORM_SHARED;MDFORM_BUILDING`.

**Демо-проекты** (консольное приложение, `demo/demo_main.cpp`)
1. Включаемые каталоги: `library\include`, стандарт C++17, `/utf-8`.
2. Ссылка на проект (Ссылки -> Добавить ссылку) на `mdform_static` или `mdform_dyn` либо Компоновщик -> Ввод -> Дополнительные зависимости: `mdform_static.lib` / `mdform_dyn.lib`.
3. Для dll-варианта в препроцессоре демо добавить `MDFORM_SHARED`; `mdform_dyn.dll` должна лежать рядом с exe.
4. Скопировать каталог `data` в рабочий каталог запуска (иначе будут встроенные словари).

Все проекты должны быть собраны с одинаковыми настройками (конфигурация, x86/x64, `/MD` или `/MT`).

## Ограничение
Интерфейс DLL использует `std::string`, `std::vector`, `std::istream` и т. д. Это нормально, когда DLL и exe собраны одним компилятором и с одной настройкой CRT (как в этой работе), но такую DLL нельзя подключить к программе, собранной другим компилятором.
