#!/usr/bin/env bash

make -C ${PWD}/depends v=1 NO_PROTON=1 HOST=x86_64-apple-darwin -j$(sysctl -n hw.ncpu)
./autogen.sh
# -Wno-deprecated-builtins -Wno-enum-constexpr-conversion
CXXFLAGS="-g0 -O2 -Wno-unknown-warning-option" \
CONFIG_SITE="$PWD/depends/x86_64-apple-darwin/share/config.site" ./configure --disable-tests --disable-bench --with-gui=qt5 --disable-bip70

WD=$PWD
# Build RandomX
cd src/crypto/randomx
if [ -d "build" ]
then
    ls -la build/librandomx*
else
    mkdir build && cd build
    CC="${CC} -g " CXX="${CXX} -g " cmake ..
    make
fi

cd $WD

make -j$(sysctl -n hw.ncpu) # V=1
