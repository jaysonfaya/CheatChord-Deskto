#!/usr/bin/env bash
# CheatChord host tests: data checks, the engine test (ASan/UBSan) and the two CLI demos. MV = mpc-vst-plugins.
set -euo pipefail
cd "$(dirname "$0")/.."
MV=${MV:-$HOME/ext/mpc-vst-plugins}
python3 tools/gen_data.py
SRC="src/chordpilot.c src/harmony.c src/theory.c src/voicing.c src/synth.c src/seq_out.c src/library.c src/artists.c src/refs.c"
gcc -O1 -g -fsanitize=address,undefined -std=gnu11 -Isrc -I"$MV/wrapper" -Ivst/build $SRC tests/engine_test.c -lm -ldl -lpthread -o /tmp/cc_engine_test
gcc -O1 -std=gnu11 -Isrc src/harmony.c src/theory.c src/artists.c src/refs.c tests/harmony_cli.c -lm -o /tmp/cc_harmony_cli
gcc -O1 -std=gnu11 -Isrc src/harmony.c src/theory.c src/artists.c src/refs.c tests/artist_cli.c -lm -o /tmp/cc_artist_cli
/tmp/cc_engine_test
rm -rf "/tmp/CheatChord Data"   # the user preset the test saved (no MPC Documents folder on a PC)
