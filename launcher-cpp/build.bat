@echo off
setlocal
cd /d %~dp0
set GCC=
if defined GCC_HOME if exist "%GCC_HOME%\g++.exe" set GCC=%GCC_HOME%\g++.exe
if not defined GCC (
  where g++ >nul 2>nul
  if not errorlevel 1 set GCC=g++
)
if not defined GCC (
  echo g++ not found. Install MinGW-w64 or LLVM and rerun.
  pause
  exit /b 1
)
if exist app.res del app.res
windres app.rc -O coff -o app.res
if errorlevel 1 (
  echo WINDRES FAILED
  pause
  exit /b 1
)
"%GCC%" -std=c++17 -O2 -finput-charset=UTF-8 -fexec-charset=UTF-8 -DUNICODE -D_UNICODE -municode -static-libgcc -static-libstdc++ src\main.cpp app.res -o EtherealLauncher.exe -lwininet -lgdi32 -lmsimg32 -lcrypt32 -liphlpapi -lshell32 -lole32 -luuid -lwinmm
if errorlevel 1 (
  echo BUILD FAILED
  pause
  exit /b 1
)
echo BUILD OK: EtherealLauncher.exe
pause
