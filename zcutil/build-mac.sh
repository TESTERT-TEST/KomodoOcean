#!/usr/bin/env bash

# seems clang on native darwin doesn't support -fopenmp compiler flag (MULTICORE=1 for libsnark)
sed -i.old -e 's|\(CURVE=ALT_BN128[ \t]*MULTICORE=\)\([0-9]\{1,\}\)|\10|' ./depends/packages/libsnark.mk
make -C ${PWD}/depends v=1 NO_PROTON=1 HOST=x86_64-apple-darwin -j$(nproc --all)
./autogen.sh
# -Wno-deprecated-builtins -Wno-enum-constexpr-conversion
CXXFLAGS="-g0 -O2 -Wno-unknown-warning-option" \
CONFIG_SITE="$PWD/depends/x86_64-apple-darwin/share/config.site" ./configure --disable-tests --disable-bench --with-gui=qt5 --disable-bip70
make -j$(nproc --all) # V=1
