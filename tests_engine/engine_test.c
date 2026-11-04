/* CheatChord functional test: drives the engine like MPC does (params, taps, transport, pads, echo) and
 * checks what comes out of the MIDI port and the synth. Build: tests/run.sh */
#include "engine.h"
#include "params_gen.h"
#include "seq_out.h"
#include "theory.h"
#include "voicing.h"
#include "harmony.h"
#include "refs.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cp_test_count(void *inst);
int cp_test_pool(void *inst);
void cp_test_wait(void *inst);

#include "artists.h"
static int fails;
/* 4.0: the ARTIST / PRODUCER options are alphabetical; tests name the 3.x list entries by their old index */
static int oldA(int i) { return artist_list_index(OLD_ARTIST_LIST[i]); }
static int oldP(int i) { return producer_list_index(OLD_PRODUCER_LIST[i]); }
#define CHECK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef struct { long frame; int ch, note, vel; } out_t;
static out_t OUT[200000];
static int nout;
static long cur_frame;
static void sink(int ch, int note, int vel) { if (nout < 200000) OUT[nout++] = (out_t){cur_frame, ch, note, vel}; }

static const mpc_engine_t *E;
static void set(const char *k, const char *v);
static void seti(const char *k, int v) { char b[16]; snprintf(b, sizeof b, "%d", v); set(k, b); }
static void *S;
static double ppq;
static int playing;
static double bpm = 120;
static double rms_acc;
static long rms_n;
static int peak;

static void block(void) {
    int16_t buf[256];
    E->song_pos(S, playing ? ppq : -1, bpm, playing, 128);
    E->render(S, buf, 128);
    for (int i = 0; i < 256; i++) { rms_acc += (double)buf[i] * buf[i]; if (abs(buf[i]) > peak) peak = abs(buf[i]); }
    rms_n += 256;
    cur_frame += 128;
    if (playing) ppq += 128 * bpm / 60.0 / 44100.0;
}
static long ms_to_frames(int ms) { return ms * 441L / 10; }
static void run_ms(int ms) { for (int i = 0; i < ms * 441 / 1280 + 1; i++) block(); }
static void set(const char *k, const char *v) { E->set_param(S, k, v); }
static const char *get(const char *k) { static char b[256]; b[0] = 0; E->get_param(S, k, b, sizeof b); return b; }
static void tap(const char *k) { set(k, "1"); run_ms(450); }
/* a chord tile without its marks ("> " selected, "* " target) */
static const char *bare(const char *k) { static char b[64]; const char *v = get(k); while (*v == '>' || *v == '*' || *v == ' ') v++; snprintf(b, sizeof b, "%s", v); return b; }

int main(void) {
    seq_out_set_sink(sink);
    E = mpc_engine();
    S = E->create(NULL);
    CHECK(S != NULL, "create");
    /* desktop build: MIDI OUT + synth on and channel 1 by default; the checks below were written for MPC's defaults */
    CHECK(atoi(get("midi_out")) == 1 && atoi(get("synth")) == 1 && atoi(get("channel")) == 1, "desktop defaults: MIDI out + synth, ch 1");
    E->set_param(S, "midi_out", "0"); E->set_param(S, "synth", "0"); E->set_param(S, "channel", "2");
    cp_test_wait(S);
    run_ms(800);   /* quiet period after create */
    set("panel", "0");   /* the IDEAS info line (the browser is the default panel) */
    set("loop_sync", "0");   /* the 1.x / 2.x checks below are about CHORD LENGTH (manual); LOOP SYNC has its own */
    /* MIDI OUT off by default: taps sound on the synth only */
    { int b0 = nout; tap("prog_1"); CHECK(nout == b0, "MIDI OUT off: nothing on the port"); }
    set("midi_out", "1"); run_ms(30);
    CHECK(cp_test_pool(S) == 8, "first pool has 8 ideas (%d)", cp_test_pool(S));
    printf("     ideas: ");
    for (int i = 1; i <= 8; i++) { char k[16]; snprintf(k, sizeof k, "sug_%d", i); printf("[%s] ", get(k)); }
    printf("\n");

    /* tap idea 1: audition, lit */
    int before = nout;
    tap("sug_1");
    CHECK(nout > before, "tapping an idea plays it on the MIDI port (%d events)", nout - before);
    CHECK(!strcmp(get("sug_1_on"), "1"), "the tapped idea is lit");
    printf("     info: %s\n", get("info"));
    /* tap again: added */
    tap("sug_1");
    CHECK(cp_test_count(S) == 1, "second tap adds it (count %d)", cp_test_count(S));
    cp_test_wait(S);
    printf("     prog_1: [%s]  status: %s\n", get("prog_1"), get("status"));
    printf("     next ideas: ");
    for (int i = 1; i <= 8; i++) { char k[16]; snprintf(k, sizeof k, "sug_%d", i); printf("[%s] ", get(k)); }
    printf("\n");
    /* ADD button adds the selected (or best) idea */
    tap("sug_3");
    set("add", "1"); run_ms(50); set("add", "0"); cp_test_wait(S);
    CHECK(cp_test_count(S) == 2, "ADD keeps the selected idea (count %d)", cp_test_count(S));
    /* reroll changes the ideas */
    char a[512] = "", b[512] = "";
    for (int i = 1; i <= 8; i++) { char k[16]; snprintf(k, sizeof k, "sug_%d", i); strcat(a, get(k)); strcat(a, "|"); }
    set("reroll", "1"); run_ms(30); cp_test_wait(S); run_ms(30);
    for (int i = 1; i <= 8; i++) { char k[16]; snprintf(k, sizeof k, "sug_%d", i); strcat(b, get(k)); strcat(b, "|"); }
    CHECK(strcmp(a, b) != 0, "REROLL deals new ideas (%s -> %s)", a, b);

    /* generate */
    set("style", "2"); set("gen_len", "1");
    set("generate", "1"); run_ms(20); cp_test_wait(S); run_ms(20);
    CHECK(cp_test_count(S) == 8, "GENERATE (8) writes 8 chords: %s", get("status"));
    run_ms(3000);
    printf("     status: %s\n", get("status"));

    /* transport: 4 bars at 120 bpm, 1 bar per chord, sustain rhythm */
    set("bars", "12"); set("rhythm", "0");
    int n0 = nout;
    playing = 1; ppq = 0;
    for (int i = 0; i < (int)(4 * 4 * 60.0 / 120 * 44100 / 128); i++) block();
    playing = 0; block(); run_ms(50);
    int ons = 0, offs = 0, first_on_frame = -1;
    long chord_starts[16]; int ncs = 0; long lastf = -100000;
    for (int i = n0; i < nout; i++) {
        if (OUT[i].vel) { ons++; if (first_on_frame < 0) first_on_frame = (int)(OUT[i].frame); if (OUT[i].frame - lastf > 4410 && ncs < 16) chord_starts[ncs++] = OUT[i].frame; lastf = OUT[i].frame; }
        else offs++;
    }
    CHECK(ons > 0 && ons == offs, "transport: every note-on has its note-off (%d ons, %d offs)", ons, offs);
    printf("     chord starts (frames): ");
    for (int i = 0; i < ncs; i++) printf("%ld ", chord_starts[i] - chord_starts[0]);
    printf("\n");
    CHECK(ncs >= 4 && labs((chord_starts[1] - chord_starts[0]) - 88200) <= 128, "chords change every bar (88200 frames at 120 bpm)");

    /* rhythm: eighths + arp + swing + loop wrap */
    set("rhythm", "4"); set("swing", "66");
    n0 = nout; playing = 1; ppq = 0;
    for (int loop = 0; loop < 3; loop++) {
        ppq = 0;
        for (int i = 0; i < (int)(2 * 4 * 60.0 / 120 * 44100 / 128); i++) block();
    }
    playing = 0; block(); run_ms(50);
    ons = offs = 0;
    for (int i = n0; i < nout; i++) { if (OUT[i].vel) ons++; else offs++; }
    CHECK(ons == offs && ons > 0, "eighths with loop wraps: ons %d == offs %d", ons, offs);
    set("rhythm", "11"); set("rate", "5");
    n0 = nout; playing = 1; ppq = 0;
    for (int i = 0; i < (int)(4 * 60.0 / 120 * 44100 / 128); i++) block();
    playing = 0; block(); run_ms(50);
    ons = 0;
    for (int i = n0; i < nout; i++) if (OUT[i].vel) ons++;
    CHECK(ons >= 16 && ons <= 20, "arp 1/16 over one bar: %d notes (16 + bass)", ons);

    /* pads: pad 1 plays slot 1 while held, echo of our own notes is ignored */
    set("rhythm", "0");
    n0 = nout;
    int pad_start = n0;
    uint8_t on[3] = {0x90, 36, 100}, off[3] = {0x80, 36, 0};
    E->midi(S, on, 3);
    run_ms(10);
    int pad_ons = 0;
    for (int i = n0; i < nout; i++) if (OUT[i].vel) pad_ons++;
    CHECK(pad_ons >= 3, "pad 1 plays chord 1 (%d notes)", pad_ons);
    /* desktop: no echo guard (a DAW does not route the plugin's MIDI output back into it) */
    E->midi(S, off, 3);
    run_ms(100);
    ons = offs = 0;
    for (int i = pad_start; i < nout; i++) { if (OUT[i].vel) ons++; else offs++; }
    CHECK(ons == offs, "pad release ends its notes (%d/%d)", ons, offs);

    /* state round trip */
    char st[8192];
    E->get_param(S, "state", st, sizeof st);
    void *S2 = E->create(NULL);
    E->set_param(S2, "state", st);
    char st2[8192];
    E->get_param(S2, "state", st2, sizeof st2);
    CHECK(!strcmp(st, st2), "state restores exactly (%d bytes)", (int)strlen(st));
    char p1[256], p2[256];
    E->get_param(S, "status", p1, sizeof p1);
    E->get_param(S2, "status", p2, sizeof p2);
    E->destroy(S2);

    /* edit slot controls */
    set("cursor", "2");
    run_ms(20);
    printf("     edit: %s | %s | %s | %s\n", get("cursor_name"), get("slot_root_name"), get("slot_type_name"), get("slot_bass_name"));
    set("slot_type", "18"); run_ms(20);
    CHECK(strstr(get("prog_2"), "maj9") != NULL, "TYPE stepper changes chord 2: %s", get("prog_2"));

    /* templates */
    set("template", "17"); set("load", "1"); run_ms(30);
    CHECK(cp_test_count(S) == 4, "template Andalusian loads 4 chords: %s", get("status"));
    run_ms(3000);
    printf("     %s / scale %s\n", get("status"), get("scale"));

    /* key change transposes the progression */
    char before_t[64]; snprintf(before_t, sizeof before_t, "%s", get("prog_1"));
    set("key", "2"); run_ms(30); cp_test_wait(S); run_ms(10);
    printf("     transpose: %s -> %s\n", before_t, get("prog_1"));
    CHECK(strcmp(before_t, get("prog_1")) != 0, "KEY change transposes the progression");

    /* export */
    set("export", "1"); run_ms(20); cp_test_wait(S); run_ms(20);
    printf("     export: %s\n", get("export_status"));
    CHECK(strstr(get("export_status"), "SAVED") != NULL, "EXPORT MIDI writes a file");

    /* LISTEN plays the progression without MPC's transport */
    run_ms(1600);   /* let the audition of the key change end first */
    n0 = nout;
    set("listen", "2"); set("run", "1");
    for (int i = 0; i < (int)(2 * 4 * 60.0 / 120 * 44100 / 128); i++) block();
    set("stop", "1"); run_ms(50);
    ons = offs = 0;
    for (int i = n0; i < nout; i++) { if (OUT[i].vel) ons++; else offs++; }
    CHECK(ons > 0 && ons == offs, "LISTEN plays without the transport (%d ons, %d offs)", ons, offs);

    /* ---- 2.0 acceptance ---- */
    {
        set("template", "0"); set("load", "1"); run_ms(40); cp_test_wait(S); run_ms(40);
        char pool_before[512] = "";
        for (int i = 1; i <= 8; i++) { char k[16]; snprintf(k, sizeof k, "sug_%d", i); strcat(pool_before, get(k)); strcat(pool_before, "|"); }
        char prog_before[256]; snprintf(prog_before, sizeof prog_before, "%s", get("status"));
        int b0 = nout;
        for (int k = 0; k < 7; k++) { tap(k % 2 ? "prog_2" : "prog_3"); cp_test_wait(S); }
        char pool_after[512] = "";
        for (int i = 1; i <= 8; i++) { char k[16]; snprintf(k, sizeof k, "sug_%d", i); strcat(pool_after, get(k)); strcat(pool_after, "|"); }
        CHECK(nout > b0, "previewing progression chords plays them");
        CHECK(!strcmp(pool_before, pool_after), "7 previews: the ideas did not change (%s)", pool_after);
        CHECK(cp_test_count(S) == 4, "previews leave the progression alone");
        /* REROLL is the explicit action: it now gives ideas for the selected chord 3 */
        set("reroll", "1"); run_ms(30); cp_test_wait(S); run_ms(30);
        printf("     info after reroll: %s\n", get("info"));
        CHECK(strstr(get("info"), "REPLACE CHORD 3") != NULL, "REROLL on a selected chord: ideas to replace it");
        char c3[64]; snprintf(c3, sizeof c3, "%s", get("prog_3"));
        tap("sug_2"); tap("sug_2"); cp_test_wait(S);
        printf("     chord 3: %s -> %s\n", c3, get("prog_3"));
        CHECK(strcmp(c3, get("prog_3")) != 0 && cp_test_count(S) == 4, "an idea replaced chord 3, still 4 chords");
        set("undo", "1"); run_ms(30);
        CHECK(!strcmp(c3, get("prog_3")), "UNDO brings chord 3 back (%s)", get("prog_3"));
        set("redo", "1"); run_ms(30);
        CHECK(strcmp(c3, get("prog_3")) != 0, "REDO applies it again (%s)", get("prog_3"));
        printf("     history: [%s] [%s] [%s]\n", get("hist_1"), get("hist_2"), get("hist_3"));
        /* history tap goes back several steps */
        set("generate", "1"); run_ms(20); cp_test_wait(S); run_ms(20);
        char genp[256]; snprintf(genp, sizeof genp, "%s", get("status"));
        tap("hist_3");
        CHECK(strstr(get("status"), "ACTIVE: #") != NULL, "a history line restores an older state: %s", get("status"));
        CHECK(strstr(get("hist_1"), "GENERATE") != NULL, "the newer GENERATE stays in the history: %s", get("hist_1"));
        /* 4 / 8: nothing deleted */
        set("gen_len", "1"); run_ms(20);
        set("generate", "1"); run_ms(20); cp_test_wait(S); run_ms(20);
        CHECK(cp_test_count(S) == 8, "8 CHORDS: generate writes 8");
        set("gen_len", "0"); run_ms(20);
        CHECK(cp_test_count(S) == 8 && get("prog_6")[0] == '(', "4 CHORDS keeps chords 5-8, greyed: %s", get("prog_6"));
        printf("     %s\n", get("status"));
        int n0b = nout; playing = 1; ppq = 0;
        long starts = 0, lastf = -100000;
        set("rhythm", "0");
        for (int i = 0; i < (int)(8 * 4 * 60.0 / 120 * 44100 / 128); i++) block();
        playing = 0; block(); run_ms(30);
        for (int i = n0b; i < nout; i++) if (OUT[i].vel && OUT[i].frame - lastf > 4410) { starts++; lastf = OUT[i].frame; }
        CHECK(starts == 8, "4-chord loop over 8 bars: %ld chord starts (2 rounds)", starts);
        set("gen_len", "1"); run_ms(20);
        /* LISTEN: own PLAY/STOP, MPC PLAY takes over, MPC STOP ends everything */
        set("listen", "2"); set("run", "1");
        int l0 = nout;
        for (int i = 0; i < 400; i++) block();
        CHECK(nout > l0, "LISTEN PLAY plays");
        playing = 1; ppq = 0;
        for (int i = 0; i < 400; i++) block();
        CHECK(!strcmp(get("run"), "0"), "MPC PLAY takes over from LISTEN's own PLAY");
        playing = 0; block(); run_ms(200);
        int l1 = nout;
        for (int i = 0; i < 2000; i++) block();
        int ons_after = 0;
        for (int i = l1; i < nout; i++) if (OUT[i].vel) ons_after++;
        CHECK(ons_after == 0, "after MPC STOP nothing keeps playing (%d notes)", ons_after);
        set("run", "1"); for (int i = 0; i < 300; i++) block();
        E->set_param(S, "mains", "0");   /* MPC STOP while its transport was already stopped */
        block(); run_ms(100);
        int l2 = nout;
        for (int i = 0; i < 2000; i++) block();
        ons_after = 0;
        for (int i = l2; i < nout; i++) if (OUT[i].vel) ons_after++;
        CHECK(ons_after == 0 && !strcmp(get("run"), "0"), "MPC STOP also stops LISTEN on its own clock");
        /* artists change the result */
        seti("artist", oldA(5)); run_ms(20);
        printf("     artist: %s / %s\n", get("sel2"), get("artist_info"));
        CHECK(!strcmp(get("style"), "7"), "choosing an artist picks its usual style (Drake -> 90s R&B)");
        set("artist", "0");
        /* keyboard display */
        tap("prog_1");
        int lit = 0; char kk[8];
        for (int i = 1; i <= 12; i++) { snprintf(kk, sizeof kk, "kb_%d", i); if (atoi(get(kk))) lit++; }
        CHECK(lit >= 3, "keyboard shows the previewed chord (%d keys lit, %s)", lit, get("shown"));
        /* history survives saving the project */
        char st3[8192]; E->get_param(S, "state", st3, sizeof st3);
        void *S3 = E->create(NULL); E->set_param(S3, "state", st3);
        char h1[200], h2[200]; E->get_param(S, "hist_2", h1, sizeof h1); E->get_param(S3, "hist_3", h2, sizeof h2);
        printf("     state %d bytes; saved: %s / restored: %s\n", (int)strlen(st3), h1, h2);
        CHECK(strlen(st3) < 8000 && strstr(h2, "#") != NULL, "history is saved with the project");
        E->destroy(S3);
        set("listen", "1");
    }


    /* ================================================================ 3.0 acceptance */
    {
        printf("---- 3.0\n");
        /* the browser lists match the profile table */
        extern const int ARTIST_LIST[], PRODUCER_LIST[];
        set("listen", "0"); set("scope", "0"); set("artist", "0"); set("producer", "0"); set("style", "0"); run_ms(50);
        set("gen_len", "0"); run_ms(20);
        set("key", "0"); set("scale", "0"); run_ms(20);
        set("template", "0"); set("load", "1"); run_ms(1600);   /* Axis Pop: C G Am F */
        char p0[4][48], p1[4][48], k[16];
        for (int i = 0; i < 4; i++) { snprintf(k, sizeof k, "prog_%d", i + 1); snprintf(p0[i], 48, "%s", bare(k)); }
        printf("     start: %s | %s | %s | %s\n", p0[0], p0[1], p0[2], p0[3]);
        /* EDIT CHORD: only the selected chord changes */
        tap("prog_3"); run_ms(1600);
        set("style", "6"); run_ms(30); cp_test_wait(S); run_ms(5);   /* Jazz */
        for (int i = 0; i < 4; i++) { snprintf(k, sizeof k, "prog_%d", i + 1); snprintf(p1[i], 48, "%s", bare(k)); }
        printf("     EDIT CHORD + JAZZ: %s | %s | %s | %s   (%s)\n", p1[0], p1[1], p1[2], p1[3], get("status"));
        CHECK(!strcmp(p0[0], p1[0]) && !strcmp(p0[1], p1[1]) && !strcmp(p0[3], p1[3]), "EDIT CHORD: chords 1, 2, 4 unchanged");
        CHECK(strcmp(p0[2], p1[2]) != 0, "EDIT CHORD: chord 3 re-fit to Jazz (%s -> %s)", p0[2], p1[2]);
        CHECK(strstr(p1[2], "A") != NULL, "EDIT CHORD keeps the root of chord 3 (%s)", p1[2]);
        set("undo", "1"); run_ms(30); cp_test_wait(S); run_ms(5);
        { char c3[48]; snprintf(c3, sizeof c3, "%s", bare("prog_3"));
          CHECK(!strcmp(c3, p0[2]) && !strcmp(get("style"), "0"), "UNDO brings chord 3 and the style back (%s)", c3); }
        /* EDIT ALL: every chord re-fits, roots stay */
        set("scope", "1"); run_ms(30); cp_test_wait(S); run_ms(5);
        CHECK(!strcmp(get("prog_2_on"), "1") && !strcmp(get("prog_4_on"), "1"), "EDIT ALL marks every chord");
        set("style", "6"); run_ms(30); cp_test_wait(S); run_ms(5);
        int changed = 0;
        for (int i = 0; i < 4; i++) { snprintf(k, sizeof k, "prog_%d", i + 1); snprintf(p1[i], 48, "%s", bare(k)); if (strcmp(p0[i], p1[i])) changed++; }
        printf("     EDIT ALL + JAZZ: %s | %s | %s | %s\n", p1[0], p1[1], p1[2], p1[3]);
        CHECK(changed >= 3, "EDIT ALL: the whole progression re-fit to Jazz (%d of 4 changed)", changed);
        /* minor scale: degrees follow (C G Am F -> Cm Gm/G Ab Fm-ish) */
        set("style", "0"); run_ms(30); cp_test_wait(S); run_ms(5);
        set("scale", "1"); run_ms(30); cp_test_wait(S); run_ms(5);
        for (int i = 0; i < 4; i++) { snprintf(k, sizeof k, "prog_%d", i + 1); snprintf(p1[i], 48, "%s", bare(k)); }
        printf("     EDIT ALL + MINOR: %s | %s | %s | %s\n", p1[0], p1[1], p1[2], p1[3]);
        CHECK(strstr(p1[0], "Cm") != NULL && strstr(p1[2], "Ab") != NULL, "SCALE Minor maps the degrees (I->i, vi->bVI)");
        /* a Q-Link turn through several keys is one history entry and lands where one step would */
        int hn0 = atoi(strchr(get("hist_pos"), '/') + 1);
        set("key", "1"); run_ms(30); cp_test_wait(S); run_ms(5); set("key", "2"); run_ms(30); cp_test_wait(S); run_ms(5); set("key", "3"); run_ms(30); cp_test_wait(S); run_ms(5);
        int hn1 = atoi(strchr(get("hist_pos"), '/') + 1);
        CHECK(hn1 == hn0 + 1, "a Q-Link turn over 3 keys = one history entry (%d -> %d)", hn0, hn1);
        CHECK(strstr(get("prog_1"), "Ebm") != NULL, "...and the progression is in Eb minor (%s)", get("prog_1"));
        set("undo", "1"); run_ms(30); cp_test_wait(S); run_ms(5);
        CHECK(strstr(get("prog_1"), "Cm") != NULL, "one UNDO goes back to C minor (%s)", get("prog_1"));
        set("undo", "1"); run_ms(30); cp_test_wait(S); run_ms(5); set("undo", "1"); run_ms(30); cp_test_wait(S); run_ms(5); set("undo", "1"); run_ms(30); cp_test_wait(S); run_ms(5);
        for (int i = 0; i < 4; i++) { snprintf(k, sizeof k, "prog_%d", i + 1); snprintf(p1[i], 48, "%s", bare(k)); }
        printf("     after 4 UNDO: %s | %s | %s | %s\n", p1[0], p1[1], p1[2], p1[3]);
        int same = 1;
        for (int i = 0; i < 4; i++) if (strcmp(p0[i], p1[i])) same = 0;
        CHECK(same && !strcmp(get("scale"), "0") && !strcmp(get("style"), "0"), "UNDO walks back to the original C G Am F");
        /* producers: profiles give audibly different results on the same progression */
        char a[256], b[256];
        seti("producer", oldP(9)); run_ms(30); cp_test_wait(S); run_ms(5);   /* Dr. Dre */
        a[0] = 0; for (int i = 1; i <= 4; i++) { snprintf(k, sizeof k, "prog_%d", i); strcat(a, bare(k)); strcat(a, " "); }
        seti("producer", oldP(1)); run_ms(2100);   /* Q. Jones (a separate entry: wait out the merge window) */
        b[0] = 0; for (int i = 1; i <= 4; i++) { snprintf(k, sizeof k, "prog_%d", i); strcat(b, bare(k)); strcat(b, " "); }
        printf("     Dr. Dre: %s\n     Q. Jones: %s\n", a, b);
        CHECK(strcmp(a, b) != 0, "Dr. Dre and Q. Jones re-fit the progression differently");
        CHECK(strstr(get("sel2"), "Q. Jones") != NULL, "the header names the producer");
        {   /* 4.0: every profile is in exactly one list, the lists are alphabetical, 100+ each */
            int seen[NUM_ARTISTS] = {0}, okl = NUM_ARTIST_LIST >= 101 && NUM_PRODUCER_LIST >= 101;
            for (int i = 1; i < NUM_ARTIST_LIST; i++) seen[ARTIST_LIST[i]]++;
            for (int i = 1; i < NUM_PRODUCER_LIST; i++) seen[PRODUCER_LIST[i]]++;
            for (int i = 1; i < NUM_ARTISTS; i++) if (seen[i] != 1) okl = 0;
            for (int i = 0; i < NUM_OLD_ARTIST_LIST; i++) if (ARTIST_LIST[oldA(i)] != OLD_ARTIST_LIST[i]) okl = 0;
            for (int i = 0; i < NUM_OLD_PRODUCER_LIST; i++) if (PRODUCER_LIST[oldP(i)] != OLD_PRODUCER_LIST[i]) okl = 0;
            CHECK(okl, "artist (%d) and producer (%d) lists cover all %d profiles once; 3.x entries convert", NUM_ARTIST_LIST - 1,
                  NUM_PRODUCER_LIST - 1, NUM_ARTISTS - 1);
        }
        set("artist", "0"); set("producer", "0"); run_ms(2100);
        /* big keyboard: a 5+ note chord shows every note, the bass apart */
        set("scope", "0"); set("voicing", "3"); run_ms(20);   /* spread */
        tap("prog_1"); run_ms(1600);
        set("slot_type", "28"); run_ms(30); cp_test_wait(S); run_ms(5);                   /* chord 1 -> maj13 */
        tap("prog_1");
        int on = 0, bass = 0, nn = 0;
        for (int i = 1; i <= 72; i++) { snprintf(k, sizeof k, "kbx_%d", i); int v = atoi(get(k)); on += v == 1; bass += v == 2; }
        char notes[128]; snprintf(notes, sizeof notes, "%s", get("notes"));
        for (char *t = strtok(notes, " |"); t; t = strtok(NULL, " |")) if (strcmp(t, "BASS")) nn++;
        printf("     big keys: %s", get("kb_name")); printf(" | %s", get("notes")); printf(" | %s\n", get("kb_range"));
        CHECK(on + bass == nn && on >= 5 && bass == 1 && strstr(get("kb_range"), "8VA") == NULL,
              "maj13, spread: all %d notes lit on the big keyboard (%d + bass)", nn, on);
        set("voicing", "0"); run_ms(20);
        {   /* every chord type, voicing, root and variation at OCTAVE -2..0 fits the 6 octaves with its bass */
            int over = 0, tot = 0;
            for (int q = 0; q < NUM_QUALITIES; q++) for (int st = 0; st < NUM_VOICINGS; st++) for (int o = -2; o <= 0; o++)
                for (int r = 0; r < 12; r++) for (int va = 0; va < 4; va++) {
                    chord_t c = {r, q, -1}; voiced_t v; voice_chord(&c, st, o, 0, va, 1, NULL, &v);
                    int lo = v.bass >= 0 ? v.bass : v.note[0];
                    tot++; if (v.note[v.n - 1] - lo / 12 * 12 >= 72) over++;
                }
            CHECK(over == 0, "big keyboard: all %d voicings (OCTAVE -2..0) fit the 6 octaves, bass included", tot);
        }
        /* BIG KEYS button opens the panel and goes back */
        set("panel", "1"); set("keys_open", "1"); run_ms(20);
        CHECK(!strcmp(get("panel"), "3"), "BIG KEYS opens the big keyboard");
        set("keys_open", "1"); run_ms(20);
        CHECK(!strcmp(get("panel"), "1"), "...and BIG KEYS again goes back to HISTORY");
        set("br_more", "1"); run_ms(20); CHECK(!strcmp(get("panel"), "2"), "FULL BROWSER expands the browser");
        set("br_less", "1"); run_ms(20); CHECK(!strcmp(get("panel"), "0"), "CLOSE folds it back to IDEAS");
        /* previews still never reroll */
        char pb[512] = "", pa[512] = "";
        cp_test_wait(S);
        for (int i = 1; i <= 8; i++) { snprintf(k, sizeof k, "sug_%d", i); strcat(pb, get(k)); }
        for (int r = 0; r < 6; r++) { tap(r % 2 ? "prog_2" : "prog_4"); cp_test_wait(S); }
        for (int i = 1; i <= 8; i++) { snprintf(k, sizeof k, "sug_%d", i); strcat(pa, get(k)); }
        CHECK(!strcmp(pa, pb), "6 more previews: ideas unchanged");
        /* history with settings survives the project */
        char st4[8192]; E->get_param(S, "state", st4, sizeof st4);
        void *S4 = E->create(NULL); E->set_param(S4, "state", st4);
        char q1[200], q2[200], o1[200];
        E->get_param(S4, "hist_1", o1, sizeof o1);
        E->get_param(S, "hist_4", q1, sizeof q1); E->get_param(S4, strstr(o1, "OPENED") ? "hist_5" : "hist_4", q2, sizeof q2);
        printf("     state %d bytes: %s / %s\n", (int)strlen(st4), q1, q2);
        CHECK(strlen(st4) < 8000 && strchr(q1, ' ') && strchr(q2, ' ') && !strcmp(strchr(q1, ' '), strchr(q2, ' ')), "history with settings is saved with the project");
        E->destroy(S4);
        set("listen", "1"); set("scope", "0"); run_ms(1600);
    }

    /* ================================================================ 3.5 acceptance */
    {
        printf("---- 3.5\n");
        char k[16];
        set("listen", "1"); set("scope", "1"); set("artist", "0"); set("producer", "0"); set("style", "0"); set("scale", "0");
        set("key", "0"); run_ms(30); cp_test_wait(S); run_ms(2100);
        /* CRUISE: from one chord, three presses fill slots 2-4 in order, each chosen after the one before */
        set("gen_len", "0"); set("clear", "1"); run_ms(30); cp_test_wait(S);
        set("style", "6"); run_ms(30); cp_test_wait(S); run_ms(2100);   /* jazz */
        set("template", "21"); set("load", "1"); run_ms(30); cp_test_wait(S);   /* minor ii-V-i: 3 chords? */
        while (cp_test_count(S) > 1) { set("cursor", "2"); run_ms(20); set("delete", "1"); run_ms(30); cp_test_wait(S); }
        char c1[32]; snprintf(c1, sizeof c1, "%s", bare("prog_1"));
        tap("prog_1"); cp_test_wait(S); run_ms(2600);
        printf("     cruise preview: %s", get("cruise_next")); printf(" / %s\n", get("status"));
        CHECK(strstr(get("status"), "CRUISE SUGGESTS") != NULL && strstr(get("status"), "CHORD 2") != NULL, "selecting a chord: CRUISE shows the next chord for slot 2");
        for (int i = 0; i < 3; i++) { set("cruise", "1"); run_ms(30); cp_test_wait(S); run_ms(450); }
        char cr[4][32];
        for (int i = 0; i < 4; i++) { snprintf(k, sizeof k, "prog_%d", i + 1); snprintf(cr[i], 32, "%s", bare(k)); }
        printf("     CRUISE x3 from %s: %s | %s | %s | %s\n", c1, cr[0], cr[1], cr[2], cr[3]);
        CHECK(cp_test_count(S) == 4 && !strcmp(cr[0], c1), "CRUISE x3 filled slots 2-4 and kept chord 1");
        CHECK(strcmp(cr[1], cr[0]) && strcmp(cr[2], cr[1]) && strcmp(cr[3], cr[2]), "each CRUISE chord differs from the one before");
        set("cruise", "1"); run_ms(30); cp_test_wait(S);
        CHECK(cp_test_count(S) == 4 && strstr(get("status"), "FULL") != NULL, "4-chord loop full: CRUISE overwrites nothing (%s)", get("status"));
        /* in the middle: CRUISE inserts after the selected chord, chords after it move on (8-chord loop) */
        set("gen_len", "1"); run_ms(20);
        tap("prog_2"); cp_test_wait(S);
        set("cruise", "1"); run_ms(30); cp_test_wait(S); run_ms(20);
        char m3[32], m4[32]; snprintf(m3, sizeof m3, "%s", bare("prog_3")); snprintf(m4, sizeof m4, "%s", bare("prog_4"));
        CHECK(cp_test_count(S) == 5 && !strcmp(m4, cr[2]), "CRUISE on chord 2 inserted a new chord 3 (%s), the old 3 is now 4", m3);
        set("undo", "1"); run_ms(30); cp_test_wait(S);
        CHECK(cp_test_count(S) == 4, "UNDO takes the CRUISE chord back");
        set("gen_len", "0"); run_ms(20);

        /* influence controls change the chords themselves */
        set("style", "0"); run_ms(30); cp_test_wait(S); run_ms(2100);
        set("template", "0"); set("load", "1"); run_ms(30); cp_test_wait(S);   /* Axis Pop */
        set("producer", "1"); run_ms(30); cp_test_wait(S); run_ms(2100);   /* Q. Jones, EDIT ALL */
        char p50[128] = "", p100[128] = "", p0[128] = "";
        for (int i = 1; i <= 4; i++) { snprintf(k, sizeof k, "prog_%d", i); strcat(p50, bare(k)); strcat(p50, " "); }
        set("producer_mix", "100"); run_ms(30); cp_test_wait(S); run_ms(2100);
        for (int i = 1; i <= 4; i++) { snprintf(k, sizeof k, "prog_%d", i); strcat(p100, bare(k)); strcat(p100, " "); }
        set("producer_mix", "0"); run_ms(30); cp_test_wait(S); run_ms(2100);
        for (int i = 1; i <= 4; i++) { snprintf(k, sizeof k, "prog_%d", i); strcat(p0, bare(k)); strcat(p0, " "); }
        printf("     Q. Jones influence 0%%: %s\n                        50%%: %s\n                       100%%: %s\n", p0, p50, p100);
        CHECK(strcmp(p0, p100) != 0, "PRODUCER INFLUENCE 0 vs 100 changes the chords");
        set("producer_mix", "50"); run_ms(30); cp_test_wait(S); run_ms(2100);
        set("producer", "0"); run_ms(30); cp_test_wait(S); run_ms(2100);

        /* references: a profile's documented moves show up more at high influence than at none */
        {
            extern const ref_t REFS[]; extern const int NUM_REFS;
            int better = 0, tested = 0;
            for (int r = 0; r < NUM_REFS; r++) {
                const ref_t *R = &REFS[r];
                if (R->n < 2) continue;
                tested++;
                int hit[2] = {0, 0};
                for (int pass = 0; pass < 2; pass++) {
                    float mix[3] = {1.0f, pass ? 2.0f : 0.0f, pass ? 2.0f : 0.0f};
                    uint32_t rng = 777 + r;
                    for (int g = 0; g < 40; g++) {
                        chord_t out[4];
                        harmony_generate(0, R->frame ? 1 : 0, 0, R->profile, 0, mix, 0.4f, 4, out, &rng);
                        for (int a = 0; a < 4; a++) {
                            int fd = (out[a].root + 12) % 12, td = (out[(a + 1) % 4].root + 12) % 12;
                            for (int q = 0; q < R->n; q++) if (R->deg[q] == fd && R->deg[(q + 1) % R->n] == td && fd != td) { hit[pass]++; break; }
                        }
                    }
                }
                if (hit[1] > hit[0]) better++;
                else printf("     (ref %s: %d -> %d)\n", R->track, hit[0], hit[1]);
            }
            CHECK(better >= tested * 3 / 4, "reference moves: %d of %d profiles use their reference's moves more at 100%% than at 0%%", better, tested);
        }

        /* LOOP SYNC: 8 even chords in a 4-bar MPC loop play half a bar each; the loop is measured when MPC wraps */
        set("gen_len", "1"); set("listen", "1"); set("rhythm", "0"); set("loop_sync", "1"); run_ms(20);
        set("generate", "1"); run_ms(20); cp_test_wait(S); run_ms(20);
        CHECK(cp_test_count(S) == 8, "8 chords");
        playing = 1; ppq = 0;
        for (int lp = 0; lp < 2; lp++) {   /* MPC plays a 2-bar loop twice: 0..8, 0..8 */
            while (ppq < 8.0 - 1e-9) block();
            ppq = 0;
        }
        printf("     %s\n", get("loop_info"));
        CHECK(strstr(get("loop_info"), "LOOP 2 BARS") != NULL, "the MPC's 2-bar loop is measured when it wraps");
        int s0 = nout; long lf = -100000, st[32]; int ns = 0;
        while (ppq < 8.0 - 0.01) block();
        for (int i = s0; i < nout; i++) if (OUT[i].vel && OUT[i].frame - lf > 2000 && ns < 32) { st[ns++] = OUT[i].frame; lf = OUT[i].frame; }
        CHECK(ns == 8 && labs((st[1] - st[0]) - 22050) <= 128, "2-bar loop, 8 chords: 8 starts, a quarter bar apart (%d, %ld frames)", ns, ns > 1 ? st[1] - st[0] : 0L);
        playing = 0; block(); run_ms(100);
        set("loop_sync", "4"); run_ms(20);   /* 4 bars, as on the spec's example */
        playing = 1; ppq = 0; s0 = nout; lf = -100000; ns = 0;
        while (ppq < 16.0 - 0.01) block();
        playing = 0; block(); run_ms(100);
        for (int i = s0; i < nout; i++) if (OUT[i].vel && OUT[i].frame - lf > 2000 && ns < 32) { st[ns++] = OUT[i].frame; lf = OUT[i].frame; }
        CHECK(ns == 8 && labs((st[1] - st[0]) - 44100) <= 128, "4 bars, 8 chords: 8 starts, half a bar apart (%d)", ns);
        set("delete", "1"); run_ms(30); cp_test_wait(S);   /* 7 chords: still the whole loop, nothing cut */
        playing = 1; ppq = 0; s0 = nout; lf = -100000; ns = 0;
        while (ppq < 16.0 - 0.01) block();
        playing = 0; block(); run_ms(100);
        int ons2 = 0, offs2 = 0;
        for (int i = s0; i < nout; i++) { if (OUT[i].vel) ons2++; else offs2++; if (OUT[i].vel && OUT[i].frame - lf > 2000 && ns < 32) { st[ns++] = OUT[i].frame; lf = OUT[i].frame; } }
        CHECK(ns == 7 && ons2 == offs2, "after DELETE: 7 chords share the 4 bars, every note ends (%d starts, %d/%d)", ns, ons2, offs2);
        printf("     %s\n", get("loop_info"));
        set("loop_sync", "0"); run_ms(20);   /* manual override: CHORD LENGTH again */
        set("bars", "12"); playing = 1; ppq = 0; s0 = nout; lf = -100000; ns = 0;
        while (ppq < 8.0 - 0.01) block();
        playing = 0; block(); run_ms(100);
        for (int i = s0; i < nout; i++) if (OUT[i].vel && OUT[i].frame - lf > 2000 && ns < 32) { st[ns++] = OUT[i].frame; lf = OUT[i].frame; }
        CHECK(ns == 2 && labs((st[1] - st[0]) - 88200) <= 128, "MANUAL: one bar per chord again (%d)", ns);

        /* the touch browser: tiles show the chosen category and a tap sets it (and re-fits, like the control) */
        set("browse", "4"); run_ms(20);
        CHECK(!strcmp(get("bri_2"), ARTIST_INFO[PRODUCER_LIST[1]].name), "BROWSER PRODUCER tiles: %s ... %s", get("bri_2"), get("bri_13"));
        set("bri_10", "1"); run_ms(30); cp_test_wait(S); run_ms(450);
        CHECK(!strcmp(get("producer"), "9") && !strcmp(get("bri_10_on"), "1"), "tapping a tile chooses it (%s)", get("sel2"));
        set("browse", "0"); run_ms(20);
        CHECK(!strcmp(get("bri_1"), "C") && !strcmp(get("bri_12"), "B"), "BROWSER KEY tiles C..B");
        set("producer", "0"); run_ms(30); cp_test_wait(S); run_ms(2100);

        /* library lines come from the model's own data */
        set("lib_kind", "1"); seti("producer", oldP(1)); run_ms(30); cp_test_wait(S); run_ms(2100);
        for (int i = 1; i <= 7; i++) { snprintf(k, sizeof k, "prof_%d", i); printf("     %s\n", get(k)); }
        CHECK(strstr(get("prof_2"), "13") != NULL && strstr(get("prof_5"), "ROCK WITH YOU") != NULL, "LIBRARY: Q. Jones shows its chords and its reference");
        int ok = 1;
        set("scope", "0"); run_ms(20);
        for (int pi = 0; pi < NUM_PRODUCER_LIST; pi++) { seti("producer", pi); run_ms(2);
            for (int i = 1; i <= 7; i++) { snprintf(k, sizeof k, "prof_%d", i); if (strlen(get(k)) > 63) { ok = 0; printf("     long: %s\n", get(k)); } } }
        set("lib_kind", "0");
        for (int pi = 0; pi < NUM_ARTIST_LIST; pi++) { seti("artist", pi); run_ms(2);
            for (int i = 1; i <= 7; i++) { snprintf(k, sizeof k, "prof_%d", i); if (strlen(get(k)) > 63) { ok = 0; printf("     long: %s\n", get(k)); } } }
        set("artist", "0"); cp_test_wait(S);
        CHECK(ok, "every library line of all %d profiles fits the 64-character readouts", NUM_ARTIST_LIST + NUM_PRODUCER_LIST - 2);
        set("producer", "0"); set("scope", "0"); set("loop_sync", "0"); run_ms(30); cp_test_wait(S); run_ms(2100);
    }


    /* ======================================================================== 4.0 acceptance */
    printf("---- 4.0\n");
    {
        char k[32];
        set("listen", "1"); set("scope", "1"); set("artist", "0"); set("producer", "0"); set("style", "0"); set("scale", "0");
        set("key", "0"); set("loop_sync", "0"); set("rhythm", "0"); set("rrate", "0"); set("swing", "50"); set("push", "0");
        set("gen_len", "0"); set("template", "0"); tap("load"); run_ms(2100); cp_test_wait(S);   /* C G Am F */

        /* CHORD LENGTH from a 1/64 triplet to 16 bars */
        int lens[3] = {0, 12, 16};
        double expect[3] = {1.0 / 24, 4, 64};
        for (int li = 0; li < 3; li++) {
            seti("bars", lens[li]); run_ms(10);
            playing = 1; ppq = 0;
            int s0 = nout, ns = 0; long first = -1, second = -1;
            int blocks = li == 2 ? (int)(70 * 4 * 60.0 / 120 * 44100 / 128) : (int)(2 * 4 * 60.0 / 120 * 44100 / 128);
            for (int i = 0; i < blocks; i++) block();
            playing = 0; run_ms(50);
            /* chord starts: the first note of each new chord on the chord channel (ch index 1) */
            int lastn = -1;
            for (int i = s0; i < nout; i++) if (OUT[i].vel && OUT[i].ch == 1 && OUT[i].note != lastn) {
                long f = OUT[i].frame;
                if (first < 0) first = f; else if (second < 0 && f > first) second = f;
                lastn = OUT[i].note; ns++;
            }
            double beats = second > first ? (second - first) * 120 / 60.0 / 44100 : -1;
            CHECK(fabs(beats - expect[li]) < 0.03 * expect[li] + 0.006, "CHORD LENGTH %s: next chord after %.4f beats (want %.4f)",
                  lens[li] == 0 ? "1/64T" : lens[li] == 12 ? "1 BAR" : "16 BARS", beats, expect[li]);
            set("panic", "1"); run_ms(20);
        }
        set("bars", "12");

        /* RHYTHM RATE and SWING: Synthwave 16 (a hit on every step) at 1/16: at 75 % every second 16th comes late, at once */
        set("rhythm", "40"); set("rrate", "6"); set("swing", "50"); run_ms(10);   /* rrate 6 = 1/16 */
        playing = 1; ppq = 0;
        int s0 = nout;
        for (int i = 0; i < (int)(1 * 60.0 / 120 * 44100 / 128); i++) block();   /* one beat */
        set("swing", "75");   /* turned while playing */
        for (int i = 0; i < (int)(1 * 60.0 / 120 * 44100 / 128); i++) block();
        playing = 0; run_ms(30);
        long hits[16]; int nh = 0; long prevf = -1;
        for (int i = s0; i < nout && nh < 16; i++) if (OUT[i].vel && OUT[i].ch == 1 && (prevf < 0 || OUT[i].frame - prevf > 400)) { hits[nh++] = OUT[i].frame; prevf = OUT[i].frame; }
        double q = 44100 * 60.0 / 120 / 4;   /* a 16th in frames */
        for (int i = 1; i < nh; i++) printf(" %.2f", (hits[i] - hits[i - 1]) / q);
        printf("\n");
        CHECK(nh >= 8 && fabs((hits[1] - hits[0]) - q) < 300 && fabs((hits[5] - hits[4]) - 1.5 * q) < 300 && fabs((hits[6] - hits[5]) - 0.5 * q) < 300,
              "RHYTHM RATE 1/16 + SWING: straight %.2f then swung %.2f sixteenths (live)", (hits[1] - hits[0]) / q, (hits[5] - hits[4]) / q);
        set("swing", "50"); set("rrate", "0"); set("panic", "1"); run_ms(20);

        /* genre grooves: Reggae One Drop plays the bass alone and skanks the upper notes */
        set("rhythm", "31"); run_ms(10);   /* 16 + 15: Reggae One Drop */
        playing = 1; ppq = 0; s0 = nout;
        for (int i = 0; i < (int)(4 * 60.0 / 120 * 44100 / 128); i++) block();
        playing = 0; run_ms(30);
        int bass_alone = 0, upper = 0, full = 0;
        for (int i = s0; i < nout; i++) if (OUT[i].vel) {
            int n = 0; for (int j = s0; j < nout; j++) if (OUT[j].vel && OUT[j].frame == OUT[i].frame) n++;
            if (n == 1) bass_alone++; else if (n == 2 || n == 3) upper++; else full++;
        }
        CHECK(bass_alone > 0 && upper > 0, "REGGAE ONE DROP: bass alone (%d) and upper-note skanks (%d)", bass_alone, upper);
        set("rhythm", "0"); set("panic", "1"); run_ms(20);

        /* the keyboard shows what an influence change did: New and Gone keys, and a line */
        set("scope", "1"); set("listen", "1"); run_ms(10);
        tap("prog_3");
        seti("producer", oldP(1)); run_ms(30); cp_test_wait(S); run_ms(500);   /* Q. Jones, EDIT ALL */
        tap("prog_3");
        int news = 0, gone = 0;
        for (int i = 1; i <= 72; i++) { snprintf(k, sizeof k, "kbx_%d", i); news += !strcmp(get(k), "3"); gone += !strcmp(get(k), "4"); }
        printf("     keys: %s | %s\n", get("kb_name"), get("kb_diff"));
        CHECK(news > 0 && strstr(get("kb_diff"), "WAS") != NULL, "BIG KEYS mark what the producer added (%d new, %d gone): %s", news, gone, get("kb_diff"));
        set("producer", "0"); run_ms(30); cp_test_wait(S); run_ms(2100);

        /* preview mute group: a second idea cuts the first at once */
        set("panel", "0");
        int a0 = nout;
        tap("sug_1"); run_ms(50);
        int b0 = nout;
        set("sug_2", "1"); run_ms(30);
        int cut = 0, b_on = 0;
        for (int i = b0; i < nout; i++) { if (!OUT[i].vel) cut++; else b_on++; }
        (void)a0;
        CHECK(cut > 0 && b_on > 0, "IDEAS are one mute group: tapping idea 2 ends idea 1 (%d offs) and plays idea 2", cut);
        run_ms(1600);

        /* LEARN: first and last pad (any bank) */
        tap("learn");
        uint8_t m1[3] = {0x99, 60, 100}, m2[3] = {0x89, 60, 0}, m3[3] = {0x99, 67, 100}, m4[3] = {0x89, 67, 0};
        E->midi(S, m1, 3); run_ms(30); E->midi(S, m2, 3); run_ms(30);
        E->midi(S, m3, 3); run_ms(30); E->midi(S, m4, 3); run_ms(30);
        run_ms(4200);   /* no more taps: LEARN ends with the chromatic row */
        CHECK(!strcmp(get("pad_base"), "60") && !strcmp(get("pad_count"), "8"), "LEARN first pad 60, last pad 67: %s", get("learn_info"));
        int p0 = nout;
        uint8_t h1[3] = {0x99, 61, 100}, h2[3] = {0x89, 61, 0};
        E->midi(S, h1, 3); run_ms(50); E->midi(S, h2, 3); run_ms(50);
        CHECK(nout > p0 && !strcmp(get("cursor"), "2"), "the second learned pad plays chord 2");

        /* VOICE: EDIT ALL with TYPE gives every chord the type; ROOT moves them all */
        set("scope", "1"); tap("prog_1");
        set("slot_type", "10"); run_ms(30);   /* m7 */
        char pr[160] = "";
        for (int i = 1; i <= 4; i++) { snprintf(k, sizeof k, "prog_%d", i); strcat(pr, bare(k)); strcat(pr, " "); }
        CHECK(!strcmp(pr, "Cm7 Gm7 Am7 Fm7 "), "EDIT ALL TYPE m7: %s", pr);
        set("slot_root", "2"); run_ms(30);   /* C -> D: every chord up a whole step */
        pr[0] = 0;
        for (int i = 1; i <= 4; i++) { snprintf(k, sizeof k, "prog_%d", i); strcat(pr, bare(k)); strcat(pr, " "); }
        CHECK(!strcmp(pr, "Dm7 Am7 Bm7 Gm7 "), "EDIT ALL ROOT D: %s", pr);
        set("scope", "0");

        /* ideas never share their notes */
        int dup = 0;
        for (int r = 0; r < 30; r++) {
            tap("reroll"); cp_test_wait(S);
            const char *nm[8]; char nb[8][48];
            for (int i = 0; i < 8; i++) { snprintf(k, sizeof k, "sug_%d", i + 1); snprintf(nb[i], 48, "%s", get(k)); nm[i] = nb[i]; }
            for (int i = 0; i < 8; i++) for (int j = i + 1; j < 8; j++) if (!strcmp(nm[i], nm[j]) && nm[i][0]) dup++;
        }
        CHECK(dup == 0, "30 rerolls: no idea twice in a pool");

        /* GENERATE never writes a progression that is already in the history */
        char seen[24][160]; int nseen = 0, rep = 0;
        set("style", "11"); run_ms(30); cp_test_wait(S);
        for (int r = 0; r < 20; r++) {
            tap("generate"); cp_test_wait(S); run_ms(20);
            char cur[160] = "";
            for (int i = 1; i <= 4; i++) { snprintf(k, sizeof k, "prog_%d", i); strcat(cur, bare(k)); strcat(cur, " "); }
            for (int j = 0; j < nseen; j++) if (!strcmp(seen[j], cur)) rep++;
            snprintf(seen[nseen++], 160, "%s", cur);
        }
        CHECK(rep == 0, "20 x GENERATE in Rock: no progression repeated");

        /* factory presets: the PRESET menu names and loads them */
        CHECK(!strcmp(get("preset:0"), "Velvet Neo Soul") && strstr(get("preset:24"), "USER 1") != NULL, "preset names: %s / %s", get("preset:0"), get("preset:24"));
        tap("pre_2"); cp_test_wait(S); run_ms(30);
        printf("     preset: %s | %s | %s\n", get("sel1"), get("sel2"), get("status"));
        CHECK(strstr(get("sel2"), "Trap") != NULL && cp_test_count(S) == 4, "factory preset 2 (Midnight Trap) loads style, profiles and chords");
        set("preset", "3"); cp_test_wait(S); run_ms(30);   /* the MPC PRESET menu path */
        CHECK(strstr(get("sel2"), "Bossa") != NULL, "MPC PRESET menu: program 3 = Bossa Rio (%s)", get("sel2"));
        tap("undo"); run_ms(30);
        CHECK(strstr(get("sel2"), "Trap") != NULL, "UNDO after a preset brings the state before it back");
        /* user presets: save to a slot, load it back */
        char before[160] = "";
        for (int i = 1; i <= 4; i++) { snprintf(k, sizeof k, "prog_%d", i); strcat(before, bare(k)); strcat(before, " "); }
        set("save_slot", "16"); tap("save_preset"); cp_test_wait(S); run_ms(30);
        printf("     save: %s | %s\n", get("status"), get("usr_16"));
        tap("pre_1"); cp_test_wait(S); run_ms(30);
        tap("usr_16"); cp_test_wait(S); run_ms(30);
        char after[160] = "";
        for (int i = 1; i <= 4; i++) { snprintf(k, sizeof k, "prog_%d", i); strcat(after, bare(k)); strcat(after, " "); }
        CHECK(!strcmp(before, after) && strstr(get("sel2"), "Trap") != NULL, "USER 16 saved and loaded back: %s", after);

        /* 3.x projects: artist / producer / length / arp rate convert */
        E->set_param(S, "state", "v=4;artist=5;producer=9;bars=1;rate=3;slots=0,0,-1,0,1|7,0,-1,0,1;");
        run_ms(30);
        CHECK(strstr(get("sel2"), "Drake") && strstr(get("sel2"), "Dr. Dre") && !strcmp(get("bars"), "12") && !strcmp(get("rate"), "5"),
              "a 3.5 project opens with the same artist, producer, CHORD LENGTH and ARP RATE (%s)", get("sel2"));
        set("artist", "0"); set("producer", "0"); cp_test_wait(S); run_ms(2100);
    }

    /* ======================================================================== 4.1 */
    printf("---- 4.1\n");
    {
        char k[32];
        set("listen", "1"); set("scope", "0"); set("artist", "0"); set("producer", "0"); set("style", "0"); set("scale", "0");
        set("key", "0"); set("loop_sync", "0"); set("rhythm", "0"); set("rrate", "0"); set("bars", "12"); set("push", "0");
        set("humanize", "0"); set("strum", "0"); set("strum_mode", "0"); set("panel", "0");
        set("gen_len", "0"); set("template", "0"); tap("load"); run_ms(2100); cp_test_wait(S);   /* C G Am F */

        /* a selected chord is the one an idea replaces (tap the chord, then an idea twice) */
        char before4[32]; snprintf(before4, sizeof before4, "%s", bare("prog_4"));
        tap("prog_1"); cp_test_wait(S);
        char idea[48]; snprintf(idea, sizeof idea, "%s", get("sug_2"));
        tap("sug_2"); tap("sug_2"); cp_test_wait(S);
        char p1[32]; snprintf(p1, sizeof p1, "%s", bare("prog_1"));
        CHECK(strncmp(p1, idea, strlen(p1)) == 0 && !strcmp(bare("prog_4"), before4) && cp_test_count(S) == 4,
              "chord 1 selected + idea tapped twice: chord 1 is %s, chord 4 stays %s", p1, bare("prog_4"));
        tap("undo");

        /* knobs, artist, producer, key, scale: silent when stopped */
        set("scope", "1"); run_ms(20);
        int q0 = nout;
        set("producer_mix", "90"); run_ms(30); cp_test_wait(S); run_ms(300);
        seti("producer", oldP(1)); run_ms(30); cp_test_wait(S); run_ms(2100);
        set("key", "2"); run_ms(30); cp_test_wait(S); run_ms(2100);
        set("scale", "1"); run_ms(30); cp_test_wait(S); run_ms(2100);
        int ons = 0; for (int i = q0; i < nout; i++) if (OUT[i].vel && OUT[i].note < 128) ons++;
        CHECK(ons == 0, "knob / producer / key / scale changes make no sound (%d note-ons): %s", ons, get("status"));
        /* TYPE when stopped: heard */
        tap("prog_2"); run_ms(1600);
        q0 = nout; set("slot_type", "8"); run_ms(60);
        ons = 0; for (int i = q0; i < nout; i++) if (OUT[i].vel && OUT[i].note < 128) ons++;
        CHECK(ons > 0, "TYPE with the transport stopped: the chord sounds (%d notes)", ons);
        run_ms(1600);
        /* playing: a change MORPHS the sounding chord (lost notes stop, new ones start, shared ones keep going) */
        set("scope", "1"); set("bars", "14"); run_ms(10);   /* 4-bar chords, sustained */
        playing = 1; ppq = 0;
        for (int i = 0; i < (int)(2 * 60.0 / 120 * 44100 / 128); i++) block();   /* 2 beats into chord 1 */
        int m0 = nout;
        set("slot_type", "10"); run_ms(40);   /* every chord m7 */
        int mon = 0, moff = 0, retrig = 0;
        for (int i = m0; i < nout; i++) { if (OUT[i].note >= 128) continue; if (OUT[i].vel) mon++; else moff++; }
        for (int i = m0; i < nout; i++) if (OUT[i].vel && OUT[i].note < 128) for (int j = m0; j < nout; j++) if (!OUT[j].vel && OUT[j].note == OUT[i].note && j < i) retrig++;
        printf("     morph: %d on, %d off, %s\n", mon, moff, get("prog_1"));
        CHECK(mon >= 1 && mon <= 3 && retrig == 0, "TYPE while playing morphs: %d new notes, %d stopped, no re-attack", mon, moff);
        playing = 0; run_ms(100); set("bars", "12"); set("scope", "0");
        tap("undo"); tap("undo");

        /* LEARN with a pad layout (scale pads): first pad 72, last 98, the rest tapped once */
        uint8_t m[3];
        tap("learn");
        int scale_pads[16] = {72, 74, 76, 77, 79, 81, 83, 84, 86, 88, 89, 91, 93, 95, 96, 98};
        m[0] = 0x90; m[2] = 100;
        m[1] = 72; E->midi(S, m, 3); run_ms(30);
        m[1] = 98; E->midi(S, m, 3); run_ms(30);
        printf("     learn: %s\n", get("learn_info"));
        for (int i = 1; i < 15; i++) { m[1] = (uint8_t)scale_pads[i]; E->midi(S, m, 3); run_ms(30); }
        run_ms(4200);
        CHECK(!strcmp(get("pad_count"), "16") && strstr(get("learn_info"), "BY NOTE") != NULL, "LEARN a scale pad layout: %s", get("learn_info"));
        int l0 = nout;
        m[1] = 76; E->midi(S, m, 3); run_ms(40);
        CHECK(nout > l0 && !strcmp(get("cursor"), "3"), "the 3rd pad of the layout (note 76) plays chord 3");
        m[0] = 0x80; m[2] = 0; E->midi(S, m, 3); run_ms(20);
        /* a chord pad layout: three notes per pad trigger one pad (by the lowest) */
        tap("learn");
        m[0] = 0x90; m[2] = 100;
        m[1] = 48; E->midi(S, m, 3); m[1] = 52; E->midi(S, m, 3); m[1] = 55; E->midi(S, m, 3); run_ms(30);
        m[1] = 60; E->midi(S, m, 3); m[1] = 64; E->midi(S, m, 3); m[1] = 67; E->midi(S, m, 3); run_ms(30);
        {   /* the 6 pads between: C D E F G A B C chords (by their lowest notes 48 50 52 53 55 57 59 60) */
            int roots[6][3] = {{50, 53, 57}, {52, 55, 59}, {53, 57, 60}, {55, 59, 62}, {57, 60, 64}, {59, 62, 65}};
            for (int p = 0; p < 6; p++) { for (int j = 0; j < 3; j++) { m[1] = (uint8_t)roots[p][j]; E->midi(S, m, 3); } run_ms(30); }
        }
        run_ms(4200);   /* 4 s without a tap ends LEARN */
        CHECK(!strcmp(get("pad_count"), "8") && strstr(get("learn_info"), "BY NOTE") != NULL, "LEARN with chord pads (3 notes each): %s", get("learn_info"));
        m[1] = 52; E->midi(S, m, 3); m[1] = 55; E->midi(S, m, 3); m[1] = 59; E->midi(S, m, 3); run_ms(40);
        CHECK(!strcmp(get("cursor"), "3"), "a chord pad hit (E minor = pad 3) plays ONE pad: chord 3 (%s)", get("cursor"));
        m[0] = 0x80; m[2] = 0; m[1] = 50; E->midi(S, m, 3); run_ms(20);
        set("pad_base", "36"); set("pad_count", "16");

        /* STRUM DIRECTION: Down starts with the highest note */
        set("strum", "60"); set("strum_mode", "1"); run_ms(1600);
        int s0 = nout;
        tap("prog_1");
        int firstn = -1, maxn = -1;
        for (int i = s0; i < nout; i++) if (OUT[i].vel && OUT[i].ch == 1 && OUT[i].note < 128) { if (firstn < 0) firstn = OUT[i].note; if (OUT[i].note > maxn) maxn = OUT[i].note; }
        CHECK(firstn == maxn && firstn > 0, "STRUM Down: the highest note comes first (%d)", firstn);
        set("strum_mode", "0"); s0 = nout; run_ms(1600); tap("prog_1");
        int minn = 200; firstn = -1;
        for (int i = s0; i < nout; i++) if (OUT[i].vel && OUT[i].ch == 1 && OUT[i].note < 128) { if (firstn < 0) firstn = OUT[i].note; if (OUT[i].note < minn) minn = OUT[i].note; }
        CHECK(firstn == minn, "STRUM Up: the lowest note comes first");
        set("strum", "0");

        /* HUMANIZE 100: a loose player (notes of one chord tens of ms apart) */
        set("humanize", "100"); set("rhythm", "3"); run_ms(10);
        playing = 1; ppq = 0; s0 = nout;
        for (int i = 0; i < (int)(4 * 60.0 / 120 * 44100 / 128); i++) block();
        playing = 0; run_ms(100);
        long spread = 0, first = -1;
        for (int i = s0; i < nout; i++) if (OUT[i].vel && OUT[i].ch == 1 && OUT[i].note < 128) {
            if (first < 0) first = OUT[i].frame;
            if (OUT[i].frame - first < 4410 && OUT[i].frame - first > spread) spread = OUT[i].frame - first;
        }
        CHECK(spread > ms_to_frames(15), "HUMANIZE 100: one hit spreads over %.1f ms", spread / 44.1);
        set("humanize", "0"); set("rhythm", "0");

        /* REVERSE per chord: the MIDI port gets an expression swell (CC 11 0 -> 127), the others none */
        tap("prog_2"); set("slot_rev", "1"); run_ms(30);
        CHECK(strstr(get("prog_2"), "REV") != NULL && strstr(get("prog_1"), "REV") == NULL, "REVERSE marks chord 2 only (%s)", get("prog_2"));
        playing = 1; ppq = 0; s0 = nout;
        for (int i = 0; i < (int)(8 * 60.0 / 120 * 44100 / 128); i++) block();   /* chords 1 and 2 */
        playing = 0; run_ms(100);
        int cc = 0, cc0 = 0, cc127 = 0;
        for (int i = s0; i < nout; i++) if (OUT[i].note == 128 + 11) { cc++; if (OUT[i].vel == 0) cc0++; if (OUT[i].vel == 127) cc127++; }
        CHECK(cc >= 10 && cc0 >= 1 && cc127 >= 1, "REVERSE: %d expression steps on chord 2, from 0 to 127", cc);
        set("slot_rev", "0");
    }

    /* MIDI OUT on mutes the synth; SYNTH + MIDI brings it back */
    rms_acc = 0; rms_n = 0; peak = 0;
    playing = 1; ppq = 0;
    for (int i = 0; i < (int)(4 * 60.0 / 120 * 44100 / 128); i++) block();
    playing = 0; run_ms(1500);
    CHECK(peak < 50, "MIDI OUT on: synth muted (peak %d)", peak);
    set("synth", "1");

    /* synth level sanity */
    rms_acc = 0; rms_n = 0; peak = 0;
    set("rhythm", "0");
    playing = 1; ppq = 0;
    for (int i = 0; i < (int)(4 * 60.0 / 120 * 44100 / 128); i++) block();
    playing = 0;
    double rms = sqrt(rms_acc / (rms_n ? rms_n : 1)) / 32768.0;
    CHECK(rms > 0.01 && peak < 32767, "synth: rms %.3f, peak %d (no clipping)", rms, peak);

    E->destroy(S);
    printf(fails ? "FAILED (%d)\n" : "PASSED\n", fails);
    return fails != 0;
}
