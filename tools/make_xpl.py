#!/usr/bin/env python3
"""CheatChord: one .xpl per factory preset in the skin's Presets folder, so MPC's Instruments browser (Sounds >
INSTRUMENTS > CheatChord) lists them next to the Default. Each holds the engine state of the loaded preset (the
wrapper's chunk is the engine's "state" text). Run after gen_vst.py:  python3 tools/make_xpl.py"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SDK = os.environ.get("MV", "/home/claude/ext/mpc-vst-plugins")
sys.path.insert(0, os.path.join(SDK, "tools"))
import xpl  # noqa: E402

SKIN = os.path.join(ROOT, "vst", "build", "skin", "JJ DA PLUG - VST - CheatChord")
SRC = ["src/chordpilot.c", "src/harmony.c", "src/theory.c", "src/voicing.c", "src/synth.c", "src/seq_out.c", "src/library.c",
       "src/artists.c", "src/refs.c", "tests/dump_presets.c"]
exe = "/tmp/cc_dump_presets"
subprocess.check_call(["gcc", "-O1", "-std=gnu11", "-Isrc", "-I" + os.path.join(SDK, "wrapper"), "-Ivst/build"] + SRC +
                      ["-lm", "-ldl", "-lpthread", "-o", exe], cwd=ROOT)
out = subprocess.check_output([exe], cwd="/tmp").decode()
dst = os.path.join(SKIN, "Presets")
os.makedirs(dst, exist_ok=True)
uid = int.from_bytes(b"ChtC", "big")
so = "%%payload-path%%/JJ DA PLUG - VST - CheatChord/cheatchord.so"
n = 0
for line in out.splitlines():
    i, name, state = line.split("\t", 2)
    path = os.path.join(dst, "%04d-%s.xpl" % (int(i) + 1, name.replace("/", "-")))
    with open(path, "w", encoding="utf-8") as f:
        f.write(xpl.xpl("CheatChord", "JJ DA PLUG", so, uid, 4100, state.encode() + b"\0", preset=name))
    n += 1
print("make_xpl: %d factory presets in %s" % (n, dst))
