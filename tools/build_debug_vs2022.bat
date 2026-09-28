@echo off
rem Build Nau Engine Debug for Visual Studio 2022 (x64) using CMake presets.
rem Run tools\configure_vs2022.bat first.
rem Usage: tools\build_debug_vs2022.bat   (can be run from any directory)

setlocal

set "CONFIGURE_PRESET=win_vs2022_x64"
set "BUILD_PRESET=VS Debug"

rem Repo root = parent of this script's folder
cd /d "%~dp0.."

rem Check

where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] cmake not found in PATH.
    echo         Install CMake and make sure "cmake" is available in the console.
    exit /b 1
)

if not exist "build\%CONFIGURE_PRESET%\CMakeCache.txt" (
    echo [ERROR] Project is not configured: "build\%CONFIGURE_PRESET%" not found.
    echo         Run tools\configure_vs2022.bat first.
    exit /b 1
)

if not defined NAU_ENGINE_SOURCE_DIR (
    echo [WARNING] NAU_ENGINE_SOURCE_DIR is not set in this console.
    echo           It is set by configure via SETX and is visible only in NEW console windows.
    echo           The engine build continues; open a new console before building projects/editor.
)

rem Build

echo [INFO] Building preset "%BUILD_PRESET%"...
cmake --build --preset "%BUILD_PRESET%"
if errorlevel 1 (
    echo [ERROR] Build failed, see the output above.
    exit /b %errorlevel%
)

echo [OK] Debug build finished.
exit /b 0