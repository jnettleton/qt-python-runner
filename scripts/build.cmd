@echo off
rem Configure, build and test one preset from a plain shell (no Developer
rem prompt needed). Usage: scripts\build.cmd [preset] [configure|build|test|all]
rem   preset  msvc-debug (default) | msvc-release | msvc-asan
rem Env overrides: VSINSTALL (VS install dir), QT_PREFIX (e.g. C:\Qt\6.12.1\msvc2022_64)
setlocal
set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=msvc-debug"
set "STEP=%~2"
if "%STEP%"=="" set "STEP=all"

if defined VSINSTALL goto :havevs
pushd "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
set "PATH=%CD%;%PATH%"
for /f "usebackq delims=" %%i in (`vswhere.exe -latest -version "[17.0,18.0)" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
popd
if not defined VSINSTALL echo error: no Visual Studio 2022 with the C++ x64 tools found. Set VSINSTALL.& exit /b 1
:havevs
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

set "EXTRA="
if defined QT_PREFIX set "EXTRA=-DCMAKE_PREFIX_PATH=%QT_PREFIX%"

cd /d "%~dp0.."
if /i "%STEP%"=="configure" goto :configure
if /i "%STEP%"=="build" goto :build
if /i "%STEP%"=="test" goto :test

:configure
cmake --preset %PRESET% %EXTRA% || exit /b 1
if /i "%STEP%"=="configure" exit /b 0
:build
cmake --build --preset %PRESET% || exit /b 1
if /i "%STEP%"=="build" exit /b 0
:test
ctest --preset %PRESET%
exit /b %ERRORLEVEL%
