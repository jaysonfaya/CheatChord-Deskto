#!/usr/bin/env python3
"""CheatChord 4.0: tools/cc_data.py -> src/gen/*.inc and src/gen/counts.h (styles, profiles, grooves, presets).

Also the one place that knows the option lists of the skin (names()), and the checks that keep every style, profile,
groove and preset distinct: run it and it fails on duplicates (names, descriptions, numbers that would sound alike)."""
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cc_data as D  # noqa: E402

ROOT = os.path.dirname(HERE)
GEN = os.path.join(ROOT, "src", "gen")

KEYS = ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
KEY_ALIAS = {"C#": 1, "D#": 3, "Gb": 6, "G#": 8, "A#": 10}
SCALES = ["Major", "Minor", "Dorian", "Mixolydian", "Lydian", "Phrygian", "Harm. Minor", "Mel. Minor"]
OLD_RHYTHMS = ["Sustain", "Whole Notes", "Half Notes", "Quarters", "Eighths", "Offbeats", "Charleston", "Tresillo",
               "Funk 16ths", "Garage", "Neo Soul Lazy", "Arp Up", "Arp Down", "Arp Up/Down", "Arp Random", "Broken Chord"]
VOICINGS = ["Close", "Open", "Drop 2", "Spread", "Neo Soul", "Shell"]
SOUNDS = ["E-Piano", "Soft Pad", "Organ", "Synth Keys", "Strings", "Bell Keys"]
# note values from a 1/64 triplet to 16 bars (CHORD LENGTH, RHYTHM RATE, ARP RATE); ticks at 96 per beat
DURS = [("1/64T", 4), ("1/64", 6), ("1/32T", 8), ("1/32", 12), ("1/16T", 16), ("1/16", 24), ("1/8T", 32), ("1/8", 48),
        ("1/4T", 64), ("1/4", 96), ("1/2T", 128), ("1/2 Bar", 192), ("1 Bar", 384), ("2 Bars", 768), ("4 Bars", 1536),
        ("8 Bars", 3072), ("16 Bars", 6144)]

QN = {"maj": "Q_MAJ", "min": "Q_MIN", "m": "Q_MIN", "dim": "Q_DIM", "aug": "Q_AUG", "sus2": "Q_SUS2", "sus4": "Q_SUS4",
      "6": "Q_6", "m6": "Q_M6", "maj7": "Q_MAJ7", "7": "Q_7", "m7": "Q_M7", "m7b5": "Q_M7B5", "dim7": "Q_DIM7",
      "mmaj7": "Q_MMAJ7", "7sus4": "Q_7SUS4", "add9": "Q_ADD9", "madd9": "Q_MADD9", "69": "Q_69", "maj9": "Q_MAJ9",
      "9": "Q_9", "m9": "Q_M9", "m11": "Q_M11", "9sus4": "Q_9SUS4", "13": "Q_13", "maj7#11": "Q_MAJ7S11",
      "7b9": "Q_7B9", "7#9": "Q_7S9", "7#5": "Q_7S5", "maj13": "Q_MAJ13", "13sus4": "Q_13SUS4"}
QORDER = ["maj", "min", "dim", "aug", "sus2", "sus4", "6", "m6", "maj7", "7", "m7", "m7b5", "dim7", "mmaj7", "7sus4",
          "add9", "madd9", "69", "maj9", "9", "m9", "m11", "9sus4", "13", "maj7#11", "7b9", "7#9", "7#5", "maj13", "13sus4"]
ROMAN = {"i": 0, "ii": 2, "iii": 4, "iv": 5, "v": 7, "vi": 9, "vii": 11}
FAMBIT = {"M": 1, "m": 2, "D": 4, "o": 8, "h": 16, "s": 32, "A": 64}
MX = 1 | 4 | 32


def fail(msg):
    sys.exit("gen_data: " + msg)


def roman(tok, frame, loose):
    """'bVII7s' -> (frame, degree, family mask). loose: a plain upper-case numeral also matches dominant / sus forms."""
    m = re.match(r"^([b#]?)(vii|iii|ii|iv|vi|v|i|VII|III|II|IV|VI|V|I)(.*)$", tok)
    if not m:
        fail("bad numeral %r" % tok)
    acc, num, suf = m.groups()
    deg = (ROMAN[num.lower()] + (-1 if acc == "b" else 1 if acc == "#" else 0)) % 12
    up = num.isupper()
    fam = 0
    for ch in suf:
        if ch == "7":
            fam |= FAMBIT["D"] if up else FAMBIT["m"]
        elif ch in "ohsAMm+":
            fam |= FAMBIT["A" if ch == "+" else ch]
        elif ch == "*":
            fam |= MX
        else:
            fail("bad suffix %r in %r" % (ch, tok))
    if not fam:
        fam = (MX if loose else FAMBIT["M"]) if up else FAMBIT["m"]
    return frame, deg, fam


def split_frame(t):
    if t.startswith("m:"):
        return 1, t[2:]
    if t.startswith("b:"):
        return 2, t[2:]
    return 0, t


def parse_toks(s):
    out = []
    for t in s.split():
        f, r = split_frame(t)
        out.append(roman(r, f, False))
    return out


def parse_moves(s):
    out = []
    for chain in s.split():
        f, r = split_frame(chain)
        parts = [roman(x, f, True) for x in r.split("-")]
        for a, b in zip(parts, parts[1:]):
            out.append((f, a[1], b[1], a[2], b[2]))
    return out


def parse_q(s):
    out = []
    for t in s.split():
        neg = t.startswith("-")
        t = t.lstrip("-")
        if t not in QN:
            fail("bad chord colour %r" % t)
        out.append((QN[t], neg))
    return out


def parse_nums(s, absolute):
    v = dict.fromkeys(D.NUMKEYS, None if absolute else 0.0)
    for kv in s.split():
        k, x = kv.split("=")
        if k not in v:
            fail("bad number %r" % k)
        v[k] = float(x)
    if absolute and any(x is None for x in v.values()):
        fail("style numbers missing: %s" % s)
    return [v[k] for k in D.NUMKEYS]


def ramp(n, hi, lo):
    return [hi - (hi - lo) * i / max(1, n - 1) for i in range(n)]


def frame_ramp(items, hi, lo):
    """weights strongest first, counted per key frame (major, minor and both lists each start at hi)"""
    ws, pos = [], {}
    for it in items:
        f = it[0]
        n = sum(1 for x in items if x[0] == f)
        k = pos.get(f, 0)
        pos[f] = k + 1
        ws.append(ramp(n, hi, lo)[k])
    return ws


def c_tok(arr, name, toks, hi, lo):
    ws = frame_ramp(toks, hi, lo)
    rows = ["{%d, %d, %d, %.2ff}" % (f, d, fm, w) for (f, d, fm), w in zip(toks, ws)] + ["END_T"]
    return "static const tok_t %s[] = {%s};" % (name, ", ".join(rows))


def c_idiom(name, moves):
    ws = frame_ramp(moves, 1.0, .5)
    rows = ["{%d, %d, %d, %d, %d, %.2ff}" % (f, a, b, fa, fb, w) for (f, a, b, fa, fb), w in zip(moves, ws)] + ["END_I"]
    return "static const idiom_t %s[] = {%s};" % (name, ", ".join(rows))


def c_q(name, qs):
    pos = [q for q, neg in qs if not neg]
    ws = ramp(len(pos), 1.0, .4)
    rows = ["{%s, %.2ff}" % (q, w) for q, w in zip(pos, ws)] + ["{%s, -0.40f}" % q for q, neg in qs if neg] + ["END_Q"]
    return "static const qpref_t %s[] = {%s};" % (name, ", ".join(rows))


def cstr(s):
    return '"%s"' % s.replace("\\", "\\\\").replace('"', '\\"')


def style_names():
    return [n for n, _ in D.OLD_STYLES] + [s["name"] for s in D.NEW_STYLES]


def all_profiles():
    """profile index -> dict(name, kind, style, t, why); 0 is "none"; 1..25 the 3.5 profiles; then the new ones"""
    out = [dict(name="Off", kind="-", style=None, t="NO PROFILE: THE STYLE ALONE", why="")]
    for n, k, st, t, why in D.OLD_PROFILES:
        out.append(dict(name=n, kind=k, style=st, t=t, why=why, old=True))
    for p in D.NEW_PROFILES:
        out.append(dict(p, old=False))
    return out


def lists():
    """the ARTIST and PRODUCER option lists: index 0 none, then alphabetical"""
    pr = all_profiles()
    key = lambda i: pr[i]["name"].lower().lstrip(".'")
    a = [0] + sorted([i for i, p in enumerate(pr) if p["kind"] == "A"], key=key)
    p = [0] + sorted([i for i, x in enumerate(pr) if x["kind"] == "P"], key=key)
    return a, p


def names():
    """option labels for tools/make_params.py"""
    pr = all_profiles()
    a, p = lists()
    return dict(styles=style_names(), artists=["No Artist"] + [pr[i]["name"] for i in a[1:]],
                producers=["No Producer"] + [pr[i]["name"] for i in p[1:]],
                rhythms=OLD_RHYTHMS + [g[0] for g in D.GROOVES], durs=[d[0] for d in DURS],
                presets=[x["name"] for x in D.PRESETS], keys=KEYS, scales=SCALES, voicings=VOICINGS, sounds=SOUNDS)


def chord_of(tok):
    r, q = tok.split(":")
    f, r = split_frame(r)
    _, deg, _ = roman(r, f, False)
    if q not in QN:
        fail("preset chord colour %r" % q)
    return deg, QN[q]


def check(styles_rows, prof_feats):
    sn = style_names()
    if len(set(sn)) != len(sn):
        fail("duplicate style names")
    descs = [d for _, d in D.OLD_STYLES] + [s["desc"] for s in D.NEW_STYLES]
    if len(set(descs)) != len(descs):
        fail("duplicate style descriptions")
    for i in range(len(styles_rows)):
        for j in range(i + 1, len(styles_rows)):
            if styles_rows[i] == styles_rows[j]:
                fail("styles %s and %s are the same" % (sn[i], sn[j]))
    pr = all_profiles()
    for fld in ("name", "t", "why"):
        seen = {}
        for p in pr[1:]:
            if p[fld] in seen:
                fail("profiles %s and %s share their %s: %r" % (seen[p[fld]], p["name"], fld, p[fld]))
            seen[p[fld]] = p["name"]
    keys = list(prof_feats)
    for i in range(len(keys)):
        for j in range(i + 1, len(keys)):
            a, b = prof_feats[keys[i]], prof_feats[keys[j]]
            if a == b:
                fail("profiles %s and %s would sound the same" % (keys[i], keys[j]))
    gn = names()["rhythms"]
    if len(set(gn)) != len(gn):
        fail("duplicate rhythm names")
    pats = [g[2] for g in D.GROOVES]
    if len(set(pats)) != len(pats):
        fail("two grooves share a pattern")
    for g in D.GROOVES:
        if (len(g[2]) % g[1]) or not re.fullmatch(r"[Xx.\-BbUu]+", g[2]):
            fail("groove %r: pattern length must be whole beats, chars X x - . B b U u" % g[0])
    seen = set()
    for p in D.PRESETS:
        sig = (p["chords"], p["style"], p["rhythm"])
        if p["name"] in seen or sig in seen:
            fail("preset %r is a duplicate" % p["name"])
        seen.add(p["name"]); seen.add(sig)


def main():
    os.makedirs(GEN, exist_ok=True)
    sn = style_names()
    out = ["/* generated by tools/gen_data.py from tools/cc_data.py: do not edit */"]
    rows, srows = [], []
    for s in D.NEW_STYLES:
        k = s["key"]
        nums = parse_nums(s["nums"], True)
        toks, moves, qs, st = parse_toks(s["toks"]), parse_moves(s["moves"]), parse_q(s["q"]), parse_toks(s["starts"])
        out += [c_tok(None, "G%s_T" % k, toks, 1.2, .35), c_idiom("G%s_I" % k, moves), c_q("G%s_Q" % k, qs),
                c_tok(None, "G%s_S" % k, st, 2.0, .8)]
        rows.append("{%s, G%s_T, G%s_I, G%s_Q, G%s_S}" % (", ".join("%.2ff" % x for x in nums), k, k, k, k))
        srows.append((tuple(nums), s["toks"], s["moves"], s["q"]))
    out.append("#define GEN_STYLE_NAMES %s" % ", ".join(cstr(s["name"]) for s in D.NEW_STYLES))
    out.append("#define GEN_STYLE_ROWS %s" % ", ".join(rows))
    out.append("const char *const STYLE_DESC[NUM_STYLES] = {%s};" %
               ", ".join(cstr(d) for d in [d for _, d in D.OLD_STYLES] + [s["desc"] for s in D.NEW_STYLES]))
    open(os.path.join(GEN, "styles_gen.inc"), "w").write("\n".join(out) + "\n")

    # profiles
    pr = all_profiles()
    out = ["/* generated by tools/gen_data.py from tools/cc_data.py: do not edit */"]
    rows, feats = [], {}
    for i, p in enumerate(pr):
        if i == 0 or p["old"]:
            continue
        if p["style"] not in sn:
            fail("profile %s: unknown style %r" % (p["name"], p["style"]))
        nums = parse_nums(p["nums"], False)
        moves = parse_moves(p["moves"])
        if p["deg"]:
            toks = parse_toks(p["deg"])
        else:   # the chords of its moves are its vocabulary
            toks, seen = [], set()
            for f, a, b, fa, fb in moves:
                for d, fm in ((a, fa), (b, fb)):
                    if (f, d) not in seen:
                        seen.add((f, d))
                        toks.append((f, d, fm))
        st = parse_toks(p["starts"]) if p["starts"] else [toks[0]]
        qs = parse_q(p["q"])
        n = "GP%d" % i
        out += [c_tok(None, n + "_T", toks, .9, .4), c_idiom(n + "_I", moves), c_q(n + "_Q", qs), c_tok(None, n + "_S", st, 1.2, .8)]
        rows.append("{%s, %s_T, %s_I, %s_Q, %s_S}" % (", ".join("%.2ff" % x for x in nums), n, n, n, n))
        feats[p["name"]] = (tuple(nums), tuple(moves), tuple(qs), p["style"])
    out.append("#define GEN_PROFILE_ROWS %s" % ", ".join(rows))
    info = []
    for i, p in enumerate(pr):
        st = sn.index(p["style"]) if p["style"] else -1
        info.append("{%s, %d, %s, %s, %d}" % (cstr(p["name"]), st, cstr(p["t"]), cstr(p["why"]),
                                             {"A": 1, "P": 2}.get(p["kind"], 0)))
    out.append("const artist_info_t ARTIST_INFO[NUM_ARTISTS] = {%s};" % ",\n    ".join(info))
    a, pl = lists()
    out.append("const int ARTIST_LIST[NUM_ARTIST_LIST] = {%s};" % ", ".join(map(str, a)))
    out.append("const int PRODUCER_LIST[NUM_PRODUCER_LIST] = {%s};" % ", ".join(map(str, pl)))
    open(os.path.join(GEN, "profiles_gen.inc"), "w").write("\n".join(out) + "\n")

    # grooves
    out = ["/* generated by tools/gen_data.py from tools/cc_data.py: do not edit */",
           "static const groove_t GROOVES[NUM_GROOVES] = {"]
    for g in D.GROOVES:
        out.append("    {%s, %d, %d, %s}," % (cstr(g[0]), g[1], len(g[2]), cstr(g[2])))
    out.append("};")
    out.append("static const int DUR_TICKS[NUM_DURS] = {%s};" % ", ".join(str(t) for _, t in DURS))
    out.append("static const char *const DUR_NAMES[NUM_DURS] = {%s};" % ", ".join(cstr(n) for n, _ in DURS))
    open(os.path.join(GEN, "grooves_gen.inc"), "w").write("\n".join(out) + "\n")

    # presets
    nm = names()
    out = ["/* generated by tools/gen_data.py from tools/cc_data.py: do not edit */",
           "static const fpreset_t FPRESETS[NUM_FACTORY_PRESETS] = {"]

    def idx(lst, v, what, pname):
        if v not in lst:
            fail("preset %s: unknown %s %r" % (pname, what, v))
        return lst.index(v)
    for p in D.PRESETS:
        key = KEY_ALIAS.get(p["key"], KEYS.index(p["key"]) if p["key"] in KEYS else -1)
        if key < 0:
            fail("preset %s: key %r" % (p["name"], p["key"]))
        art = 0 if p["artist"] == "Off" else idx(nm["artists"], p["artist"], "artist", p["name"])
        pro = 0 if p["producer"] == "Off" else idx(nm["producers"], p["producer"], "producer", p["name"])
        ch = [chord_of(t) for t in p["chords"].split()]
        out.append("    {%s, %s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, {%s}}," % (
            cstr(p["name"]), cstr(p["desc"]), key, idx(SCALES, p["scale"], "scale", p["name"]),
            idx(nm["styles"], p["style"], "style", p["name"]), art, pro, p["spice"],
            idx(VOICINGS, p["voicing"], "voicing", p["name"]), idx(nm["rhythms"], p["rhythm"], "rhythm", p["name"]),
            idx(nm["durs"], p["bars"], "length", p["name"]), p["swing"], idx(SOUNDS, p["sound"], "sound", p["name"]),
            len(ch), ", ".join("{%d, %s}" % c for c in ch)))
    out.append("};")
    open(os.path.join(GEN, "presets_gen.inc"), "w").write("\n".join(out) + "\n")

    counts = ["/* generated by tools/gen_data.py: do not edit */", "#pragma once",
              "#define NUM_STYLES %d" % len(sn), "#define NUM_ARTISTS %d" % len(pr),
              "#define NUM_ARTIST_LIST %d" % len(a), "#define NUM_PRODUCER_LIST %d" % len(pl),
              "#define NUM_GROOVES %d" % len(D.GROOVES), "#define NUM_OLD_RHYTHMS %d" % len(OLD_RHYTHMS),
              "#define NUM_DURS %d" % len(DURS), "#define NUM_FACTORY_PRESETS %d" % len(D.PRESETS),
              "#define NUM_USER_PRESETS %d" % D.NUM_USER_PRESETS]
    open(os.path.join(GEN, "counts.h"), "w").write("\n".join(counts) + "\n")
    check(srows, feats)
    print("gen_data: %d styles, %d artists, %d producers, %d grooves, %d presets" %
          (len(sn), len(a) - 1, len(pl) - 1, len(D.GROOVES), len(D.PRESETS)))


if __name__ == "__main__":
    main()
