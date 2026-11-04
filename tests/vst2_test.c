/* CheatChord: loads the VST2 build like a VST2 host (MPC 2 desktop) does and checks the ABI from the host side.
 * The structures here are written separately from vst2abi/ (after the MPC toolkit's own VST2 host test), so a
 * mismatch in the clean-room headers shows up as a failure.
 *   vst2_test <CheatChord.so> */
#include <dlfcn.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AEffect AEffect;
typedef intptr_t (*cb_t)(AEffect *, int32_t, int32_t, intptr_t, void *, float);
struct AEffect {
    int32_t magic;
    intptr_t (*d)(AEffect *, int32_t, int32_t, intptr_t, void *, float);
    void *p;
    void (*setP)(AEffect *, int32_t, float);
    float (*getP)(AEffect *, int32_t);
    int32_t np, npar, ni, no, flags;
    intptr_t r1, r2;
    int32_t a, b, c;
    float io;
    void *obj, *user;
    int32_t uid, ver;
    void (*pr)(AEffect *, float **, float **, int32_t);
    void *pdr;
    char f[56];
};
typedef struct { int32_t type, byteSize, deltaFrames, flags, noteLength, noteOffset; unsigned char m[4]; char x[4]; } ME;
typedef struct { int32_t n; intptr_t r; void *ev[2]; } EV;
typedef struct {
    double samplePos, sampleRate, nanoSeconds, ppqPos, tempo, barStartPos, cycleStartPos, cycleEndPos;
    int32_t num, den, smpteOffset, smpteFrameRate, samplesToNextClock, flags;
} TI;

static int fails, checks;
#define CHECK(c, ...) do { checks++; printf ("%s ", (c) ? "ok  " : "FAIL"); printf (__VA_ARGS__); printf ("\n"); if (!(c)) fails++; } while (0)

static TI ti;
static int ons, offs, onGrid, groups;
static double lastStart = -10, blockPpq;
static intptr_t host (AEffect *e, int32_t op, int32_t i, intptr_t v, void *p, float o) {
    (void)e; (void)i; (void)v; (void)o;
    if (op == 1) return 2400;   /* audioMasterVersion */
    if (op == 7) return (intptr_t)&ti;   /* audioMasterGetTime */
    if (op == 8 && p) {   /* audioMasterProcessEvents: the plugin's MIDI out */
        EV *ev = p;
        for (int k = 0; k < ev->n; k++) {
            ME *m = ev->ev[k];
            if (m->type != 1) continue;
            int st = m->m[0] & 0xf0;
            if (st == 0x90 && m->m[2]) {
                ons++;
                double b = blockPpq + m->deltaFrames * ti.tempo / 60.0 / ti.sampleRate;
                if (b - lastStart > 0.5 && (ti.flags & 2)) {
                    groups++;
                    lastStart = b;
                    double n = floor (b / 4.0 + 0.5) * 4.0;
                    if (fabs (b - n) * 60.0 / ti.tempo * ti.sampleRate < 1.5) onGrid++;
                }
            } else if (st == 0x80 || st == 0x90) offs++;
        }
        return 1;
    }
    if (op == 37) return 0;   /* canDo */
    return 0;
}

static float peak;
static void run (AEffect *a, int blocks, int n) {
    float L[1024], R[1024], *o[2] = {L, R};
    for (int k = 0; k < blocks; k++) {
        blockPpq = ti.ppqPos;
        a->pr (a, NULL, o, n);
        for (int i = 0; i < n; i++) { if (fabsf (L[i]) > peak) peak = fabsf (L[i]); }
        ti.samplePos += n;
        if (ti.flags & 2) ti.ppqPos += n * ti.tempo / 60.0 / ti.sampleRate;
    }
}

int main (int argc, char **argv) {
    if (argc < 2) { printf ("usage: vst2_test <CheatChord.so>\n"); return 2; }
    void *h = dlopen (argv[1], RTLD_NOW | RTLD_LOCAL);
    CHECK (h != NULL, "dlopen %s", h ? "" : dlerror ());
    if (!h) return 1;
    AEffect *(*entry)(cb_t) = (AEffect * (*)(cb_t)) dlsym (h, "VSTPluginMain");
    CHECK (entry != NULL, "VSTPluginMain exported");
    if (!entry) return 1;
    AEffect *a = entry (host);
    CHECK (a && a->magic == 0x56737450, "magic 'VstP'");
    if (!a) return 1;
    CHECK ((a->flags & (1 << 8)) && (a->flags & (1 << 4)) && (a->flags & (1 << 5)) && (a->flags & 1),
           "flags: synth, replacing, chunks, editor (0x%x)", a->flags);
    CHECK (a->no == 2 && a->ni == 0, "0 in, 2 out (%d/%d)", a->ni, a->no);
    char s[256] = "";
    a->d (a, 0, 0, 0, NULL, 0);   /* effOpen */
    a->d (a, 45, 0, 0, s, 0);
    char vnd[256] = "";
    a->d (a, 47, 0, 0, vnd, 0);
    CHECK (!strcmp (s, "CheatChord") && !strcmp (vnd, "JJ DA PLUG"), "name %s, vendor %s", s, vnd);
    CHECK (a->d (a, 35, 0, 0, NULL, 0) == 2, "category synth");
    CHECK (a->d (a, 51, 0, 0, (void *)"receiveVstMidiEvent", 0) == 1 && a->d (a, 51, 0, 0, (void *)"sendVstMidiEvent", 0) == 1,
           "canDo receive and send MIDI");
    CHECK (a->npar >= 50 && a->np == 40, "%d parameters, %d programs", a->npar, a->np);
    char pn[64] = "";
    a->d (a, 29, 1, 0, pn, 0);
    CHECK (pn[0], "program 2 = %s", pn);
    char p0[64] = "";
    a->d (a, 8, 0, 0, p0, 0);
    CHECK (!strcmp (p0, "Key"), "parameter 0 = %s", p0);

    ti.sampleRate = 48000; ti.tempo = 120; ti.num = 4; ti.den = 4;
    ti.flags = (1 << 9) | (1 << 10) | (1 << 13);
    a->d (a, 10, 0, 0, NULL, 48000.0f);   /* effSetSampleRate */
    a->d (a, 11, 0, 512, NULL, 0);        /* effSetBlockSize */
    a->d (a, 12, 0, 1, NULL, 0);          /* effMainsChanged on */
    run (a, 100, 512);
    /* GENERATE through its automatable action parameter (rising edge) */
    int gen = -1, mode = -1, hum = -1;
    for (int i = 0; i < a->npar; i++) {
        char nm[64] = "";
        a->d (a, 8, i, 0, nm, 0);
        if (!strcmp (nm, "Action: Generat") || !strncmp (nm, "Action: Generate", 15)) gen = i;
        if (!strncmp (nm, "Play Mode", 8)) mode = i;
        if (!strcmp (nm, "Humanize")) hum = i;
    }
    CHECK (gen >= 0 && mode >= 0 && hum >= 0, "found Generate (%d), Play Mode (%d), Humanize (%d)", gen, mode, hum);
    if (gen >= 0) { a->setP (a, gen, 1.0f); run (a, 10, 512); a->setP (a, gen, 0.0f); run (a, 40, 512); }
    if (mode >= 0) a->setP (a, mode, 0.5f);   /* HOST */
    if (hum >= 0) a->setP (a, hum, 0.0f);
    run (a, 4, 512);
    ons = offs = 0;
    ti.flags |= 2;   /* playing */
    ti.ppqPos = 0;
    run (a, (int)(16.0 * 48000 / 512) + 2, 512);   /* 8 bars at 120 */
    ti.flags &= ~2;
    run (a, 60, 512);
    CHECK (ons >= 24 && ons == offs, "MIDI out to the host while it plays: %d on, %d off", ons, offs);
    CHECK (groups >= 8 && onGrid == groups, "chords on the bar lines: %d of %d", onGrid, groups);
    CHECK (peak > 0.01f && peak < 1.0f, "synth audio, peak %.3f", peak);

    /* a pad from MPC: note 36 while stopped */
    ons = offs = 0;
    ME on = {1, sizeof (ME), 7, 0, 0, 0, {0x90, 36, 100, 0}, {0}}, off = {1, sizeof (ME), 3, 0, 0, 0, {0x80, 36, 0, 0}, {0}};
    EV ev = {1, 0, {&on, NULL}};
    a->d (a, 25, 0, 0, &ev, 0);
    run (a, 30, 512);
    ev.ev[0] = &off;
    a->d (a, 25, 0, 0, &ev, 0);
    run (a, 30, 512);
    CHECK (ons >= 3 && ons == offs, "pad note 36 plays chord 1 (%d on, %d off)", ons, offs);

    /* chunk: a second instance opens to the same state */
    void *chunk = NULL;
    intptr_t n = a->d (a, 23, 0, 0, &chunk, 0);   /* effGetChunk (bank) */
    CHECK (n > 100 && chunk, "chunk %ld bytes", (long)n);
    AEffect *b = entry (host);
    b->d (b, 0, 0, 0, NULL, 0);
    b->d (b, 10, 0, 0, NULL, 48000.0f);
    b->d (b, 12, 0, 1, NULL, 0);
    char *copy = malloc ((size_t)n);
    memcpy (copy, chunk, (size_t)n);
    b->d (b, 24, 0, n, copy, 0);   /* effSetChunk */
    void *chunk2 = NULL;
    intptr_t n2 = b->d (b, 23, 0, 0, &chunk2, 0);
    CHECK (n2 > 100, "second instance loaded the chunk (%ld bytes)", (long)n2);
    float k1 = a->getP (a, 0), k2 = b->getP (b, 0);
    CHECK (fabsf (k1 - k2) < 1e-6f, "same key after loading (%.3f / %.3f)", k1, k2);
    free (copy);
    a->d (a, 12, 0, 0, NULL, 0);
    b->d (b, 12, 0, 0, NULL, 0);
    a->d (a, 1, 0, 0, NULL, 0);   /* effClose */
    b->d (b, 1, 0, 0, NULL, 0);
    printf (fails ? "FAILED (%d of %d)\n" : "PASSED (%d checks)\n", fails ? fails : checks, checks);
    return fails ? 1 : 0;
}
