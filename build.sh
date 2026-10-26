#!/bin/sh
# Compila no Linux/macOS com mingw-w64 (Debian/Ubuntu: apt install gcc-mingw-w64-x86-64).
# Saída em build/:
#   EyeSight.exe            versão portátil (dois cliques, roda de qualquer pasta)
#   EyeSight-Instalador/    instalador que assina localmente a versão uiAccess
set -e
cd "$(dirname "$0")"
CC=${CC:-x86_64-w64-mingw32-gcc}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
CFLAGS="-O2 -Wall -municode -mwindows -static -s"
LIBS="-luser32 -lgdi32 -lshell32 -ldwmapi -ladvapi32"
mkdir -p build/EyeSight-Instalador
for v in app app-uiaccess; do
  (cd src && $WINDRES $v.rc -O coff -o ../build/$v.res)
done
$CC $CFLAGS src/main.c build/app.res -o build/EyeSight.exe $LIBS
$CC $CFLAGS src/main.c build/app-uiaccess.res -o build/EyeSight-Instalador/EyeSight-uiaccess.exe $LIBS
cp installer/* build/EyeSight-Instalador/
rm -f build/*.res
echo "Pronto: build/EyeSight.exe e build/EyeSight-Instalador/"
