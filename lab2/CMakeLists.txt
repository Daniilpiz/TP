cmake_minimum_required(VERSION 3.16)
project(lab2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /W3)      # исходники в UTF-8
else()
    add_compile_options(-Wall -Wextra)
endif()

add_executable(lab2
    app/main.cpp
    src/markdown.cpp
    src/form.cpp
    src/app.cpp)
target_include_directories(lab2 PRIVATE include)

# Словари кладём рядом с исполняемым файлом (каталог data)
add_custom_command(TARGET lab2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:lab2>/data)
