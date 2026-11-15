@echo off
rem Lab4 build: run from the lab4 folder:  .\build.bat
rem Prefers Ninja (handles non-ASCII paths correctly); falls back to direct g++.
rem NOTE: this file is ASCII-only because cmd.exe misparses UTF-8 batch files.
setlocal

rem MSYS2 UCRT64 tools, in case they are not on PATH yet.
set "PATH=C:\msys64\ucrt64\bin;%PATH%"

where ninja >nul 2>nul
if errorlevel 1 goto :no_ninja
where cmake >nul 2>nul
if errorlevel 1 goto :no_ninja

echo [1/2] cmake configure (Ninja)...
for /f "delims=" %%G in ('where g++ 2^>nul') do if not defined GXX set "GXX=%%G"
if defined GXX (set "CXXFLAG=-DCMAKE_CXX_COMPILER=%GXX%") else (set "CXXFLAG=")
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release %CXXFLAG%
if errorlevel 1 goto :err
echo [2/2] build...
cmake --build build
if errorlevel 1 goto :err
goto :done

:no_ninja
where g++ >nul 2>nul
if errorlevel 1 goto :no_tools
echo [fallback] direct g++ build...
if not exist build mkdir build
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -static -DLAB2_STATIC -I. main.cpp application.cpp file_processor.cpp -o build\lab4.exe
if errorlevel 1 goto :err
goto :done

:no_tools
echo.
echo Neither Ninja+CMake nor g++ was found. Install MSYS2 UCRT64
echo (packages mingw-w64-ucrt-x86_64-toolchain, -ninja, -cmake) or Visual Studio.
exit /b 1

:done
echo.
echo DONE. Run: build\lab4.exe [directory]
exit /b 0

:err
echo.
echo BUILD FAILED - see the error above.
echo If the build folder is stale, remove it manually:  rd /s /q build
exit /b 1
