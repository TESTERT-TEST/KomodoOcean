#!/usr/bin/env bash

HOST=x86_64-apple-darwin
make -C depends v=1 NO_PROTON=1 HOST=$HOST -j$(nproc)

# ------------------------------------------------------------
# Кросс‑сборка RandomX с использованием toolchain из depends
TOOLCHAIN_BIN="$PWD/depends/$HOST/native/bin"
export CC="$TOOLCHAIN_BIN/x86_64-apple-darwin-clang"
export CXX="$TOOLCHAIN_BIN/x86_64-apple-darwin-clang++"
export CFLAGS="-g"   # можно убрать или изменить
export CXXFLAGS="-g"

RANDOMX_DIR="src/crypto/randomx"
RANDOMX_BUILD_DIR="$RANDOMX_DIR/build"
RANDOMX_LIB="$RANDOMX_BUILD_DIR/librandomx.a"

cd "$RANDOMX_DIR"
if [ -f "$RANDOMX_LIB" ]; then
    echo "RandomX already built"
else
    rm -rf build
    mkdir -p build && cd build
    cmake \
        -DCMAKE_SYSTEM_NAME=Darwin \
        -DCMAKE_C_COMPILER="$CC" \
        -DCMAKE_CXX_COMPILER="$CXX" \
        -DCMAKE_C_FLAGS="$CFLAGS" \
        -DCMAKE_CXX_FLAGS="$CXXFLAGS" \
        -DARCH=native \
        ..
    make
    cd ..
fi
cd "$OLDPWD"


./autogen.sh
CONFIG_SITE="$PWD/depends/$HOST/share/config.site" \
    CXXFLAGS="-g0 -O2 -Wno-unknown-warning-option $CXXFLAGS" \
    ./configure --disable-tests --disable-bench --with-gui=qt5 --disable-bip70

make -j$(nproc)