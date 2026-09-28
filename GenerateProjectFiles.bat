@echo off

cd /d "%~dp0"

echo Generating Visual Studio 2026 project files...

premake5.exe vs2026

if errorlevel 1 (
    echo.
    echo [ERROR] Failed to generate project files.
    pause
    exit /b 1
)

echo.
echo Project files generated successfully.
pause