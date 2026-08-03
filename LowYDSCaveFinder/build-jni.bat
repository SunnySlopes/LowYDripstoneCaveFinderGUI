@echo off
setlocal
set MASTER=%~dp0
if "%JAVA_HOME%"=="" (
  for /f "delims=" %%i in ('where java 2^>nul') do set "JAVA_HOME=%%~dpi..\.."
)
set PATH=C:\msys64\ucrt64\bin;%PATH%
set OUT=%MASTER%native\jni\build\native
set RF=%MASTER%..\native
rem Cubitect cubiomes from sibling repo (lerp_x). Do NOT patch the xpple submodule.
set CUB=%MASTER%..\..\LowYDripstoneCaveFinder\LowYDSCaveFinder\cubiomes
if not exist "%CUB%\noise.c" (
  echo ERROR: sibling cubiomes not found at:
  echo   %CUB%
  echo Clone/build LowYDripstoneCaveFinder next to this repo, or set CUB to a Cubitect tree with lerp_x.
  exit /b 1
)
mkdir "%OUT%" 2>nul
mkdir "%RF%\windows" 2>nul

cd /d "%MASTER%"
set INC=%JAVA_HOME%\include
set INCW=%JAVA_HOME%\include\win32
rem -I points at parent of the cubiomes/ folder so #include "cubiomes/..." works
set DEF=-DNDEBUG -D_WIN32 -DRIVER_FINDER_JNI_LIB -I. -I"%CUB%\.." -I"%INC%" -I"%INCW%"

del /q *.o bridge.o 2>nul
gcc -std=c17 -O3 %DEF% -c "%CUB%\finders.c" "%CUB%\generator.c" "%CUB%\layers.c" "%CUB%\biomenoise.c" "%CUB%\biomes.c" "%CUB%\noise.c" "%CUB%\util.c" "%CUB%\quadbase.c"
if errorlevel 1 exit /b 1
g++ -std=c++20 -O3 %DEF% -c BiomeSampler.cpp
if errorlevel 1 exit /b 1
g++ -std=c++20 -O3 %DEF% -c native/jni/sunnyslopes_lowydscavefinder_LowYDripstoneCaveFinderBridge.cpp -o bridge.o
if errorlevel 1 exit /b 1
g++ -shared -O3 -o "%OUT%\libDripstoneCaveFinderLibJ.dll" finders.o generator.o layers.o biomenoise.o biomes.o noise.o util.o quadbase.o BiomeSampler.o bridge.o -static -static-libgcc -static-libstdc++ -pthread
if errorlevel 1 exit /b 1

copy /y "%OUT%\libDripstoneCaveFinderLibJ.dll" "%RF%\windows\libDripstoneCaveFinderLibJ.dll"
echo Built: %OUT%\libDripstoneCaveFinderLibJ.dll
echo Copied to: %RF%\windows\libDripstoneCaveFinderLibJ.dll
echo Cubiomes: %CUB%
