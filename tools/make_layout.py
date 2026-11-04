#!/usr/bin/env python3
"""CheatChord 4.0: writes vst/layout.conf (the keyboards' 72 + 72 key lights are computed here, the rest is spelled out).
Layout y = image y + 86 (the plugin area starts 86 px down)."""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import gen_data  # noqa: E402

NM = gen_data.names()
WHITE = [0, 2, 4, 5, 7, 9, 11]
BLACKS = [(1, 1), (3, 2), (6, 4), (8, 5), (10, 6)]   # (semitone, white-key boundary index)


def keys(x, y, kw, bw, bh, kh, tag, when):
    """the light meters of a 6-octave keyboard drawn by make_art.keyboard_wide at (x, y)"""
    x0, y0 = 8, 4
    out = []
    wh = kh - bh - 8   # the lower, lit part of a white key
    wy = y + y0 + bh + 4 + wh // 2
    for o in range(6):
        for i, n in enumerate(WHITE):
            cx = x + x0 + (o * 7 + i) * kw + kw // 2
            out.append("meter cx=%d cy=%d w=%d h=%d key=kbx_%d strip=art/key%s_white.png frames=5 when=%s" %
                       (cx, wy, kw - 4, wh, o * 12 + n + 1, tag, when))
    by = y + y0 + 2 + (bh - 4) // 2
    for o in range(6):
        for n, b in BLACKS:
            cx = x + x0 + (o * 7 + b) * kw
            out.append("meter cx=%d cy=%d w=%d h=%d key=kbx_%d strip=art/key%s_black.png frames=5 when=%s" %
                       (cx, by, bw - 2, bh - 4, o * 12 + n + 1, tag, when))
    return out


def groups():
    n_basic, n_arp = 11, 5
    return "BASICS:%d,ARPEGGIOS:%d,GENRE GROOVES:%d" % (n_basic, n_arp, len(NM["rhythms"]) - n_basic - n_arp)


L = []
A = L.append
A("""# CheatChord 4.0 skin (written by tools/make_layout.py: edit that, not this file).
# Charcoal panels, cream type, gold for what is active; the logo is the composer medallion.
# Every choice with more than a handful of options is a list field: one tap opens ALL of its options at once
# (MPC's own list widget), a second tap or a pick closes it. Arrows beside a field still step through it.
style=td3
art_css=skin.css
label_scale=1.25
scale_names=1
theme_bg=141518
theme_box=1c1d21
theme_panel=1f2024
theme_ink=ece4d4
theme_ink_dim=a39c8e
theme_ink_faint=3a3832
theme_accent=d6a858
theme_accent_hi=f6d68c
theme_seg_active=d6a858
theme_seg_inactive=2a2b30
theme_seg_active_tx=17181b
theme_lcd=111215
theme_line=3a3c42
theme_btn_bg=d6a858
theme_btn_text=17181b
theme_display_ink=efd9a8
theme_knob_face=2a2c31
theme_knob_ring=d6a858
theme_knob_dot=f6d68c
theme_tab_on=d6a858
theme_tab_on_tx=17181b
theme_chrome_ink=ece4d4
theme_bar=1c1d21
theme_tile_on=4a2a1c
theme_btn_on=f6d68c
qlinks_track = key,scale,style,artist,producer,spice,producer_mix,level
""")

# ------------------------------------------------------------------------------------------------ CRUISE
A("[tab CRUISE]")
A("art file=art/bg.png fit=cover")
A("art file=art/logo_header.png x=10 y=88 w=208 h=64")
A("readout cx=426 cy=103 w=396 h=26 key=sel1 talign=center tsize=20")
A("readout cx=426 cy=133 w=396 h=26 key=sel2 talign=center tsize=16")
A("art file=art/keybed_s.png x=636 y=90 w=150 h=56")
for i, n in enumerate(WHITE):
    A("meter cx=%d cy=133 w=18 h=16 key=kb_%d strip=art/keys_white.png frames=5" % (651 + i * 20, n + 1))
for n, b in BLACKS:
    A("meter cx=%d cy=108 w=12 h=30 key=kb_%d strip=art/keys_black.png frames=5" % (641 + b * 20, n + 1))
A("readout cx=864 cy=118 w=140 h=30 key=shown talign=center tsize=18")
A('button cx=990 cy=118 w=100 h=40 label="KEYS" key=keys_open')
A('enum_h cx=1164 cy=118 sw=74 sh=36 label="" key=midi_out options="MIDI OFF,MIDI ON"')
A('frame x=24 y=156 w=1232 h=146 title="PROGRESSION"')
A('enum_h cx=322 cy=180 sw=90 sh=30 label="" key=gen_len options="4 CHORDS,8 CHORDS"')
A('enum_h cx=534 cy=180 sw=108 sh=30 label="" key=scope options="EDIT CHORD,EDIT ALL"')
A('button cx=728 cy=180 w=150 h=34 label="CRUISE" key=cruise')
A('button cx=858 cy=180 w=96 h=32 label="UNDO" key=undo')
A('button cx=962 cy=180 w=96 h=32 label="REDO" key=redo')
A('button cx=1072 cy=180 w=112 h=32 label="DELETE" key=delete')
A('button cx=1184 cy=180 w=96 h=32 label="CLEAR" key=clear')
A("list x=44 y=202 w=1192 h=58 cols=8 rows=1 th=58 gap=8 key=prog tsize=23")
A("readout cx=640 cy=282 w=1192 h=26 key=status talign=center tsize=19")
A('frame x=24 y=310 w=1232 h=352 title=""')
A('enum_h cx=470 cy=334 sw=140 sh=34 label="" key=panel options="IDEAS,HISTORY,BROWSER,BIG KEYS,PRESETS,MY PRESETS"')
A("readout cx=1150 cy=334 w=170 h=28 key=hist_pos talign=center tsize=16")
# BROWSER: every list one tap away, the keyboard and the influence knobs together
W = "panel:browser"
A('popup cx=99 cy=394 w=110 h=42 label="KEY" key=key when=%s' % W)
A('popup cx=259 cy=394 w=190 h=42 label="SCALE" key=scale when=%s' % W)
A('popup cx=489 cy=394 w=250 h=42 label="STYLE (%d)" key=style cols=6 cw=150 when=%s' % (len(NM["styles"]), W))
A('popup cx=774 cy=394 w=300 h=42 label="ARTIST (%d)" key=artist cols=10 cw=124 when=%s' % (len(NM["artists"]) - 1, W))
A('popup cx=1084 cy=394 w=300 h=42 label="PRODUCER (%d)" key=producer cols=9 cw=136 when=%s' % (len(NM["producers"]) - 1, W))
A("art file=art/keybed_br.png x=44 y=428 w=902 h=122 when=%s" % W)
L.extend(keys(44, 428, 21, 13, 70, 114, "r", W))
A("readout cx=495 cy=568 w=902 h=28 key=kb_diff talign=center tsize=17 when=%s" % W)
for cx, cy, lab, key in ((1028, 470, "SPICE", "spice"), (1168, 470, "STYLE", "style_mix"),
                         (1028, 572, "ARTIST", "artist_mix"), (1168, 572, "PRODUCER", "producer_mix")):
    A('text cx=%d cy=%d size=1.35 align=center color=a39c8e label="%s" when=%s' % (cx, cy - 40, lab, W))
    A('knob cx=%d cy=%d r=20 label="%s" key=%s lay=side bw=128 bh=54 vs=22 when=%s' % (cx, cy, lab, key, W))
# IDEAS / HISTORY
A("list x=44 y=356 w=912 h=200 cols=4 rows=2 th=96 gap=8 key=sug tsize=26 when=panel:ideas")
A('button cx=1106 cy=400 w=250 h=80 label="ADD" key=add when=panel:ideas')
A('button cx=1106 cy=508 w=250 h=80 label="REROLL" key=reroll when=panel:ideas')
A("list x=44 y=356 w=1192 h=232 cols=2 rows=4 th=52 gap=8 key=hist tsize=19 order=cols when=panel:history")
# BIG KEYS
A("readout cx=250 cy=372 w=400 h=30 key=kb_name talign=center tsize=22 when=panel:3")
A("readout cx=770 cy=372 w=620 h=30 key=notes talign=center tsize=19 when=panel:3")
A("readout cx=1164 cy=372 w=144 h=30 key=kb_range talign=center tsize=14 when=panel:3")
A("art file=art/keybed_big.png x=44 y=394 w=1192 h=150 when=panel:3")
L.extend(keys(44, 394, 28, 16, 88, 142, "b", "panel:3"))
A("readout cx=640 cy=572 w=1192 h=28 key=kb_diff talign=center tsize=16 when=panel:3")
# PRESETS: the 24 factory presets as big tiles; MY PRESETS: the 16 own slots and saving
A('list x=44 y=356 w=1192 h=246 cols=6 rows=4 th=57 gap=6 key=pre tsize=21 when=panel:4')
A('text cx=50 cy=616 size=1.45 align=left color=a39c8e label="TAP = LOAD (UNDO BRINGS YOUR STATE BACK). ALSO IN MPC\'S PRESET MENU AND THE INSTRUMENTS BROWSER" when=panel:4')
A('list x=44 y=356 w=820 h=246 cols=4 rows=4 th=57 gap=6 key=usr tsize=18 when=panel:5')
A('text cx=1050 cy=372 size=1.6 align=center color=d6a858 label="SAVE THIS STATE AS" when=panel:5')
A('button cx=928 cy=420 w=60 h=50 label="<" key=save_slot_prev when=panel:5')
A("readout cx=1050 cy=420 w=176 h=50 key=save_slot_name talign=center tsize=15 when=panel:5")
A('button cx=1172 cy=420 w=60 h=50 label=">" key=save_slot_next when=panel:5')
A('button cx=1050 cy=500 w=300 h=70 label="SAVE PRESET" key=save_preset when=panel:5')
A('text cx=1050 cy=566 size=1.3 align=center color=a39c8e label="MPC DOCUMENTS / CHEATCHORD / PRESETS" when=panel:5')
A('text cx=1050 cy=592 size=1.3 align=center color=a39c8e label="TAP A SLOT ON THE LEFT TO LOAD IT" when=panel:5')
A("readout cx=640 cy=642 w=1192 h=26 key=info talign=center tsize=18")
A('button cx=132 cy=690 w=224 h=44 label="GENERATE" key=generate')
A('enum_h cx=500 cy=690 sw=110 label="" key=listen options="OFF,MPC,LISTEN"')
A('button cx=790 cy=690 w=170 h=44 label="PLAY" key=run')
A('button cx=966 cy=690 w=150 h=44 label="STOP" key=stop')
A("readout cx=1150 cy=690 w=180 h=36 key=now talign=center tsize=15")
A('qlinks "CRUISE" = key,scale,style,artist,producer,spice,producer_mix,scope')
A("")

# ------------------------------------------------------------------------------------------------ PLAY
durs = ",".join(d.upper() for d in NM["durs"])
A("[tab PLAY]")
A("art file=art/bg.png fit=cover")
A('frame x=24 y=96 w=1232 h=176 title="PLAYBACK AND LOOP SYNC"')
A('enum_h cx=150 cy=176 sw=86 label="PLAY MODE" key=listen options="OFF,MPC,LISTEN"')
A('enum_h cx=820 cy=176 sw=78 label="LOOP SYNC (CHORD LENGTHS FOLLOW THE MPC LOOP)" key=loop_sync '
  'options="MANUAL,AUTO,1 BAR,2 BARS,4 BARS,8 BARS,16 BARS,32 BARS"')
A("readout cx=640 cy=214 w=1192 h=26 key=mode_help talign=center tsize=16")
A("readout cx=640 cy=248 w=1192 h=28 key=loop_info talign=center tsize=19")
A('frame x=24 y=280 w=1232 h=116 title="TIMING"')
A('button cx=70 cy=356 w=50 h=44 label="<" key=bars_prev')
A('popup cx=230 cy=356 w=250 h=44 label="CHORD LENGTH (MANUAL)" key=bars')
A('button cx=390 cy=356 w=50 h=44 label=">" key=bars_next')
A('enum_h cx=790 cy=356 sw=100 label="DIRECTION" key=direction options="FORWARD,REVERSE,PING-PONG,RANDOM"')
A('toggle cx=1150 cy=344 label="PUSH" key=push')
A('frame x=24 y=404 w=1232 h=124 title="RHYTHM AND GROOVE"')
A('button cx=70 cy=482 w=50 h=44 label="<" key=rhythm_prev')
A('popup cx=240 cy=482 w=270 h=44 label="RHYTHM / GENRE (%d)" key=rhythm groups="%s" cw=150' % (len(NM["rhythms"]), groups()))
A('button cx=410 cy=482 w=50 h=44 label=">" key=rhythm_next')
A('popup cx=560 cy=482 w=170 h=44 label="RHYTHM RATE" key=rrate cols=6 cw=110')
A('popup cx=760 cy=482 w=170 h=44 label="ARP RATE" key=rate cols=6 cw=110')
A('enum_h cx=960 cy=482 sw=50 label="ARP OCT" key=arp_oct options="1,2,3"')
A('text cx=1160 cy=448 size=1.5 align=center label="SWING"')
A('knob cx=1160 cy=486 r=20 label="SWING" key=swing lay=side bw=150 bh=54 vs=22')
A('frame x=24 y=536 w=1232 h=126 title="FEEL"')
A('knob cx=110 cy=584 r=16 label="STRUM" key=strum ns=15 vs=15 bw=120')
A('enum_h cx=440 cy=600 sw=90 label="STRUM DIRECTION" key=strum_mode options="UP,DOWN,ALTERNATE,GUITAR,RANDOM"')
A('knob cx=770 cy=584 r=16 label="GATE" key=gate ns=15 vs=15 bw=120')
A('knob cx=900 cy=584 r=16 label="VELOCITY" key=velocity ns=15 vs=15 bw=120')
A('knob cx=1025 cy=584 r=16 label="HUMANIZE" key=humanize ns=15 vs=15 bw=116')
A('toggle cx=1185 cy=596 label="MUTE" key=mute bw=100')
A('button cx=500 cy=690 w=170 h=36 label="PLAY" key=run')
A('button cx=680 cy=690 w=150 h=36 label="STOP" key=stop')
A("readout cx=1000 cy=690 w=420 h=32 key=now talign=center tsize=17")
A('qlinks "PLAY" = rhythm,rrate,swing,bars,loop_sync,rate,gate,velocity')
A("")

# ------------------------------------------------------------------------------------------------ VOICE
A("[tab VOICE]")
A("art file=art/bg.png fit=cover")
A('frame x=24 y=96 w=1232 h=118 title="VOICING"')
A('enum_h cx=350 cy=174 sw=100 label="VOICING" key=voicing options="CLOSE,OPEN,DROP 2,SPREAD,NEO SOUL,SHELL"')
A('enum_h cx=900 cy=174 sw=64 label="OCTAVE" key=octave options="-2,-1,0,+1,+2"')
A('frame x=24 y=222 w=1232 h=106 title="SMOOTHING"')
A('toggle cx=200 cy=282 label="VOICE LEADING" key=lead')
A('toggle cx=470 cy=282 label="BASS NOTE" key=bass')
A('text cx=620 cy=292 size=1.9 align=left label="VOICE LEADING MOVES EACH CHORD AS LITTLE AS POSSIBLE."')
A('frame x=24 y=336 w=1232 h=106 title="PROGRESSION"')
A("list x=44 y=372 w=1192 h=60 cols=8 rows=1 th=60 gap=8 key=prog tsize=24")
A('frame x=24 y=450 w=1232 h=258 title="EDIT THE CHORD AT THE CURSOR"')
A('enum_h cx=1100 cy=474 sw=118 sh=28 label="" key=scope options="EDIT CHORD,EDIT ALL"')
A('text cx=470 cy=478 size=1.4 align=left color=a39c8e label="EDIT ALL: ROOT MOVES ALL, TYPE / REVERSE SET ALL"')
A('stepper cx=234 cy=550 w=380 h=54 label="CHORD" key=cursor get=cursor_name')
for cx, lab, key, extra in ((640, "ROOT", "slot_root", ""), (1046, "TYPE", "slot_type", " cols=6 cw=120")):
    A('button cx=%d cy=550 w=50 h=54 label="<" key=%s_prev' % (cx - 165, key))
    A('popup cx=%d cy=550 w=270 h=54 label="%s" key=%s%s' % (cx, lab, key, extra))
    A('button cx=%d cy=550 w=50 h=54 label=">" key=%s_next' % (cx + 165, key))
A('button cx=69 cy=654 w=50 h=54 label="<" key=slot_bass_prev')
A('popup cx=234 cy=654 w=270 h=54 label="BASS" key=slot_bass')
A('button cx=399 cy=654 w=50 h=54 label=">" key=slot_bass_next')
A('enum_h cx=640 cy=660 sw=76 label="VARIATION" key=slot_var options="SMOOTH,ALT 1,ALT 2,ALT 3"')
A('enum_h cx=950 cy=660 sw=86 label="LENGTH" key=slot_len options="HALF,NORMAL,DOUBLE"')
A('enum_h cx=1165 cy=660 sw=70 label="REVERSE" key=slot_rev options="OFF,ON"')
A('qlinks "VOICE" = voicing,octave,lead,bass,cursor,slot_root,slot_type,slot_bass')
A("")

# ------------------------------------------------------------------------------------------------ SOUND
A("[tab SOUND]")
A("art file=art/bg.png fit=cover")
A('frame x=24 y=96 w=1232 h=158 title="PREVIEW SYNTH"')
A('toggle cx=130 cy=178 label="SYNTH + MIDI" key=synth')
A('popup cx=390 cy=190 w=240 h=48 label="SOUND" key=sound')
A('knob cx=620 cy=168 r=36 label="LEVEL" key=level')
A('knob cx=800 cy=168 r=36 label="TONE" key=tone')
A('knob cx=980 cy=168 r=36 label="RELEASE" key=release')
A('knob cx=1160 cy=168 r=36 label="SPACE" key=space')
A('frame x=24 y=262 w=1232 h=160 title="MIDI OUT"')
A('stepper cx=234 cy=358 w=380 h=52 label="CHORDS CHANNEL" key=channel')
A('stepper cx=640 cy=358 w=380 h=52 label="BASS CHANNEL" key=bass_ch get=bass_ch_name')
A('button cx=1060 cy=358 w=280 h=52 label="ALL NOTES OFF" key=panic')
A("readout cx=640 cy=404 w=1192 h=26 key=port talign=center tsize=18")
A('frame x=24 y=430 w=1232 h=138 title="PADS  (1-8: PROGRESSION, 9-16: IDEAS)"')
A('button cx=150 cy=520 w=220 h=56 label="LEARN PADS" key=learn')
A("readout cx=560 cy=508 w=560 h=32 key=learn_info talign=center tsize=17")
A("readout cx=560 cy=544 w=560 h=28 key=last_note talign=center tsize=15")
A('enum_h cx=1080 cy=528 sw=120 label="PAD VELOCITY" key=vel_mode options="PLAYED,FIXED"')
A('frame x=24 y=576 w=1232 h=132 title="EXPORT"')
A('button cx=180 cy=654 w=270 h=52 label="EXPORT MIDI" key=export')
A("readout cx=760 cy=654 w=860 h=40 key=export_status talign=center tsize=20")
A('qlinks "SOUND" = sound,level,tone,release,space,channel,bass_ch,vel_mode')
A("")

# ------------------------------------------------------------------------------------------------ LIBRARY
A("[tab LIBRARY]")
A("art file=art/bg.png fit=cover")
A('frame x=24 y=96 w=1232 h=150 title="PROGRESSION TEMPLATES"')
A('button cx=70 cy=176 w=50 h=50 label="<" key=template_prev')
A('popup cx=290 cy=176 w=370 h=50 label="TEMPLATE" key=template cols=4 cw=200')
A('button cx=510 cy=176 w=50 h=50 label=">" key=template_next')
A('button cx=720 cy=176 w=200 h=50 label="LOAD" key=load')
A('text cx=850 cy=176 size=1.8 align=left label="REPLACES IT (UNDO BRINGS IT BACK)"')
A("readout cx=640 cy=224 w=1192 h=30 key=tpl_info talign=center tsize=22")
A('frame x=24 y=254 w=1232 h=454 title="ARTIST AND PRODUCER LIBRARY"')
A('enum_h cx=200 cy=318 sw=160 sh=40 label="" key=lib_kind options="ARTISTS,PRODUCERS"')
A('popup cx=200 cy=410 w=330 h=54 label="ARTIST (%d)" key=artist cols=10 cw=124' % (len(NM["artists"]) - 1))
A('popup cx=200 cy=510 w=330 h=54 label="PRODUCER (%d)" key=producer cols=9 cw=136' % (len(NM["producers"]) - 1))
A('text cx=110 cy=572 size=1.4 align=center color=a39c8e label="ARTIST MIX"')
A('knob cx=110 cy=622 r=20 label="ARTIST MIX" key=artist_mix lay=side bw=140 bh=54 vs=22')
A('text cx=290 cy=572 size=1.4 align=center color=a39c8e label="PRODUCER MIX"')
A('knob cx=290 cy=622 r=20 label="PRODUCER MIX" key=producer_mix lay=side bw=140 bh=54 vs=22')
A("readout cx=815 cy=312 w=820 h=44 key=prof_1 talign=left tsize=23")
for i, cy in enumerate((368, 418, 468, 530, 580, 630), 2):
    A("readout cx=815 cy=%d w=820 h=42 key=prof_%d talign=left tsize=%d" % (cy, i, 16 if i == 7 else 18))
A('text cx=410 cy=684 size=1.4 align=left color=a39c8e label="READ FROM THE NUMBERS THE CHORD ENGINE USES. ONE TAP ON A FIELD SHOWS ALL NAMES."')
A('qlinks "LIBRARY" = template,artist,producer,artist_mix,producer_mix')
A("")

# ------------------------------------------------------------------------------------------------ INFO
A("[tab INFO]")
A("art file=art/bg.png fit=cover")
A("art file=art/logo_big.png x=26 y=96 w=560 h=168")
A('text cx=612 cy=150 size=2.5 align=left label="CHEATCHORD 4.0"')
A('text cx=612 cy=188 size=1.7 align=left label="HARMONY CHEAT CODES FOR THE MPC"')
A("art file=art/jf_velvet.png x=1086 y=92 w=150 h=200")
INFO = [
    (50, 300, "QUICK GUIDE", [
        "1  BROWSER: ONE TAP ON A FIELD SHOWS EVERY OPTION.",
        "2  TAP A CHORD = PREVIEW. CRUISE = THE NEXT CHORD, NEXT SLOT.",
        "3  IDEAS: TAP = HEAR (ONE MUTE GROUP), TAP AGAIN = KEEP.",
        "4  UNDO / REDO / HISTORY: EVERY STATE STAYS.",
        "5  PRESETS: 24 FACTORY + 16 OF YOUR OWN, ALSO IN MPC'S MENU."]),
    (50, 476, "INFLUENCE + KEYS", [
        "TURN SPICE / STYLE / ARTIST / PRODUCER WHILE IT PLAYS:",
        "THE BROWSER KEYS SHOW NEW NOTES GREEN, GONE NOTES RED.",
        "A KNOB BACK = THE SAME CHORDS BACK."]),
    (50, 600, "PADS", [
        "LEARN: HIT THE FIRST PAD, THEN THE LAST - ANY BANK.",
        "1-8 PLAY THE PROGRESSION, 9-16 THE IDEAS."]),
    (660, 300, "LOOP SYNC + LENGTHS", [
        "AUTO: THE CHORDS SHARE THE MPC LOOP. CHORD LENGTH,",
        "RHYTHM RATE, ARP RATE: 1/64 TRIPLET UP TO 16 BARS."]),
    (660, 398, "RHYTHM", [
        "%d RHYTHMS: BASICS, ARPS AND %d GENRE GROOVES." % (len(NM["rhythms"]), len(NM["rhythms"]) - 16),
        "SWING ACTS AT ONCE, ALSO WHILE IT PLAYS."]),
    (660, 496, "EDIT ALL (VOICE TAB)", [
        "ROOT MOVES EVERY CHORD, TYPE SETS EVERY CHORD.",
        "VARIATION AND LENGTH SET EVERY CHORD."]),
    (660, 594, "PROFILES", [
        "EACH LIBRARY ENTRY SAYS WHAT IT DOES AND WHAT IT IS BASED ON.",
        "NO SONG IS COPIED."]),
]
for x, y, title, lines in INFO:
    A('text cx=%d cy=%d size=2.0 align=left color=d6a858 label="%s"' % (x, y, title))
    for k, t in enumerate(lines):
        A('text cx=%d cy=%d size=1.5 align=left label="%s"' % (x, y + 30 + 26 * k, t))
A('text cx=50 cy=694 size=1.4 align=left color=a39c8e label="%d STYLES - %d ARTISTS - %d PRODUCERS - %d RHYTHMS - %d PRESETS - 26 TEMPLATES - 30 CHORD TYPES - MIDI EXPORT"' %
  (len(NM["styles"]), len(NM["artists"]) - 1, len(NM["producers"]) - 1, len(NM["rhythms"]), len(NM["presets"])))

open(os.path.join(ROOT, "vst", "layout.conf"), "w").write("\n".join(L) + "\n")
print("layout: %d lines" % len(L))
