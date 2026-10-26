@echo off
rem Compila no Windows. Requer MSYS2 com MinGW-w64 (gcc e windres no PATH).
cd /d "%~dp0"
if not exist build\EyeSight-Instalador mkdir build\EyeSight-Instalador
pushd src
windres app.rc -O coff -o ..\build\app.res || exit /b 1
windres app-uiaccess.rc -O coff -o ..\build\app-uiaccess.res || exit /b 1
popd
set FLAGS=-O2 -municode -mwindows -static -s
set LIBS=-luser32 -lgdi32 -lshell32 -ldwmapi -ladvapi32
gcc %FLAGS% src\main.c build\app.res -o build\EyeSight.exe %LIBS% || exit /b 1
gcc %FLAGS% src\main.c build\app-uiaccess.res -o build\EyeSight-Instalador\EyeSight-uiaccess.exe %LIBS% || exit /b 1
copy /y installer\* build\EyeSight-Instalador\ >nul
del build\*.res
echo Pronto: build\EyeSight.exe e build\EyeSight-Instalador\
