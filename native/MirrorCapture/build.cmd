@echo off
rem Build MirrorCapture.dll (OpenVR mirror texture -> 32-bit BGRA BMP readback).
rem Output: unity-client/Assets/Jarvis/Plugins/x86_64/MirrorCapture.dll
rem Requires Visual Studio (MSVC x64 C++ tools).
setlocal

set SRC=%~dp0MirrorCapture.cpp
set OUT=%~dp0..\..\unity-client\Assets\Aoi\Plugins\x86_64\MirrorCapture.dll

for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set VSROOT=%%i
if not defined VSROOT (
    echo ERROR: Visual Studio with MSVC C++ tools not found.
    exit /b 1
)
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo ERROR: vcvars64.bat failed.
    exit /b 1
)

cl /nologo /O2 /MT /LD "%SRC%" /Fe:"%OUT%" /link d3d11.lib
if errorlevel 1 (
    echo ERROR: compile failed.
    exit /b 1
)
echo Built: %OUT%
