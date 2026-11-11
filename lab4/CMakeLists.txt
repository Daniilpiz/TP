cmake_minimum_required(VERSION 3.15)
project(Lab4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Библиотека ЛР №2 (обработка Markdown) ----
add_library(Lab2Lib STATIC src/Lab2Markdown.cpp)
target_include_directories(Lab2Lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ---- Основная программа ЛР №4 ----
add_executable(Lab4
    src/FileProcessor.cpp
    src/DirectoryWatcher.cpp
    src/ProcessingEngine.cpp
    src/AsyncProcessor.cpp
    src/main.cpp
)

target_include_directories(Lab4 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ---- Подключение библиотеки потоков (замена флагу -pthread) ----
find_package(Threads REQUIRED)
target_link_libraries(Lab4 PRIVATE 
    Lab2Lib 
    Threads::Threads
)