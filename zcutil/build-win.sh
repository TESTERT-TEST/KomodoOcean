#!/usr/bin/env bash

HOST=x86_64-w64-mingw32
CXX=x86_64-w64-mingw32-g++-posix
CC=x86_64-w64-mingw32-gcc-posix
PREFIX="$(pwd)/depends/$HOST"

set -eu -o pipefail

set -x
cd "$(dirname "$(readlink -f "$0")")/.."

make "$@" -C ${PWD}/depends V=1 HOST=x86_64-w64-mingw32
./autogen.sh
CONFIG_SITE="$PWD/depends/x86_64-w64-mingw32/share/config.site" CXXFLAGS="-DCURL_STATICLIB -g0 -O2" ./configure --disable-tests --disable-bench --with-gui=qt5 --disable-bip70

WD=$PWD

# Build RandomX
cd src/crypto/randomx
if [ -f "build/librandomx.a" ]; then
    echo "RandomX already built"
else
    rm -rf build
    mkdir -p build && cd build
    CC="${CC}" CXX="${CXX}" cmake \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_C_COMPILER="${CC}" \
        -DCMAKE_CXX_COMPILER="${CXX}" \
        -DCMAKE_C_FLAGS="-static-libgcc" \
        -DCMAKE_CXX_FLAGS="-static-libstdc++" \
        -DARCH=native \
        ..
    make
    cd ..
fi

cd $WD

make "$@" # V=1