# Nau Engine

Nau Engine is a free open-source game engine that's designed to be versatile and accessible to everyone. 

It's free: no limitations, royalties or other fees apply. Nau Engine will always be free for any purpose, including commercial use.
It's open: the entire source code is stored in a GIT repository. Anyone is able to use it for educational purposes or work with it as they wish.
It's open-source: anyone has the opportunity to participate in developing the engine without restrictions, creating forks and add-ons based on it.

## Initialize repository
`git submodule update --init --recursive`

## Prerequisites

Install the following tools required to build the engine:

* CMake
* Visual Studio 2022
  * MSVC v143
  * Windows 10 SDK 
* Python 3.10+

## Python packages

The configure step runs the ECS code generator (`cmake/_scripts/clang_ecs_parser.py`) and stops with
`The "cymbal" Python3 package is not installed` without these packages:

`py -m pip install cymbal clang`

CMake checks them with `pip show`, so the `Scripts` folder of that Python (for example
`%LOCALAPPDATA%\Programs\Python\Python313\Scripts`) must be in `PATH` when `cmake --preset` runs.

Run `cmake --preset` from cmd or PowerShell, not from Git Bash: there CMake finds Git's `gawk`, libpng then generates
its `pnglibconf.h` against another zlib and the build fails with `ZLIB_VERNUM != PNG_ZLIB_VERNUM`. If that happened,
reconfigure from PowerShell with `cmake --preset <preset> -U AWK`.

## Vcpkg setup

In any other folder, say `c:\tools`, clone vcpkg repository:
`git clone https://github.com/microsoft/vcpkg`
Run bootstrap here:
`.\vcpkg\bootstrap-vcpkg.bat`

Add enviroment variable
`VCPKG_ROOT=c:\tools\vcpkg`

After any `vcpkg.json` update (in the Nau root folder), a clean build is recommended. 
Make sure to save all of your changes, and then run this command to clean the local git tree: git clean -d -x -f

## Configure

Configure with cmake presets:
Windows
`cmake --preset win_vs2022_x64`
or (optional)
`cmake --preset win_ninja_clangcl-debug`
or use ide that does supports cmake presets (VSCode)

## Build

Build with IDE or 
`cmake --build build\win_vs2022_x64 --config=Debug`
With 16 GB of RAM limit the number of projects built at once, otherwise the compiler can fail with
`error D8040` (out of memory): `cmake --build build\win_vs2022_x64 --config=Debug -- /m:4`
or (optional) any other build
`cmake --build build\win_ninja_clangcl-debug`


## Build mode / Install
Install a previously built Engine in the `output` folder:

`cmake --install build\win_vs2022_x64 --config=Debug`

In `output` folder, run Configure and Build using same presets
