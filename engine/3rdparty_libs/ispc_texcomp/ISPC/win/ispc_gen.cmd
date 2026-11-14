echo off
set "exe=%~dp0ispc.exe"

if not exist %2 (
    "%exe%" %*
)

if not exist %4 (
    "%exe%" %*
)