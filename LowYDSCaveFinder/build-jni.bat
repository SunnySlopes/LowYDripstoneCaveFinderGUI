@echo off
setlocal
set MASTER=%~dp0
if "%JAVA_HOME%"=="" (
  for /f "delims=" %%i in ('where java 2^>nul') do set "JAVA_HOME=%%~dpi..\.."
)
set PATH=C:\msys64\ucrt64\bin;%PATH%
set OUT=%MASTER%native\jni\build\native
set RF=%MASTER%..\native
mkdir "%OUT%" 2>nul
mkdir "%RF%\windows" 2>nul

cd /d "%MASTER%"
set INC=%JAVA_HOME%\include
set INCW=%JAVA_HOME%\include\win32
set DEF=-DNDEBUG -D_WIN32 -DRIVER_FINDER_JNI_LIB -I. -Icubiomes -I"%INC%" -I"%INCW%"

del /q *.o bridge.o 2>nul
gcc -std=c17 -O3 %DEF% -c cubiomes/finders.c cubiomes/generator.c cubiomes/layers.c cubiomes/biomenoise.c cubiomes/biomes.c cubiomes/noise.c cubiomes/util.c cubiomes/quadbase.c
g++ -std=c++20 -O3 %DEF% -c BiomeSampler.cpp
g++ -std=c++20 -O3 %DEF% -c native/jni/sunnyslopes_lowydscavefinder_LowYDripstoneCaveFinderBridge.cpp -o bridge.o
g++ -shared -O3 -o "%OUT%\libDripstoneCaveFinderLibJ.dll" finders.o generator.o layers.o biomenoise.o biomes.o noise.o util.o quadbase.o BiomeSampler.o bridge.o -static -static-libgcc -static-libstdc++ -pthread
if errorlevel 1 exit /b 1

copy /y "%OUT%\libDripstoneCaveFinderLibJ.dll" "%RF%\windows\libDripstoneCaveFinderLibJ.dll"
echo Built: %OUT%\libDripstoneCaveFinderLibJ.dll
echo Copied to: %RF%\windows\libDripstoneCaveFinderLibJ.dll
