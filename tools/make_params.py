#!/usr/bin/env python3
"""CheatChord: the one parameter list. Writes vst/params.json (for the skin builder) and src/params_gen.h (for the
engine), so the two never drift apart. The list order is the VST parameter index: append only, never reorder."""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import gen_data  # noqa: E402  (4.0: the option lists come from tools/cc_data.py)
NM = gen_data.names()

KEYS = ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
SCALES = ["Major", "Minor", "Dorian", "Mixolydian", "Lydian", "Phrygian", "Harm. Minor", "Mel. Minor"]
STYLES = NM["styles"]
RHYTHMS = NM["rhythms"]
DURS = NM["durs"]
VOICINGS = ["Close", "Open", "Drop 2", "Spread", "Neo Soul", "Shell"]
SOUNDS = ["E-Piano", "Soft Pad", "Organ", "Synth Keys", "Strings", "Bell Keys"]
TEMPLATES = ["Axis Pop", "50s Doo-Wop", "Sad Pop", "Royal Road", "Jazz ii-V-I", "Turnaround", "Neo Soul Descent",
             "Backdoor", "Mixolydian Rock", "Gospel Walk", "Canon", "Major-Minor IV", "70s Soul Lift",
             "Chromatic Soul", "Brit Pop Bridge", "Lo-Fi Sway", "R&B Slow Jam", "Andalusian", "Epic Minor",
             "Deep House", "Dorian Vamp", "Minor ii-V-i", "Line Cliche", "80s Funk Minor", "Dark Trap",
             "Cinematic Rise"]
NUMQ = 30
OFFON = ["Off", "On"]
KEYLIGHT = ["Off", "On", "Bass", "New", "Gone"]   # 4.0: New / Gone = what an influence change added or took away

P = []


def opt(key, name, options, default=0, **kw):
    P.append(dict(key=key, name=name, options=options, default=default, **kw))


def num(key, name, lo, hi, default, unit="", **kw):
    d = dict(key=key, name=name, min=lo, max=hi, default=default, display="int", **kw)
    if unit:
        d["unit"] = unit
    P.append(d)


def trig(key, name, **kw):
    P.append(dict(key=key, name=name, min=0, max=1, momentary=True, **kw))


def text(key, name, **kw):
    P.append(dict(key=key, name=name, display="string", type="readout", **kw))


def tile(key, name):
    P.append(dict(key=key, name=name, display="string"))


# ---- index 0 is toggled by MPC on insert and STOP (JUCE prepareToPlay): keep it harmless
num("level", "Synth Level", 0, 100, 75, "%")
opt("key", "Key", KEYS, 0, qlink_ticks=4)
opt("scale", "Scale", SCALES, 0, qlink_ticks=4)
opt("style", "Style", STYLES, 0, qlink_ticks=4)
num("spice", "Spice", 0, 100, 35, "%")
for i in range(1, 9):
    tile("sug_%d" % i, "Idea %d" % i)
for i in range(1, 9):
    tile("prog_%d" % i, "Chord %d" % i)
for i in range(1, 9):
    num("tint_%d" % i, "Tint %d" % i, 0, 5, 4, live=True)
trig("add", "Add")
trig("reroll", "Reroll")
trig("generate", "Generate")
trig("delete", "Delete")
trig("clear", "Clear")
opt("gen_len", "Loop Length", ["4 Chords", "8 Chords"], 0)
text("status", "Status", poll=True)
# ---- PLAY
opt("follow", "Play With MPC", OFFON, 1)
opt("bars", "Chord Length", DURS, 12, qlink_ticks=3)
opt("rhythm", "Rhythm", RHYTHMS, 0, qlink_ticks=3)
opt("rate", "Arp Rate", DURS, 5, qlink_ticks=3)
opt("direction", "Direction", ["Forward", "Reverse", "Ping-Pong", "Random"], 0, qlink_ticks=4)
opt("push", "Push", OFFON, 0)
num("strum", "Strum", 0, 100, 0, "ms")
num("swing", "Swing", 50, 75, 50, "%")
num("gate", "Gate", 10, 100, 92, "%")
num("velocity", "Velocity", 1, 127, 100)
num("humanize", "Humanize", 0, 100, 10, "%")
opt("mute", "Mute", OFFON, 0)
# ---- VOICE
opt("voicing", "Voicing", VOICINGS, 0, qlink_ticks=4)
opt("lead", "Voice Leading", OFFON, 1)
opt("octave", "Octave", ["-2", "-1", "0", "+1", "+2"], 2, qlink_ticks=4)
opt("bass", "Bass Note", OFFON, 1)
opt("arp_oct", "Arp Octaves", ["1 Oct", "2 Oct", "3 Oct"], 0)
num("cursor", "Edit Slot", 1, 8, 1)
QSYM = ["maj", "m", "dim", "aug", "sus2", "sus4", "6", "m6", "maj7", "7", "m7", "m7b5", "dim7", "m(maj7)", "7sus4",
        "add9", "madd9", "6/9", "maj9", "9", "m9", "m11", "9sus4", "13", "maj7#11", "7b9", "7#9", "7#5", "maj13", "13sus4"]
assert len(QSYM) == NUMQ
NOTES = ["C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B"]
opt("slot_root", "Root", NOTES, 0, qlink_ticks=4)            # 4.0: option lists (popups); same values as before
opt("slot_type", "Chord Type", QSYM, 0, qlink_ticks=3)
opt("slot_bass", "Bass", ["Root"] + ["/" + n for n in NOTES], 0, qlink_ticks=4)
opt("slot_var", "Variation", ["Smoothest", "Alt 1", "Alt 2", "Alt 3"], 0)
opt("slot_len", "Slot Length", ["Half", "Normal", "Double"], 1)
# ---- SOUND / MIDI
opt("sound", "Sound", SOUNDS, 0, qlink_ticks=4)
num("tone", "Tone", 0, 100, 50, "%")
num("release", "Release", 0, 100, 35, "%")
num("space", "Space", 0, 100, 30, "%")
opt("synth", "Synth With MIDI", OFFON, 0)
num("channel", "MIDI Ch", 1, 16, 2)
num("bass_ch", "Bass Ch", 0, 16, 0)
num("pad_base", "Pad 1 Note", 0, 112, 36)
trig("panic", "All Notes Off")
trig("export", "Export MIDI")
# ---- LIBRARY
opt("template", "Template", TEMPLATES, 0)
trig("load", "Load")
text("tpl_info", "Template Chords", poll=True)
text("port", "MIDI Port", poll=True)
text("info", "Info", poll=True)
text("cursor_name", "Edit Slot Name")
text("slot_root_name", "Root Name")
text("slot_type_name", "Type Name")
text("slot_bass_name", "Bass Name")
text("slot_var_name", "Variation Name")
text("bass_ch_name", "Bass Ch Name")
text("pad_name", "Pad Note Name")
text("export_status", "Export Status", poll=True)
text("now", "Now Playing", poll=True)
# stepper arrows: the wrapper reads the target, adds the delta, clamps and writes it back
for k, nm in (("cursor", "Slot"), ("slot_root", "Root"), ("slot_type", "Type"), ("slot_bass", "Bass"),
              ("slot_var", "Variation"), ("channel", "MIDI Ch"), ("bass_ch", "Bass Ch"), ("pad_base", "Pad Note")):
    P.append(dict(key=k + "_prev", name=nm + " -", min=0, max=1, momentary=True, step_of=k, step_delta=-1))
    P.append(dict(key=k + "_next", name=nm + " +", min=0, max=1, momentary=True, step_of=k, step_delta=1))
opt("vel_mode", "Pad Velocity", ["Played", "Fixed"], 0)
trig("learn", "Learn Pad 1")
text("last_note", "Last Pad Note", poll=True)
# steppers for option lists (no pop-up lists over other controls)
for k, nm in (("key", "Key"), ("scale", "Scale"), ("style", "Style"), ("bars", "Length"), ("rhythm", "Rhythm"),
              ("sound", "Sound"), ("template", "Template")):
    P.append(dict(key=k + "_prev", name=nm + " -", min=0, max=1, momentary=True, step_of=k, step_delta=-1))
    P.append(dict(key=k + "_next", name=nm + " +", min=0, max=1, momentary=True, step_of=k, step_delta=1))

opt("midi_out", "MIDI Out", ["Off", "On"], 0)
opt("listen", "Play Mode", ["Off", "MPC", "Listen"], 1)

# ---- 2.0
# must match ARTIST_LIST / PRODUCER_LIST in src/artists.c (tests/engine_test.c checks it)
ARTISTS = NM["artists"]       # 4.0: alphabetical, index 0 = none (old projects are converted by the engine)
PRODUCERS = NM["producers"]
opt("artist", "Artist", ARTISTS, 0, qlink_ticks=3)
opt("panel", "Panel", ["Ideas", "History", "Browser", "Big Keys", "Presets", "My Presets"], 2)
opt("run", "Listen Play", OFFON, 0)
trig("stop", "Stop")
trig("undo", "Undo")
trig("redo", "Redo")
for i in range(1, 9):
    tile("hist_%d" % i, "History %d" % i)
for i in range(1, 13):
    opt("kb_%d" % i, "Key Light %d" % i, KEYLIGHT, 0, live=True, poll=False)
text("sel1", "Key Scale", poll=True)
text("sel2", "Style Artist", poll=True)
text("shown", "Shown Chord", poll=True)
text("mode_help", "Mode Help", poll=True)
text("artist_info", "Artist Info", poll=True)
for k, nm in (("artist", "Artist"),):
    P.append(dict(key=k + "_prev", name=nm + " -", min=0, max=1, momentary=True, step_of=k, step_delta=-1))
    P.append(dict(key=k + "_next", name=nm + " +", min=0, max=1, momentary=True, step_of=k, step_delta=1))


# ---- CheatChord 3.0
opt("producer", "Producer", PRODUCERS, 0, qlink_ticks=3)
opt("scope", "Edit Scope", ["Chord", "Whole"], 0)
for k, nm in (("producer", "Producer"),):
    P.append(dict(key=k + "_prev", name=nm + " -", min=0, max=1, momentary=True, step_of=k, step_delta=-1))
    P.append(dict(key=k + "_next", name=nm + " +", min=0, max=1, momentary=True, step_of=k, step_delta=1))
trig("keys_open", "Big Keys")
trig("br_more", "Open Browser")
trig("br_less", "Close Browser")
for i in range(1, 73):   # the big keyboard: 6 octaves from kb_lo (C of the lowest note's octave)
    opt("kbx_%d" % i, "Big Key %d" % i, KEYLIGHT, 0, live=True, poll=False)
for k, nm in (("notes", "Chord Notes"), ("kb_range", "Keyboard Range"), ("scope_help", "Scope Help"),
              ("producer_info", "Producer Info"), ("hist_pos", "History Step"), ("br_key", "Key Name"),
              ("br_scale", "Scale Name"), ("br_style", "Style Name"), ("br_artist", "Artist Name"),
              ("br_producer", "Producer Name"), ("kb_name", "Keys Chord")):
    text(k, nm, poll=True)

# ---- CheatChord 3.5
num("style_mix", "Style Influence", 0, 100, 50, "%")
num("artist_mix", "Artist Influence", 0, 100, 50, "%")
num("producer_mix", "Producer Influence", 0, 100, 50, "%")
opt("browse", "Browse", ["Key", "Scale", "Style", "Artist", "Producer"], 3)
trig("cruise", "Cruise")
opt("loop_sync", "Loop Sync", ["Manual", "Auto (MPC)", "1 Bar", "2 Bars", "4 Bars", "8 Bars", "16 Bars", "32 Bars"], 1)
opt("lib_kind", "Library", ["Artists", "Producers"], 1)
for i in range(1, 8):
    text("prof_%d" % i, "Profile Line %d" % i, poll=True)
for k, nm in (("loop_info", "Loop Info"), ("cruise_next", "Cruise Next"), ("mix_info", "Mix Info")):
    text(k, nm, poll=True)
for i in range(1, 15):   # the touch browser: big tiles with the options of the chosen category
    tile("bri_%d" % i, "Browser Tile %d" % i)
for i in range(1, 15):   # the library list
    tile("lib_%d" % i, "Library Tile %d" % i)

# ---- CheatChord 4.0
opt("rrate", "Rhythm Rate", ["Auto"] + DURS, 0, qlink_ticks=3)   # the rhythm's step: Auto = the groove's own grid
num("pad_count", "Pads Used", 1, 16, 16)
num("preset", "Preset", 0, len(NM["presets"]) + 16 - 1, 0)        # MPC's PRESET menu (vst.json programs); names from the engine
num("save_slot", "Save Slot", 1, 16, 1)
trig("save_preset", "Save Preset")
for k, nm in (("preset_info", "Preset Info"), ("kb_diff", "Keys Change"), ("style_info", "Style Info"),
              ("save_slot_name", "Save Slot Name"), ("learn_info", "Learn Info")):
    text(k, nm, poll=True)
for i in range(1, len(NM["presets"]) + 1):
    tile("pre_%d" % i, "Factory Preset %d" % i)
for i in range(1, 17):
    tile("usr_%d" % i, "User Preset %d" % i)
for k, nm in (("save_slot", "Save Slot"),):
    P.append(dict(key=k + "_prev", name=nm + " -", min=0, max=1, momentary=True, step_of=k, step_delta=-1))
    P.append(dict(key=k + "_next", name=nm + " +", min=0, max=1, momentary=True, step_of=k, step_delta=1))

# ---- CheatChord 4.1
opt("strum_mode", "Strum Direction", ["Up", "Down", "Alternate", "Guitar", "Random"], 0)   # Up = low to high
opt("slot_rev", "Reverse", ["Off", "On"], 0)   # the chord at the cursor (EDIT ALL: every chord) swells in reverse

with open(os.path.join(ROOT, "vst", "params.json"), "w") as f:
    json.dump({"name": "CheatChord", "params": P}, f, indent=2)

# C side: an enum of indices plus min/max/default for the numeric/option params
lines = ["/* generated by tools/make_params.py: do not edit */", "#pragma once", "enum {"]
for i, p in enumerate(P):
    lines.append("    P_%s = %d," % (p["key"].upper(), i))
lines.append("    NUM_P = %d" % len(P))
lines.append("};")
lines.append("typedef struct { const char *key; int min, max, def, kind; } pdef_t;   /* kind 0 value, 1 trigger, 2 text */")
lines.append("static const pdef_t PDEF[NUM_P] = {")
for p in P:
    if "options" in p:
        lo, hi, d, kind = 0, len(p["options"]) - 1, p["default"], 0
    elif p.get("momentary"):
        lo, hi, d, kind = 0, 1, 0, 1
    elif p.get("display") == "string":
        lo, hi, d, kind = 0, 0, 0, 2
    else:
        lo, hi, d, kind = p["min"], p["max"], p["default"], 0
    lines.append('    {"%s", %d, %d, %d, %d},' % (p["key"], lo, hi, d, kind))
lines.append("};")
lines.append("static const char *const TEMPLATE_NAMES[] = {%s};" % ", ".join('"%s"' % t for t in TEMPLATES))
lines.append("static const char *const RHYTHM_NAMES[] = {%s};" % ", ".join('"%s"' % t.replace('"', '\\"') for t in RHYTHMS))
lines.append("static const char *const SCALE_LABELS[] = {%s};" % ", ".join('"%s"' % t for t in SCALES))
lines.append("static const char *const KEY_LABELS[] = {%s};" % ", ".join('"%s"' % t for t in KEYS))
with open(os.path.join(ROOT, "src", "params_gen.h"), "w") as f:
    f.write("\n".join(lines) + "\n")
print("%d params" % len(P))
