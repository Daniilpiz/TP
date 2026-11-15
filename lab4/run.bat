@echo off
rem Run lab4 watching the input folder. Double-click this file
rem (or run .\run.bat from the lab4 folder). ASCII only: cmd.exe
rem misparses UTF-8 batch files with Cyrillic.
cd /d "%~dp0"
if not exist build\lab4.exe (
    echo build\lab4.exe not found. Build first with .\build.bat
    pause
    exit /b 1
)
build\lab4.exe input
