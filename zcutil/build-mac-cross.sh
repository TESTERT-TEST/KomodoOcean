#!/usr/bin/env bash

# don't forget to place SDK into project folder before run this script
mkdir -p ${PWD}/depends/SDKs
tar -C ${PWD}/depends/SDKs -xf ${PWD}/Xcode-13.2.1-13C100-extracted-SDK-with-libcxx-headers.tar.gz
# make deps
make -C ${PWD}/depends v=1 NO_PROTON=1 HOST=x86_64-apple-darwin -j$(nproc --all)

# here we need bit modify config.site for darwin cross-compile case,
# to fix env command path and paths to cctools (ar, ranlib, strip, nm, etc.),
# but probably we don't need to use $(toolchain_path) variable in
# depends/Makefile for $(host_prefix)/share/config.site target.

CONFIG_SITE="$PWD/depends/x86_64-apple-darwin/share/config.site"
FIX_PATH=${PWD}/depends/x86_64-apple-darwin/native/bin/
if test -n "${FIX_PATH-}"; then
	sed -i.old 's|'${FIX_PATH}'env|'$(command -v env)'|g' ${CONFIG_SITE} && \
	sed -i.old 's|'${FIX_PATH}'/|/|g' ${CONFIG_SITE}
fi

./autogen.sh
LDFLAGS="-Wl,-no_pie" \
CXXFLAGS="-g0 -O2" \
CONFIG_SITE="$PWD/depends/x86_64-apple-darwin/share/config.site" ./configure --disable-tests --disable-bench --with-gui=qt5 --disable-bip70

WD=$PWD

RANDOMX_DIR="src/crypto/randomx"
RANDOMX_BUILD_DIR="$RANDOMX_DIR/build"
RANDOMX_LIB="$RANDOMX_BUILD_DIR/librandomx.a"

cd "$RANDOMX_DIR"
if [ -f "$RANDOMX_LIB" ]; then
    echo "RandomX already built: $RANDOMX_LIB"
else
    rm -rf build
    mkdir -p build && cd build
    # Явно задаём компиляторы и целевую систему (для кросс‑компиляции с Linux)
    CC="${CC}" CXX="${CXX}" cmake \
        -DCMAKE_SYSTEM_NAME=Darwin \
        -DCMAKE_C_COMPILER="${CC}" \
        -DCMAKE_CXX_COMPILER="${CXX}" \
        -DCMAKE_C_FLAGS="-g" \
        -DCMAKE_CXX_FLAGS="-g" \
        -DARCH=native \
        ..
    make
    cd ..
fi
cd $WD

make -j$(nproc --all) # V=1
