#!/usr/bin/env python3
"""Draw a page of the skin preview with the real texts and values of an engine session (tests/mock_state.c), so a
mockup shows what the plugin really says. Reads vst/layout.conf and vst/params.json; image y = layout y - 86.

usage: mockup.py PREVIEW_PNG STATE_TSV FONTS_DIR OUT_PNG TAB [PANEL_INDEX]"""
import json
import os
import re
import sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
prev, state, fonts, out, tab = sys.argv[1:6]
panel = int(sys.argv[6]) if len(sys.argv) > 6 else None
ST = dict(l.rstrip("\n").split("\t", 1) for l in open(state) if "\t" in l)
PR = {p["key"]: p for p in json.load(open(os.path.join(ROOT, "vst", "params.json")))["params"]}
PANEL_OPTS = [o.lower() for o in PR["panel"]["options"]]
im = Image.open(prev).convert("RGB")
d = ImageDraw.Draw(im)
f = lambda s: ImageFont.truetype(os.path.join(fonts, "TitilliumWeb-SemiBold.ttf"), s)
CREAM, GOLD, DIM, DARK, LCD = (236, 228, 212), (214, 168, 88), (163, 156, 142), (23, 24, 27), (17, 18, 21)
KEYS = {0: None, 1: (242, 196, 107), 2: (200, 120, 40), 3: (88, 214, 128), 4: (176, 70, 70)}
KEYS_B = {0: None, 1: (242, 196, 107), 2: (200, 120, 40), 3: (70, 196, 110), 4: (128, 34, 34)}


def text(x, cy, t, size, col, maxw, align="center"):
    fo = f(size)
    while d.textlength(t, font=fo) > maxw and size > 9:
        size -= 1
        fo = f(size)
    w = d.textlength(t, font=fo)
    x0 = x - w / 2 if align == "center" else x
    d.text((x0, cy - size * 0.72), t, font=fo, fill=col)


def parse():
    tabs, cur = {}, None
    for line in open(os.path.join(ROOT, "vst", "layout.conf")):
        line = line.strip()
        m = re.match(r"\[tab (.*)\]", line)
        if m:
            cur = m.group(1)
            tabs[cur] = []
            continue
        if not cur or not line or line.startswith("#"):
            continue
        kind = line.split()[0]
        kv = dict((a, b.strip('"')) for a, b in re.findall(r'(\w+)=("[^"]*"|\S+)', line))
        tabs[cur].append((kind, kv))
    return tabs


def shown(kv):
    w = kv.get("when")
    if not w:
        return True
    p, o = w.split(":")
    oi = int(o) if o.isdigit() else PANEL_OPTS.index(o.lower())
    return panel == oi


def opt_label(key):
    p = PR.get(key, {})
    v = ST.get(key, "")
    if "options" in p and v.lstrip("-").isdigit():
        i = int(v)
        if 0 <= i < len(p["options"]):
            return p["options"][i]
    return v


for kind, kv in parse()[tab]:
    if not shown(kv):
        continue
    I = lambda k: int(kv[k])
    if kind == "readout":
        cx, cy, w, h = I("cx"), I("cy") - 86, I("w"), I("h")
        t = ST.get(kv["key"], "")
        d.rectangle((cx - w / 2 + 1, cy - h / 2 + 1, cx + w / 2 - 1, cy + h / 2 - 1), fill=LCD)
        size = int(kv.get("tsize", 16))
        if kv.get("talign") == "left":
            text(cx - w / 2 + 10, cy + 1, t, size, GOLD if kv["key"] == "prof_1" else CREAM, w - 20, "left")
        else:
            text(cx, cy + 1, t, size, CREAM, w - 12)
    elif kind == "popup":
        cx, cy, w, h = I("cx"), I("cy") - 86, I("w"), I("h")
        d.rectangle((cx - w / 2 + 8, cy - h / 2 + 3, cx + w / 2 - 44, cy + h / 2 - 3), fill=LCD)
        text(cx - w / 2 + 14, cy + 1, opt_label(kv["key"]), 19, GOLD, w - 64, "left")
    elif kind == "stepper":
        cx, cy, w, h = I("cx"), I("cy") - 86, I("w"), I("h")
        t = ST.get(kv.get("get", kv["key"]), "")
        d.rectangle((cx - w / 2 + 60, cy - h / 2 + 4, cx + w / 2 - 60, cy + h / 2 - 4), fill=LCD)
        text(cx, cy + 1, t, 19, GOLD, w - 130)
    elif kind == "list":
        x, y, w, cols, rows, th, gap = I("x"), I("y") - 86, I("w"), I("cols"), I("rows"), I("th"), int(kv.get("gap", 8))
        tw = (w - (cols - 1) * gap) // cols
        for i in range(cols * rows):
            if kv.get("order") == "cols":
                c, r = i // rows, i % rows
            else:
                c, r = i % cols, i // cols
            k = "%s_%d" % (kv["key"], i + 1)
            x0, y0 = x + c * (tw + gap), y + r * (th + gap)
            on = ST.get(k + "_on") == "1"
            d.rectangle((x0 + 1, y0 + 1, x0 + tw - 1, y0 + th - 1), fill=LCD)
            if on:
                d.rounded_rectangle((x0 + 1, y0 + 1, x0 + tw - 1, y0 + th - 1), radius=4, fill=(74, 50, 28), outline=GOLD, width=3)
            text(x0 + tw / 2, y0 + th / 2 + 2, ST.get(k, ""), int(kv.get("tsize", 18)), GOLD if on else CREAM, tw - 10)
    elif kind == "meter":
        cx, cy, w, h = I("cx"), I("cy") - 86, I("w"), I("h")
        v = int(ST.get(kv["key"], "0") or 0)
        black = "black" in kv["strip"]
        col = (KEYS_B if black else KEYS).get(v)
        base = (24, 24, 26) if black else (236, 230, 218)
        d.rounded_rectangle((cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2), radius=3, fill=col or base)
        if v == 4:
            for xx in range(int(cx - w / 2) - h, int(cx + w / 2), 6):
                d.line((max(xx, cx - w / 2), cy + h / 2, min(xx + h, cx + w / 2), cy - h / 2), fill=(60, 10, 10), width=2)
    elif kind == "knob" and kv.get("lay") == "side":
        cx, cy, bw, bh, r = I("cx"), I("cy") - 86, I("bw"), I("bh"), I("r")
        sz = 2 * r + 10
        x0 = cx - bw / 2 + sz + 4
        d.rectangle((x0, cy - bh / 2 + 2, cx + bw / 2 - 2, cy + bh / 2 - 2), fill=(28, 29, 33))
        text((x0 + cx + bw / 2) / 2, cy + 1, ST.get(kv["key"], "") + "%", 22, CREAM, bw - sz - 10)
    elif kind == "enum_h":
        opts = kv.get("options", "").split(",")
        if not opts or kv["key"] not in ST:
            continue
        v = ST[kv["key"]]
        if not v.isdigit() or int(v) >= len(opts):
            continue
        sw, sh = I("sw"), int(kv.get("sh", 33))
        n = len(opts)
        tot = n * sw + (n - 1) * 2
        x0 = I("cx") - tot / 2 + int(v) * (sw + 2)
        cy = I("cy") - 86 + (0 if not kv.get("label") else 0)
        d.rectangle((x0, cy - sh / 2, x0 + sw, cy + sh / 2), fill=(242, 196, 107))
        text(x0 + sw / 2, cy + 2, opts[int(v)], 14, DARK, sw - 4)
im.save(out)
print("mockup:", out)
