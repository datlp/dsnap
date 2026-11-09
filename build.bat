@echo off
setlocal

echo ============================================================
echo   dsnap - Build Script (Modern C++ Win32 Native)
echo ============================================================

:: Terminate running instance before building
taskkill /f /im dsnap.exe >nul 2>&1

set "WINGET_BIN=C:\Users\Dat\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261020-ucrt-x86_64\bin"

if exist "%WINGET_BIN%\clang++.exe" (
    set "PATH=%WINGET_BIN%;%PATH%"
    set "CXX=%WINGET_BIN%\clang++.exe"
    set "RC=%WINGET_BIN%\windres.exe"
    set "STRIP=%WINGET_BIN%\strip.exe"
    goto do_compile
)

where clang++ >nul 2>&1
if %errorlevel% equ 0 (
    set "CXX=clang++"
    set "RC=windres"
    set "STRIP=strip"
    goto do_compile
)

where g++ >nul 2>&1
if %errorlevel% equ 0 (
    set "CXX=g++"
    set "RC=windres"
    set "STRIP=strip"
    goto do_compile
)

where cl >nul 2>&1
if %errorlevel% equ 0 (
    echo [INFO] Detected MSVC compiler (cl.exe)...
    rc /fo res\dsnap.res /Ires res\dsnap.rc
    cl /O2 /std:c++17 /EHsc /Isrc /Ires src\*.cpp res\dsnap.res /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib gdiplus.lib shell32.lib advapi32.lib ole32.lib oleaut32.lib strmiids.lib urlmon.lib /OUT:dsnap.exe
    if %errorlevel% equ 0 (
        echo [SUCCESS] Built dsnap.exe successfully with MSVC!
        dir dsnap.exe
    )
    goto end
)

echo [ERROR] No C++ compiler found!
goto end

:do_compile
echo [INFO] Using compiler: %CXX%
echo [INFO] Compiling Windows resources...
pushd res
"%RC%" -i dsnap.rc -o dsnap.res -O coff
popd

echo [INFO] Compiling modular sources into dsnap.exe with -O3 optimization...
set "SRCS=src\main.cpp src\common.cpp src\config.cpp src\audio.cpp src\capture.cpp src\overlay.cpp src\recorder.cpp src\tray.cpp src\hotkey.cpp src\i18n.cpp src\click_effect.cpp"
"%CXX%" -O3 -std=c++17 -mwindows -Isrc -Ires %SRCS% res\dsnap.res -o dsnap.exe -lgdi32 -luser32 -lshell32 -ladvapi32 -lgdiplus -lole32 -loleaut32 -lstrmiids -lurlmon -static

if %errorlevel% equ 0 (
    if not "%STRIP%"=="" (
        "%STRIP%" --strip-all dsnap.exe
    )
    echo ============================================================
    echo [SUCCESS] Built dsnap.exe successfully!
    echo ============================================================
    dir dsnap.exe
) else (
    echo [ERROR] Compilation failed!
)

:end
