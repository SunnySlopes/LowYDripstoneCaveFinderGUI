@echo off
setlocal EnableExtensions
set MASTER=%~dp0
if "%JAVA_HOME%"=="" (
  for /f "delims=" %%i in ('where java 2^>nul') do set "JAVA_HOME=%%~dpi..\.."
)
set PATH=C:\msys64\ucrt64\bin;%PATH%
set CUB=%MASTER%cubiomes
set OUT=%MASTER%native\jni\build\native
set RF=%MASTER%..\native

if not exist "%CUB%\noise.c" (
  echo ERROR: cubiomes not found at:
  echo   %CUB%
  echo Init the submodule: git submodule update --init jni/cubiomes
  exit /b 1
)

mkdir "%OUT%" 2>nul
mkdir "%RF%\windows" 2>nul
cd /d "%MASTER%"

set INC=%JAVA_HOME%\include
set INCW=%JAVA_HOME%\include\win32
rem -I. so #include "cubiomes/..." resolves to jni/cubiomes
set CDEF=-DNDEBUG -D_WIN32 -DDRIPSTONECAVE_FINDER_JNI_LIB -I. -I"%INC%" -I"%INCW%"
rem pre_cubiomes.h avoids C++20 std::lerp clash without copying cubiomes sources
set CXXDEF=%CDEF% -include cxx_compat/pre_cubiomes.h

del /q *.o bridge.o 2>nul
rem Core biome/noise only (no finders/quadbase/loot — unused by cave search)
gcc -std=c17 -O3 %CDEF% -c "%CUB%\generator.c" "%CUB%\layers.c" "%CUB%\biomenoise.c" "%CUB%\biomes.c" "%CUB%\noise.c" "%CUB%\util.c"
if errorlevel 1 exit /b 1
g++ -std=c++20 -O3 %CXXDEF% -c BiomeSampler.cpp
if errorlevel 1 exit /b 1
g++ -std=c++20 -O3 %CXXDEF% -c OctaveFieldCache.cpp
if errorlevel 1 exit /b 1
g++ -std=c++20 -O3 %CXXDEF% -c native/jni/sunnyslopes_lowydscavefinder_LowYDripstoneCaveFinderBridge.cpp -o bridge.o
if errorlevel 1 exit /b 1
g++ -shared -O3 -o "%OUT%\libDripstoneCaveFinderLibJ.dll" generator.o layers.o biomenoise.o biomes.o noise.o util.o BiomeSampler.o OctaveFieldCache.o bridge.o -static -static-libgcc -static-libstdc++ -pthread
if errorlevel 1 exit /b 1

copy /y "%OUT%\libDripstoneCaveFinderLibJ.dll" "%RF%\windows\libDripstoneCaveFinderLibJ.dll"
echo Built: %OUT%\libDripstoneCaveFinderLibJ.dll
echo Copied to: %RF%\windows\libDripstoneCaveFinderLibJ.dll
echo Cubiomes: %CUB%
