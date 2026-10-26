#!/bin/sh
# Compila no Linux/macOS com mingw-w64 (Debian/Ubuntu: apt install gcc-mingw-w64-x86-64).
# Saída em build/:
#   EyeSight-Instalador/     instalador (assina localmente a versão uiAccess)
#   EyeSight-Desinstalador/  só o desinstalador
set -e
cd "$(dirname "$0")"
CC=${CC:-x86_64-w64-mingw32-gcc}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
CFLAGS="-O2 -Wall -municode -mwindows -static -s"
LIBS="-luser32 -lgdi32 -lshell32 -ldwmapi -ladvapi32"
rm -rf build/EyeSight-Instalador build/EyeSight-Desinstalador
mkdir -p build/EyeSight-Instalador build/EyeSight-Desinstalador
(cd src && $WINDRES app-uiaccess.rc -O coff -o ../build/app-uiaccess.res)
$CC $CFLAGS src/main.c build/app-uiaccess.res -o build/EyeSight-Instalador/EyeSight-uiaccess.exe $LIBS
cp installer/* build/EyeSight-Instalador/
cp installer/Desinstalar.cmd installer/uninstall.ps1 build/EyeSight-Desinstalador/
rm -f build/*.res
echo "Pronto: build/EyeSight-Instalador/ e build/EyeSight-Desinstalador/"
