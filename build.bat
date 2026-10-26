@echo off
rem Compila no Windows. Requer MSYS2 com MinGW-w64 (gcc e windres no PATH).
cd /d "%~dp0"
if exist build\EyeSight-Instalador rmdir /s /q build\EyeSight-Instalador
if exist build\EyeSight-Desinstalador rmdir /s /q build\EyeSight-Desinstalador
mkdir build\EyeSight-Instalador build\EyeSight-Desinstalador
pushd src
windres app-uiaccess.rc -O coff -o ..\build\app-uiaccess.res || exit /b 1
popd
set FLAGS=-O2 -municode -mwindows -static -s
set LIBS=-luser32 -lgdi32 -lshell32 -ldwmapi -ladvapi32
gcc %FLAGS% src\main.c build\app-uiaccess.res -o build\EyeSight-Instalador\EyeSight-uiaccess.exe %LIBS% || exit /b 1
copy /y installer\* build\EyeSight-Instalador\ >nul
copy /y installer\Desinstalar.cmd build\EyeSight-Desinstalador\ >nul
copy /y installer\uninstall.ps1 build\EyeSight-Desinstalador\ >nul
del build\*.res
echo Pronto: build\EyeSight-Instalador\ e build\EyeSight-Desinstalador\
