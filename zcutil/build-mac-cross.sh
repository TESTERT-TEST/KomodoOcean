#!/usr/bin/env bash

# don't forget to place SDK into project folder before run this script
mkdir -p ${PWD}/depends/SDKs
tar -C ${PWD}/depends/SDKs -xf ${PWD}/Xcode-13.2.1-13C100-extracted-SDK-with-libcxx-headers.tar.gz

# Сборка depends
make -C ${PWD}/depends v=1 NO_PROTON=1 HOST=x86_64-apple-darwin -j$(nproc --all)

# Корректировка config.site (как у вас)
CONFIG_SITE="$PWD/depends/x86_64-apple-darwin/share/config.site"
FIX_PATH=${PWD}/depends/x86_64-apple-darwin/native/bin/
if test -n "${FIX_PATH-}"; then
    sed -i.old 's|'${FIX_PATH}'env|'$(command -v env)'|g' ${CONFIG_SITE} && \
	sed -i.old 's|'${FIX_PATH}'/|/|g' ${CONFIG_SITE}
fi

# ------------------------------------------------------------
# Кросс-сборка RandomX для macOS с использованием тулчейна depends
RANDOMX_DIR="src/crypto/randomx"
RANDOMX_BUILD_DIR="$RANDOMX_DIR/build"
RANDOMX_LIB="$RANDOMX_BUILD_DIR/librandomx.a"

# Определяем компиляторы из toolchain depends
TOOLCHAIN_BIN="$PWD/depends/x86_64-apple-darwin/native/bin"
export CC="$TOOLCHAIN_BIN/x86_64-apple-darwin-clang"
export CXX="$TOOLCHAIN_BIN/x86_64-apple-darwin-clang++"

cd "$RANDOMX_DIR"
if [ -f "$RANDOMX_LIB" ]; then
    echo "RandomX already built: $RANDOMX_LIB"
else
    rm -rf build
    mkdir -p build && cd build
    cmake \
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
cd "$OLDPWD"

./autogen.sh
LDFLAGS="-Wl,-no_pie" \
CXXFLAGS="-g0 -O2" \
CONFIG_SITE="$PWD/depends/x86_64-apple-darwin/share/config.site" ./configure --disable-tests --disable-bench --with-gui=qt5 --disable-bip70

# Финальная сборка
make -j$(nproc --all)
