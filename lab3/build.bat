@echo off
rem Build lab3 with MinGW g++ (no CMake). Run from the lab3 folder:  .\build.bat
chcp 65001 >nul
setlocal
set FLAGS=-std=c++17 -O2 -Ilibrary/include

if not exist build\obj mkdir build\obj

echo [1/5] compiling static library objects...
for %%f in (markdown form app) do (
    g++ %FLAGS% -c library/src/%%f.cpp -o build/obj/%%f.o || goto :err
)

echo [2/5] creating static library build\mdform_static.lib ...
ar rcs build/mdform_static.lib build/obj/markdown.o build/obj/form.o build/obj/app.o || goto :err

echo [3/5] building demo_static.exe ...
g++ %FLAGS% demo/demo_main.cpp build/mdform_static.lib -o build/demo_static.exe || goto :err

echo [4/5] building dynamic library mdform_dyn.dll + mdform_dyn.lib ...
g++ %FLAGS% -DMDFORM_SHARED -DMDFORM_BUILDING -shared library/src/markdown.cpp library/src/form.cpp library/src/app.cpp -o build/mdform_dyn.dll -Wl,--out-implib,build/mdform_dyn.lib || goto :err

echo [5/5] building demo_dynamic.exe ...
g++ %FLAGS% -DMDFORM_SHARED demo/demo_main.cpp build/mdform_dyn.lib -o build/demo_dynamic.exe || goto :err

xcopy data build\data /E /I /Y >nul

echo.
echo DONE. Run:
echo   cd build
echo   .\demo_static.exe
echo   .\demo_dynamic.exe
exit /b 0

:err
echo.
echo BUILD FAILED - see the error above.
exit /b 1