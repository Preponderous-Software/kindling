@echo off
setlocal
rem Plays Kindling (the cozy homestead game on the Strata engine) from the
rem latest main branch, building it first. The game saves in
rem %USERPROFILE%\kindling-save and continues where you left off.
rem Extra arguments are passed to the game, e.g.:
rem   play-kindling.bat --new                 start over (the old save is kept as the previous one)
rem   play-kindling.bat --autopilot --speed 4 watch the autopilot restore the cabin

set "KD_DIR=%USERPROFILE%\kindling-play"
set "KD_REPO=https://github.com/Preponderous-Software/kindling.git"
set "SAVE_DIR=%USERPROFILE%\kindling-save"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "INSTALLED="

where git >nul 2>nul || (echo Git is not installed: https://git-scm.com/download/win & pause & exit /b 1)

where cmake >nul 2>nul || (
    echo Kindling needs CMake 3.24 or newer. Installing it with winget ...
    winget install --id Kitware.CMake -e --accept-source-agreements --accept-package-agreements || (echo Install it from https://cmake.org/download/ & pause & exit /b 1)
    set "INSTALLED=1"
)

set "HAS_MSVC="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "HAS_MSVC=1"
)
if not defined HAS_MSVC (
    echo Kindling needs the Visual Studio C++ build tools. Installing them with winget, this takes a while ...
    winget install --id Microsoft.VisualStudio.2022.BuildTools -e --accept-source-agreements --accept-package-agreements --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended" || (echo Install "Desktop development with C++" from https://visualstudio.microsoft.com/visual-cpp-build-tools/ & pause & exit /b 1)
    set "INSTALLED=1"
)

if defined INSTALLED (
    echo Tools installed. Close this window and run the script again.
    pause
    exit /b 0
)

if not exist "%KD_DIR%\.git" (
    echo Cloning Kindling into %KD_DIR% ...
    git clone -b main "%KD_REPO%" "%KD_DIR%" || (pause & exit /b 1)
) else (
    echo Updating %KD_DIR% ...
    pushd "%KD_DIR%"
    git fetch origin || (popd & pause & exit /b 1)
    git checkout -B main origin/main || (popd & pause & exit /b 1)
    popd
)

pushd "%KD_DIR%"
echo Building (the first time fetches and builds Strata and SDL3, a few minutes) ...
cmake -S . -B build -DKINDLING_BUILD_TESTS=OFF -Wno-dev || (popd & pause & exit /b 1)
cmake --build build --config Release --parallel --target kindling || (popd & pause & exit /b 1)

build\Release\kindling.exe --slot "%SAVE_DIR%" %*
set "RC=%ERRORLEVEL%"
popd
if not "%RC%"=="0" pause
exit /b %RC%
