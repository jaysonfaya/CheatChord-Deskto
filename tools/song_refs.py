#!/usr/bin/env python3
"""CheatChord desktop: the song references, taken over from MIDI CONDUCTA 3.3 (src/sf_ref.h: McGill Billboard chord
annotations, CC0, plus phrases from public-domain Mutopia scores; and the named progressions of its library LIB in
src/sf_compose.c). Writes engine/gen/songrefs_gen.inc.

Each loop becomes CheatChord slots: chord (degree above the tonic, quality, slash bass) and length. A bar with n chords
is split 1 / 1/2+1/2 / 1/2+1/4+1/4 / 4 x 1/4; equal neighbours merge into one longer chord. The chord length (1/2, 1 or
2 bars) is the largest that fits, every chord a Half, Normal or Double of it (longer ones repeat). At most 8 slots.

usage: song_refs.py <sample-forge src dir>"""
import os
import re
import sys
from fractions import Fraction as Fr

SRC = sys.argv[1]
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DEG = {"I": 0, "II": 2, "III": 4, "IV": 5, "V": 7, "VI": 9, "VII": 11}
BASSN = [0, 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21]
# CheatChord qualities (engine/theory.h order)
Q = dict(maj=0, m=1, dim=2, aug=3, sus2=4, sus4=5, six=6, m6=7, maj7=8, d7=9, m7=10, m7b5=11, dim7=12, mmaj7=13,
         sus7=14, add9=15, madd9=16, s69=17, maj9=18, d9=19, m9=20, m11=21, sus9=22, d13=23, maj7s11=24, d7b9=25,
         d7s9=26)


def quality(suf, upper):
    T = {"": "maj" if upper else "m", "7": "d7" if upper else "m7", "m7": "m7", "m": "m", "m6": "m6", "m9": "m9",
         "m11": "m11", "maj7": "maj7", "maj9": "maj9", "9": "d9" if upper else "m9", "11": "sus9" if upper else "m11",
         "13": "d13" if upper else "m9", "6": "six" if upper else "m6", "69": "s69", "h7": "m7b5", "o7": "dim7",
         "o": "dim", "+": "aug", "sus": "sus4", "7sus": "sus7", "9sus": "sus9", "add9": "add9" if upper else "madd9",
         "7#9": "d7s9", "7b9": "d7b9", "mmaj7": "mmaj7"}
    return Q[T[suf]] if suf in T else None


def parse(tok):
    core, _, bass = tok.partition("/")
    m = re.match(r"^([b#]*)([IViv]+)(.*)$", core)
    if not m:
        return None
    acc = m.group(1).count("#") - m.group(1).count("b")
    num = m.group(2)
    if num.upper() not in DEG:
        return None
    upper = num[0].isupper()
    q = quality(m.group(3), upper)
    if q is None:
        return None
    deg = (DEG[num.upper()] + acc) % 12
    b = -1
    if bass:
        bm = re.match(r"^([b#]*)(\d+)$", bass)
        if bm and 1 <= int(bm.group(2)) <= 13:
            iv = (BASSN[int(bm.group(2))] + bm.group(1).count("#") - bm.group(1).count("b")) % 12
            if iv:
                b = (deg + iv) % 12
    return (deg, q, b)


SPLIT = {1: [Fr(1)], 2: [Fr(1, 2)] * 2, 3: [Fr(1, 2), Fr(1, 4), Fr(1, 4)], 4: [Fr(1, 4)] * 4}


def to_slots(prog):
    seq = []
    for bar in prog.split("|"):
        toks = bar.split()
        if not toks or len(toks) > 4:
            return None
        for t, d in zip(toks, SPLIT[len(toks)]):
            c = parse(t)
            if c is None:
                return None
            if seq and seq[-1][0] == c:
                seq[-1][1] += d
            else:
                seq.append([c, d])
    if len(seq) > 1 and seq[0][0] == seq[-1][0] and len(seq) == 2:
        pass
    for L, idx in ((Fr(2), 13), (Fr(1), 12), (Fr(1, 2), 11)):
        out = []
        ok = True
        for c, d in seq:
            rest = d
            while rest > 0:
                for mult, ln in ((2, 2), (1, 1), (Fr(1, 2), 0)):
                    if rest >= L * mult:
                        out.append((c, ln))
                        rest -= L * mult
                        break
                else:
                    ok = False
                    break
            if not ok:
                break
        if ok and len(out) <= 8 and len(out) >= 2:
            return idx, out
    return None


FAM = dict(SOUL=1, GOSPEL=2, NEO=4, JAZZ=8, LATIN=16, POP=32, ROCK=64, FILM=128, TRAP=256, HOUSE=512, S80=1024,
           LOFI=2048, FUNK=4096, BLUES=8192, WORLD=16384)
STYLES = ['Pop', '60s Brit Pop', '70s Soul', '80s Pop Funk', 'Neo Soul', 'Gospel', 'Jazz', '90s R&B', 'House', 'Lo-Fi',
          'Cinematic', 'Rock', 'Trap', 'Boom Bap', 'Drill', 'Afrobeats', 'Amapiano', 'Reggaeton', 'Bossa Nova',
          'Salsa / Son', 'Blues', 'Country', 'Disco', 'Synthwave', 'Ambient', 'Classical', 'Romantic', 'EDM',
          'Future Bass', 'UK Garage', 'City Pop', 'K-Pop', 'Flamenco', 'Reggae', 'Motown', 'Metal']
FAM_STYLES = {
    "SOUL": ['70s Soul', 'Motown', '90s R&B'], "GOSPEL": ['Gospel'], "NEO": ['Neo Soul', 'Lo-Fi', '90s R&B'],
    "JAZZ": ['Jazz', 'Lo-Fi', 'Bossa Nova'], "LATIN": ['Bossa Nova', 'Salsa / Son', 'Reggaeton', 'Flamenco'],
    "POP": ['Pop', '60s Brit Pop', 'K-Pop', 'City Pop'], "ROCK": ['Rock', 'Metal', '60s Brit Pop'],
    "FILM": ['Cinematic', 'Ambient', 'Classical', 'Romantic'], "TRAP": ['Trap', 'Drill'],
    "HOUSE": ['House', 'UK Garage', 'EDM', 'Disco', 'Future Bass'], "S80": ['80s Pop Funk', 'Synthwave', 'City Pop'],
    "LOFI": ['Lo-Fi', 'Boom Bap'], "FUNK": ['80s Pop Funk', 'Disco', '70s Soul', 'Boom Bap'],
    "BLUES": ['Blues', 'Country'], "WORLD": ['Afrobeats', 'Amapiano', 'Reggae', 'Flamenco']}


def style_mask(fam):
    m = 0
    for k, bit in FAM.items():
        if fam & bit:
            for st in FAM_STYLES[k]:
                m |= 1 << STYLES.index(st)
    return m


def famval(expr):
    v = 0
    for part in expr.split("|"):
        part = part.strip()
        if part.isdigit():
            v |= int(part)
        else:
            v |= FAM[part[2:].replace("80S", "S80")]
    return v


entries = []
ref = open(os.path.join(SRC, "sf_ref.h"), encoding="utf-8").read()
for m in re.finditer(r'\{ "((?:[^"\\]|\\.)*)", "([^"]*)", (\d), (\d+), (\d+), (\d+) \}', ref):
    entries.append(dict(name=m.group(1), prog=m.group(2), minor=int(m.group(3)), fam=int(m.group(4)),
                        w=int(m.group(5)), rich=int(m.group(6)), kind=0))
comp = open(os.path.join(SRC, "sf_compose.c"), encoding="utf-8").read()
lib = comp[comp.index("static const libprog_t LIB[]"):]
lib = lib[:lib.index("};")]
for m in re.finditer(r'\{ "((?:[^"\\]|\\.)*)", "([^"]*)", (\d), ([A-Z_0-9 |]+) \}', lib):
    entries.append(dict(name=m.group(1), prog=m.group(2), minor=int(m.group(3)), fam=famval(m.group(4)), w=60,
                        rich=50, kind=1))

seen = {}
out = []
skipped = 0
for e in entries:
    r = to_slots(e["prog"])
    if r is None:
        skipped += 1
        continue
    bars_idx, slots = r
    key = (e["minor"], bars_idx, tuple(slots))
    if key in seen:
        o = out[seen[key]]
        o["w"] = max(o["w"], e["w"])
        continue
    if e["kind"] == 0 and " - " in e["name"]:
        artist, title = e["name"].split(" - ", 1)
    else:
        artist, title = ("LIBRARY" if e["kind"] else ""), e["name"]
    seen[key] = len(out)
    out.append(dict(artist=artist.strip(), title=title.strip(), roman=e["prog"], minor=e["minor"],
                    styles=style_mask(e["fam"]), w=e["w"], rich=e["rich"], bars=bars_idx, slots=slots,
                    kind=e["kind"]))

out.sort(key=lambda o: (o["kind"], o["artist"], o["title"]))


def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


lines = ["/* generated by tools/song_refs.py from MIDI CONDUCTA 3.3 (sf_ref.h: McGill Billboard Project chord annotations,",
         " * CC0, J. A. Burgoyne, J. Wild, I. Fujinaga, ISMIR 2011; phrases from public-domain Mutopia scores; and its named",
         " * progression library). Only chord changes. Do not edit. */"]
for o in out:
    n = len(o["slots"])
    ch = ", ".join("{%d, %d, %d, %d}" % (c[0], c[1], c[2], ln) for c, ln in o["slots"])
    lines.append("    {%s, %s, %s, %d, 0x%xULL, %d, %d, %d, %d, %d, {%s}}," % (
        cstr(o["artist"]), cstr(o["title"][:60]), cstr(o["roman"][:120]), o["minor"], o["styles"], min(o["w"], 255),
        o["rich"], o["bars"], o["kind"], n, ch))
os.makedirs(os.path.join(ROOT, "engine", "gen"), exist_ok=True)
with open(os.path.join(ROOT, "engine", "gen", "songrefs_gen.inc"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
print("%d loops (%d songs / library), %d skipped (more than 8 slots or a length that does not fit)" % (
    len(out), len({(o['artist'], o['title']) for o in out}), skipped))
