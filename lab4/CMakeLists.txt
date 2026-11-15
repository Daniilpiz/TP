# Лабораторная работа №4. Многопоточная и асинхронная обработка файлов.
#
# Сборка:
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
# либо просто запустить .\build.bat (автоматически выбирает инструменты).
#
# Внимание: на Windows, если в пути к проекту есть не-ASCII символы (кириллица),
# используйте генератор Ninja или Visual Studio: генератор "MinGW Makefiles"
# не находит исходники из-за неправильной кодировки путей в Makefile.

cmake_minimum_required(VERSION 3.16)
project(Lab4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

add_executable(lab4
    main.cpp
    application.cpp
    file_processor.cpp
)
target_link_libraries(lab4 PRIVATE Threads::Threads)

# legacy/lab2.h объявляет API ЛР2; сами алгоритмы подключаются как заголовки,
# поэтому сборка выполняется в статическом режиме (без dllimport/dllexport).
target_compile_definitions(lab4 PRIVATE LAB2_STATIC)

if(MSVC)
    target_compile_options(lab4 PRIVATE /utf-8 /W4)
else()
    target_compile_options(lab4 PRIVATE -Wall -Wextra -Wpedantic)
    # MinGW: link the C++ runtime statically so lab4.exe runs by double-click
    # without MSYS2 DLLs (libstdc++-6.dll etc.) on PATH.
    target_link_options(lab4 PRIVATE -static)
endif()
