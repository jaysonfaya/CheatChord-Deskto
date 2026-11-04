#!/usr/bin/env bash
# Build CheatChord for MPC OS Gen1 (MPC Live / Live II / One / X / Key, Force): armhf, hard float.
# Uses any arm-linux-gnueabihf gcc (CC=...). The binary only needs very old glibc symbols, so it loads on
# MPC OS 2.x and 3.x alike. Run tools/make_params.py and gen_vst.py (skin, params.h) first.
set -euo pipefail
cd "$(dirname "$0")/.."
MV=${MV:-$HOME/ext/mpc-vst-plugins}
CC=${CC:-arm-linux-gnueabihf-gcc}
STRIP=${STRIP:-${CC%gcc}strip}
SOURCES="src/chordpilot.c src/harmony.c src/theory.c src/voicing.c src/synth.c src/seq_out.c src/library.c src/artists.c src/refs.c"
mkdir -p vst/build
"$CC" -O2 -Wall -Wextra -Wno-unused-parameter -fPIC -shared -fvisibility=hidden -std=gnu11 \
    -march=armv7-a -mfpu=neon -mfloat-abi=hard \
    -Isrc -Ivst/build -I"$MV/wrapper" $SOURCES src/wrapper/vst2_wrap.c \
    -static-libgcc -lm -ldl -lpthread -Wl,--no-undefined -o vst/build/cheatchord.so
"$STRIP" vst/build/cheatchord.so
echo "exported: $(readelf --dyn-syms -W vst/build/cheatchord.so | grep -E ' GLOBAL .* [0-9]+ [A-Za-z]' | grep -v UND | awk '{print $8}' | tr '\n' ' ')"
echo "highest glibc: $(readelf -V vst/build/cheatchord.so | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1)"
echo "needs: $(readelf -d vst/build/cheatchord.so | grep NEEDED | sed 's/.*\[\(.*\)\]/\1/' | tr '\n' ' ')"
