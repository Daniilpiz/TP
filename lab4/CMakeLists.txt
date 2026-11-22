cmake_minimum_required(VERSION 3.16)
project(Lab4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /W3)      # исходники в UTF-8
else()
    add_compile_options(-Wall -Wextra)
endif()

# ---- Библиотека ЛР №2/№3 (mdform): регулярные выражения, Markdown, анкета ----
# Используется РЕАЛЬНЫЙ код лабораторных работ №2 и №3, а не упрощённая заглушка.
add_library(mdform STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../lab3/library/src/markdown.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../lab3/library/src/form.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../lab3/library/src/app.cpp)
target_include_directories(mdform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../lab3/library/include)

# ---- Основная программа ЛР №4 ----
add_executable(Lab4
    src/Lab2Adapter.cpp
    src/FileProcessor.cpp
    src/DirectoryWatcher.cpp
    src/ProcessingEngine.cpp
    src/AsyncProcessor.cpp
    src/AccessPolicy.cpp
    src/main.cpp
)

target_include_directories(Lab4 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(Threads REQUIRED)
target_link_libraries(Lab4 PRIVATE mdform Threads::Threads)

# Словари кладём рядом с исполняемым файлом (каталог data) - генератор анкет ЛР2
add_custom_command(TARGET Lab4 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../lab3/data $<TARGET_FILE_DIR:Lab4>/data)
