@echo off
rem Shared settings for tools\configure_vs2022.bat and tools\build_debug_vs2022.bat.
rem This file is called by those scripts, it is not meant to be run directly.
rem
rem To use the DLL variant, change both values here:
rem   CONFIGURE_PRESET=win_vs2022_x64_dll
rem   BUILD_PRESET=VS Debug DLL

set "CONFIGURE_PRESET=win_vs2022_x64"
set "BUILD_PRESET=VS Debug"
