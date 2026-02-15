#!/usr/bin/env bash

# Задаём хост для macOS
HOST=x86_64-apple-darwin

# Распаковываем SDK (если ещё не распакован)
mkdir -p "${PWD}/depends/SDKs"
tar -C "${PWD}/depends/SDKs" -xf "${PWD}/Xcode-13.2.1-13C100-extracted-SDK-with-libcxx-headers.tar.gz"

# Сборка depends (тулчейн и библиотеки)
make -C "${PWD}/depends" v=1 NO_PROTON=1 HOST="$HOST" -j$(nproc --all)

# Корректировка config.site – заменяем пути к утилитам на системные
CONFIG_SITE="${PWD}/depends/$HOST/share/config.site"
FIX_PATH="${PWD}/depends/$HOST/native/bin/"
if test -n "${FIX_PATH-}"; then
    sed -i.old "s|${FIX_PATH}env|$(command -v env)|g" "$CONFIG_SITE"
    sed -i.old "s|${FIX_PATH}/|/|g" "$CONFIG_SITE"
fi

# ------------------------------------------------------------
# Кросс‑сборка RandomX (ДО ./configure)
TOOLCHAIN_BIN="${PWD}/depends/$HOST/native/bin"
export CC="${TOOLCHAIN_BIN}/x86_64-apple-darwin-clang"
export CXX="${TOOLCHAIN_BIN}/x86_64-apple-darwin-clang++"
export CFLAGS="-g"
export CXXFLAGS="-g"

RANDOMX_DIR="src/crypto/randomx"
RANDOMX_BUILD_DIR="${RANDOMX_DIR}/build"
RANDOMX_LIB="${RANDOMX_BUILD_DIR}/librandomx.a"

cd "${RANDOMX_DIR}"
if [ -f "${RANDOMX_LIB}" ]; then
    echo "RandomX already built"
else
    rm -rf build
    mkdir -p build && cd build
    cmake \
        -DCMAKE_SYSTEM_NAME=Darwin \
        -DCMAKE_C_COMPILER="${CC}" \
        -DCMAKE_CXX_COMPILER="${CXX}" \
        -DCMAKE_C_FLAGS="${CFLAGS}" \
        -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
        -DARCH=native \
        ..
    make
    cd ..
fi
cd "${OLDPWD}"   # возвращаемся в корень проекта

# Генерация и конфигурация основного проекта
./autogen.sh
LDFLAGS="-Wl,-no_pie" \
CXXFLAGS="-g0 -O2" \
CONFIG_SITE="$PWD/depends/$HOST/share/config.site" ./configure --disable-tests --disable-bench --with-gui=qt5 --disable-bip70

# Финальная сборка
make -j$(nproc --all)
