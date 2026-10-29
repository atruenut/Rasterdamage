@echo off
REM ============================================================================
REM  Raster Damage - one-click Windows build
REM
REM  Produces:  dist\RasterDamage.vst3\Contents\x86_64-win\RasterDamage.vst3
REM  (that is a normal 64-bit Windows DLL with the .vst3 extension)
REM
REM  Needs ONE of:
REM    - Visual Studio 2019/2022 (Build Tools are enough, "Desktop C++" workload), or
REM    - MinGW-w64 (g++ on PATH, e.g. from MSYS2 / winlibs)
REM  plus CMake and Git on PATH.
REM
REM  Usage:  build.bat            build + test
REM          build.bat install    build, test, copy to C:\Program Files\Common Files\VST3
REM          build.bat clean      remove build folders
REM ============================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

if /i "%1"=="clean" (
  rmdir /s /q build_vs 2>nul
  rmdir /s /q build_mingw 2>nul
  rmdir /s /q dist 2>nul
  echo cleaned.
  exit /b 0
)

where cmake >nul 2>nul || (echo [error] CMake not found on PATH. Install from https://cmake.org/download/ & exit /b 1)

REM ---- 1. VST3 SDK ----------------------------------------------------------------
if not exist third_party\vst3sdk\pluginterfaces\base\funknown.h (
  where git >nul 2>nul || (echo [error] Git not found on PATH. Install from https://git-scm.com/ & exit /b 1)
  echo Fetching the VST3 SDK ^(MIT licensed^)...
  git clone --depth 1 --recursive --shallow-submodules https://github.com/steinbergmedia/vst3sdk.git third_party\vst3sdk
  if errorlevel 1 (echo [error] could not fetch the SDK & exit /b 1)
)

REM ---- 2. pick a compiler -----------------------------------------------------------
set BUILD=
set TESTEXE=
where cl >nul 2>nul
if not errorlevel 1 goto :use_msvc_env

REM try to find Visual Studio automatically
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
  if defined VSPATH (
    call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul
    goto :use_msvc_env
  )
)
where g++ >nul 2>nul
if not errorlevel 1 goto :use_mingw
echo [error] No C++ compiler found. Install Visual Studio Build Tools ^(Desktop C++^) or MinGW-w64.
exit /b 1

:use_msvc_env
echo Using Visual Studio / MSVC
set BUILD=build_vs
cmake -S . -B %BUILD% -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1
cmake --build %BUILD%
if errorlevel 1 exit /b 1
goto :built

:use_mingw
echo Using MinGW-w64
set BUILD=build_mingw
cmake -S . -B %BUILD% -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1
cmake --build %BUILD%
if errorlevel 1 exit /b 1

:built
REM ---- 3. self-test the DSP engine ----------------------------------------------------
if exist %BUILD%\rd_test.exe (
  echo.
  echo Running DSP self-test...
  %BUILD%\rd_test.exe
  if errorlevel 1 (echo [error] self-test failed & exit /b 1)
)

REM ---- 4. assemble a proper VST3 bundle ---------------------------------------------------
set OUT=dist\RasterDamage.vst3\Contents\x86_64-win
rmdir /s /q dist 2>nul
mkdir %OUT%
copy /y %BUILD%\dist\RasterDamage.vst3 %OUT%\RasterDamage.vst3 >nul
if errorlevel 1 (echo [error] plugin was not produced & exit /b 1)
copy /y %BUILD%\dist\RasterDamage.vst3 dist\RasterDamage.vst3.dll >nul
if exist %BUILD%\rd_render.exe copy /y %BUILD%\rd_render.exe dist\rd_render.exe >nul

echo.
echo ======================================================================
echo  Built:  %CD%\dist\RasterDamage.vst3   ^(bundle - the recommended form^)
echo          %CD%\dist\RasterDamage.vst3.dll   ^(same DLL, plain file^)
echo ======================================================================

if /i "%1"=="install" (
  set "DEST=%CommonProgramFiles%\VST3"
  echo Installing to !DEST! ^(may need an Administrator prompt^)...
  xcopy /e /i /y dist\RasterDamage.vst3 "!DEST!\RasterDamage.vst3" >nul
  if errorlevel 1 (echo [warn] copy failed - run this from an Administrator command prompt, or copy dist\RasterDamage.vst3 by hand.) else (echo Installed. Rescan plugins in your DAW.)
) else (
  echo To install: copy the dist\RasterDamage.vst3 FOLDER to  C:\Program Files\Common Files\VST3\
  echo          or run:  build.bat install   ^(from an Administrator prompt^)
)
endlocal
