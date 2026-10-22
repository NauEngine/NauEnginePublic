@echo off
rem Configure Nau Engine for Visual Studio 2022 (x64) using CMake presets.
rem Usage: tools\configure_vs2022.bat   (can be run from any directory)

setlocal

rem CONFIGURE_PRESET and BUILD_PRESET are shared with build_debug_vs2022.bat
call "%~dp0vs2022_config.bat"

rem Repo root = parent of this script's folder
cd /d "%~dp0.."

rem Check

where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] cmake not found in PATH.
    echo         Install CMake and make sure "cmake" is available in the console.
    exit /b 1
)

if not defined VCPKG_ROOT (
    echo [ERROR] VCPKG_ROOT environment variable is not set.
    echo         Clone https://github.com/microsoft/vcpkg, run bootstrap-vcpkg.bat
    echo         and set VCPKG_ROOT to the vcpkg folder, e.g. VCPKG_ROOT=C:\tools\vcpkg
    exit /b 1
)

if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    echo [ERROR] vcpkg toolchain not found: "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
    echo         Check that VCPKG_ROOT points to a bootstrapped vcpkg folder.
    exit /b 1
)

rem Configure

echo [INFO] Configuring preset "%CONFIGURE_PRESET%"...
cmake --preset "%CONFIGURE_PRESET%"
if errorlevel 1 (
    echo [ERROR] CMake configure failed, see the output above.
    exit /b %errorlevel%
)

echo [OK] Configure finished. Next step: tools\build_debug_vs2022.bat
exit /b 0