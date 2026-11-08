@echo off
rem TwinFET - one-click Windows build: VST3 (single file), standalone app and, if Inno Setup 6 is
rem installed, the installer. Needs the free Visual Studio Community with "Desktop development with C++".
setlocal EnableExtensions EnableDelayedExpansion
title TwinFET - build
cd /d "%~dp0"
echo.
echo  TwinFET: building the VST3 plug-in, the standalone app and the installer
echo  -----------------------------------------------------------------------
echo.

rem ---- 1) Visual Studio with the C++ tools, and the CMake that ships with it
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto novs
set "VSDIR="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto novs
set "CMAKE=!VSDIR!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "!CMAKE!" (
    set "CMAKE="
    for /f "tokens=*" %%c in ('where cmake 2^>nul') do if not defined CMAKE set "CMAKE=%%c"
)
if not defined CMAKE goto nocmake
echo  Visual Studio: !VSDIR!
echo  CMake:         !CMAKE!
echo.

rem ---- 2) JUCE 8.0.15, downloaded once into the _juce folder (no git needed)
set "JUCEVER=8.0.15"
set "JUCEDIR=%CD%\_juce\JUCE-%JUCEVER%"
if exist "!JUCEDIR!\CMakeLists.txt" goto havejuce
echo  Downloading JUCE %JUCEVER% - about 100 MB, only the first time ...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; $ProgressPreference='SilentlyContinue'; [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; New-Item -ItemType Directory -Force -Path '_juce' | Out-Null; Invoke-WebRequest -UseBasicParsing -Uri 'https://github.com/juce-framework/JUCE/archive/refs/tags/%JUCEVER%.zip' -OutFile '_juce\juce.zip'; Expand-Archive -Force -Path '_juce\juce.zip' -DestinationPath '_juce'; Remove-Item '_juce\juce.zip'"
if not exist "!JUCEDIR!\CMakeLists.txt" goto nojuce
:havejuce
set "JUCECMAKE=!JUCEDIR:\=/!"

rem ---- 3) configure and build: Release, 64-bit
"!CMAKE!" -S . -B build -A x64 -DJUCE_PATH="!JUCECMAKE!"
if errorlevel 1 goto fail
"!CMAKE!" --build build --config Release --parallel --target TwinFET_All
if errorlevel 1 goto fail

rem ---- 4) collect the results in the dist folder
set "ART=build\TwinFET_artefacts\Release"
if not exist "dist\Standalone" mkdir "dist\Standalone"
copy /y "%ART%\VST3\TwinFET.vst3\Contents\x86_64-win\TwinFET.vst3" "dist\TwinFET.vst3" >nul
if errorlevel 1 goto fail
copy /y "%ART%\Standalone\TwinFET.exe" "dist\Standalone\TwinFET.exe" >nul

rem ---- 5) the installer, if Inno Setup 6 is installed (https://jrsoftware.org/isdl.php)
set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "!ISCC!" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if exist "!ISCC!" (
    "!ISCC!" /Q "/DArtefacts=!CD!\%ART%" "/DOutDir=!CD!\dist" installer.iss
    if errorlevel 1 goto fail
) else (
    echo  Inno Setup 6 is not installed - skipping the installer, the single-file plug-in is ready anyway.
)

echo.
echo  Done. Everything is in the dist folder:
echo     dist\TwinFET.vst3               the plug-in, a single file
echo     dist\Standalone\TwinFET.exe     works without a DAW
if exist "dist\TwinFET-Setup-Windows.exe" echo     dist\TwinFET-Setup-Windows.exe  the installer
echo.

rem ---- 6) offer to put the plug-in where every DAW looks (asks for administrator rights)
if defined CommonProgramW6432 (set "VST3DIR=%CommonProgramW6432%\VST3") else (set "VST3DIR=%CommonProgramFiles%\VST3")
choice /c YN /m " Install TwinFET.vst3 into !VST3DIR! now"
if errorlevel 2 goto end
> "%TEMP%\twinfet_install.cmd" (
    echo @echo off
    echo if not exist "!VST3DIR!" mkdir "!VST3DIR!"
    echo if exist "!VST3DIR!\TwinFET.vst3\" rmdir /s /q "!VST3DIR!\TwinFET.vst3"
    echo copy /y "!CD!\dist\TwinFET.vst3" "!VST3DIR!\TwinFET.vst3"
)
powershell -NoProfile -Command "Start-Process -FilePath (Join-Path $env:TEMP 'twinfet_install.cmd') -Verb RunAs -Wait"
if exist "!VST3DIR!\TwinFET.vst3" (echo  Installed. Close and reopen your DAW, then rescan the plug-ins.) else (echo  Not installed - copy dist\TwinFET.vst3 there by hand.)
goto end

:novs
echo.
echo  Visual Studio with the C++ tools was not found.
echo  Install the free Visual Studio Community and tick "Desktop development with C++":
echo     https://visualstudio.microsoft.com/downloads/
echo  then run this file again.
goto end

:nocmake
echo.
echo  CMake was not found. Open the Visual Studio Installer, press Modify and make sure that
echo  "C++ CMake tools for Windows" is ticked, then run this file again.
goto end

:nojuce
echo.
echo  Could not download JUCE. Check the internet connection and run this file again.
goto end

:fail
echo.
echo  The build stopped with an error - scroll up to the first error message.
echo  If it talks about a path that is too long, move the TwinFET folder to C:\TwinFET and try again.

:end
echo.
pause
endlocal
