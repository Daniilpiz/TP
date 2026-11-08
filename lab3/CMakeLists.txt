cmake_minimum_required(VERSION 3.16)
project(lab3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /W3)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Все .exe и .dll попадают в одну папку bin/ (чтобы exe нашёл dll)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(MDFORM_SOURCES
    library/src/markdown.cpp
    library/src/form.cpp
    library/src/app.cpp)

# ---------------------------------------------------------------------------
# 1. Статическая библиотека  ->  mdform_static.lib  (Linux: libmdform_static.a)
# ---------------------------------------------------------------------------
add_library(mdform_static STATIC ${MDFORM_SOURCES})
target_include_directories(mdform_static PUBLIC library/include)

# ---------------------------------------------------------------------------
# 2. Динамическая библиотека  ->  mdform_dyn.dll + mdform_dyn.lib (библиотека импорта)
#    MDFORM_SHARED  - нужен и библиотеке, и потребителю (PUBLIC)
#    MDFORM_BUILDING - только при сборке самой DLL (PRIVATE): включает dllexport
# ---------------------------------------------------------------------------
add_library(mdform_dyn SHARED ${MDFORM_SOURCES})
target_include_directories(mdform_dyn PUBLIC library/include)
target_compile_definitions(mdform_dyn PUBLIC MDFORM_SHARED PRIVATE MDFORM_BUILDING)

# ---------------------------------------------------------------------------
# 3. Демонстрационные проекты (один исходник, две линковки)
# ---------------------------------------------------------------------------
add_executable(demo_static demo/demo_main.cpp)
target_link_libraries(demo_static PRIVATE mdform_static)

add_executable(demo_dynamic demo/demo_main.cpp)
target_link_libraries(demo_dynamic PRIVATE mdform_dyn)

foreach(t demo_static demo_dynamic)
    add_custom_command(TARGET ${t} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:${t}>/data)
endforeach()
