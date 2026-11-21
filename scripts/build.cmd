@echo off
rem Configure, build and test one preset from a plain shell (no Developer
rem prompt needed). Usage: scripts\build.cmd [preset] [configure|build|test|all] [2022|2026|latest]
rem   preset   msvc-debug (default) | msvc-release | msvc-asan
rem   toolset  Visual Studio generation whose C++ tools to use. Default 2022
rem            (the generation Qt's msvc2022_64 kit is built and supported with).
rem            2026 builds against the same kit through MSVC's v14x binary
rem            compatibility. latest = newest of the two.
rem Env overrides:
rem   VSVERSION   same as the third argument (2022 | 2026 | latest)
rem   VSINSTALL   exact VS install dir; wins over VSVERSION
rem   QT_PREFIX   Qt kit, e.g. C:\Qt\6.12.1\msvc2022_64
rem A 2026 build gets its own build tree (build\<preset>-vs2026) so the two
rem compilers never share a CMake cache.
setlocal
set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=msvc-debug"
set "STEP=%~2"
if "%STEP%"=="" set "STEP=all"
if not "%~3"=="" set "VSVERSION=%~3"
if "%VSVERSION%"=="" set "VSVERSION=2022"

if defined VSINSTALL goto :havevs
set "VSRANGE="
if "%VSVERSION%"=="2022" set "VSRANGE=[17.0,18.0)"
if "%VSVERSION%"=="2026" set "VSRANGE=[18.0,19.0)"
if /i "%VSVERSION%"=="latest" set "VSRANGE=[17.0,19.0)"
if not defined VSRANGE echo error: VSVERSION must be 2022, 2026 or latest, got '%VSVERSION%'.& exit /b 1
pushd "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
set "PATH=%CD%;%PATH%"
for /f "usebackq delims=" %%i in (`vswhere.exe -latest -version "%VSRANGE%" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
popd
if not defined VSINSTALL echo error: no Visual Studio %VSVERSION% with the C++ x64 tools found. Set VSINSTALL.& exit /b 1
:havevs
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

rem Separate build tree per VS generation (consumed by CMakePresets.json).
set "PYRUN_BUILD_TAG="
if "%VisualStudioVersion%"=="18.0" set "PYRUN_BUILD_TAG=-vs2026"
echo [build.cmd] toolset: VS %VisualStudioVersion% at %VSINSTALL%, preset %PRESET%%PYRUN_BUILD_TAG%

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
