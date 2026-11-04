@echo off
rem Run a NetWalk executable with the opt-in profiler from the executable's
rem own directory.  Usage:
rem
rem   tools\run_profiled.cmd build\windows-x64-mingw-debug\netwalk.exe
rem
rem The profiler exists only in debug builds (the *-debug presets).
rem It writes nwprof.csv next to the executable, in the current directory, or
rem in %TEMP% if the first two are not writable.
setlocal
if "%~1"=="" (
    echo usage: %~nx0 path\to\netwalk.exe
    exit /b 2
)
set "EXE=%~f1"
if not exist "%EXE%" (
    echo not found: %EXE%
    exit /b 1
)
cd /d "%~dp1"
set "NW_DEBUG_PROFILE=1"
set "NW_NO_AUDIO=1"
if not "%~2"=="" set "NW_RENDER_SCALE=%~2"
echo running: %EXE%
echo env: NW_DEBUG_PROFILE=%NW_DEBUG_PROFILE% NW_NO_AUDIO=%NW_NO_AUDIO% NW_RENDER_SCALE=%NW_RENDER_SCALE%
"%EXE%"
echo exit=%ERRORLEVEL%
if exist nwprof.csv (
    echo profile: %CD%\nwprof.csv
) else (
    echo profile not found in %CD%; check %TEMP%\nwprof.csv
)
pause
