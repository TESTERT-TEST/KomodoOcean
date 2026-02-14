#!/usr/bin/env bash

HOST=x86_64-w64-mingw32
CXX=x86_64-w64-mingw32-g++-posix
CC=x86_64-w64-mingw32-gcc-posix
PREFIX="$(pwd)/depends/$HOST"

set -eu -o pipefail
set -x

cd "$(dirname "$(readlink -f "$0")")/.."


make "$@" -C depends V=1 HOST=$HOST

RANDOMX_DIR="src/crypto/randomx"
RANDOMX_BUILD_DIR="$RANDOMX_DIR/build"
RANDOMX_LIB="$RANDOMX_BUILD_DIR/librandomx.a"

cd "$RANDOMX_DIR"
if [ -f "$RANDOMX_LIB" ]; then
    echo "RandomX: $RANDOMX_LIB"
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
cd "$OLDPWD"

export LDFLAGS="$LDFLAGS -L$PWD/$RANDOMX_BUILD_DIR"
export LIBS="$LIBS -lrandomx"

ls -la "$PWD/$RANDOMX_LIB"

./autogen.sh
CONFIG_SITE="$PWD/depends/$HOST/share/config.site" \
    CXXFLAGS="-DCURL_STATICLIB -g0 -O2" \
    ./configure --disable-tests --disable-bench --with-gui=qt5 --disable-bip70

make "$@"