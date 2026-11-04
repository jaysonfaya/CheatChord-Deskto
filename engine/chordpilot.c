/* CheatChord (formerly Chord Pilot): a chord-progression co-pilot for MPC OS.
 *
 * Workflow (Cruise): the IDEAS tiles show eight chords that fit after the chord before the edit slot. Tap one to hear
 * it (it lights), tap it again (or ADD) to put it into the PROGRESSION; the edit slot moves on and a fresh set of
 * ideas appears for the next step. REROLL deals eight different ideas for the same spot. GENERATE writes a whole
 * progression. Tap a progression tile to hear it and make it the edit slot (ideas then fit between its neighbours).
 *
 * Output: the progression plays with MPC's transport in the chosen rhythm (or from pads 1-8 live), on a small
 * built-in synth and out of the ALSA port "CheatChord MIDI Out" that any other track can take as its MIDI input.
 *
 * Threads: the wrapper serialises every engine call per instance. Scoring candidates and writing MIDI files run on a
 * worker thread of the instance's own; results are picked up in render() with a trylock. */
#include "compat.h"       /* desktop: threads, atomics, UTF-8 files on every platform */
#include "cc_desktop.h"
#include "engine.h"
#include "harmony.h"
#include "library.h"
#include "artists.h"
#include "refs.h"
#include "params_gen.h"
#include "seq_out.h"
#include "synth.h"
#include "theory.h"
#include "voicing.h"
#include "gen/counts.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* desktop: the host's rate (cc_set_sample_rate). g_sr: for ms_frames(), which has no instance */
static double g_sr = 44100.0;
#define SR (s->sr)
#define NSLOT 8
#define NSUG 8
#define QCAP 2048
#define TAP_DEBOUNCE_MS 120   /* desktop: a mouse sends one click (MPC touch: 380 ms against double events) */
#define QUIET_MS 700
#define ECHO_MS 80
#define HMAX 48
#define HSAVE 16
#define PPQ 96          /* 4.0: the sequencer grid, ticks per beat (a 1/64 triplet is 4 ticks, a 1/64 note 6) */
#define NPRESET (NUM_FACTORY_PRESETS + NUM_USER_PRESETS)

/* 4.0: genre grooves (tools/cc_data.py): a pattern of steps, per_beat steps in one beat */
typedef struct { const char *name; int per_beat, steps; const char *pat; } groove_t;
/* factory presets (tools/cc_data.py) */
typedef struct { int deg, q; } pchord_t;
typedef struct {
    const char *name, *desc;
    int key, scale, style, artist, producer, spice, voicing, rhythm, bars, swing, sound, n;
    pchord_t c[8];
} fpreset_t;
#include "gen/grooves_gen.inc"
#include "gen/presets_gen.inc"

/* ------------------------------------------------------------------------------------------------ types */
typedef struct { chord_t c; int filled, var, len, rev; } slot_t;   /* rev (4.1): the chord swells in reverse */

enum { SRC_SEQ = 1, SRC_LIVE = 2 };
typedef struct {
    int64_t frame;
    uint32_t gen;
    int32_t len;              /* a reversed note: samples until its end (the synth swells over them) */
    uint8_t on, ch, note, vel, src, rev;   /* on: 1 note on, 0 note off, 2 controller (note = cc, vel = value) */
} qev_t;

typedef struct {
    qev_t e[QCAP];
    int n;
} queue_t;

enum { JOB_POOL, JOB_GENERATE, JOB_EXPORT, JOB_REFIT, JOB_CRUISE, JOB_FILE, NJOB };
enum { FILE_SCAN, FILE_SAVE, FILE_LOAD };   /* JOB_FILE: user presets on disk (never in the audio path) */
#define GAVOID 12                           /* GENERATE never repeats one of the last progressions */
#define NSET 9   /* the settings a re-fit depends on: key scale style artist producer spice style/artist/producer mix */

typedef struct {
    int seq;
    int key, scale, style, len, artist, producer;   /* artist, producer: profile indices */
    float spice;
    chord_t prev, prev2, next, loop_to;
    int has_prev, has_prev2, has_next, has_loop;
    unsigned char seen[HARMONY_GROUPS];
    uint32_t rng;
    chord_t avoid[NSUG];
    int navoid;
    float mix[3];                 /* STYLE / ARTIST / PRODUCER influence, 1.0 = 50 % */
    /* JOB_REFIT: the progression to re-fit, from the settings bset to the job's settings */
    slot_t base[NSLOT], cur[NSLOT];   /* base: the source chords re-fit from; cur: the progression as it is now */
    int count, whole, cursor, which, bset[NSET], job_set[NSET], prog_rev, L;
    char label[40], ck[16];
    /* JOB_CRUISE: the next chord for slot target (insert: shift the rest right); apply 0 = only show it */
    int target, insert, apply;
    chord_t used[NSLOT];
    int nused;
    /* JOB_GENERATE: progressions not to write again; JOB_FILE: what to do with which user slot */
    chord_t gav[GAVOID][NSLOT];
    int gavn[GAVOID], ngav;
    int fop, fslot;
} job_t;

#define FIO_MAX 12288
typedef struct {            /* JOB_FILE data (heap): the preset text going to or coming from disk, the slots' names */
    char text[FIO_MAX];
    char names[NUM_USER_PRESETS][48];
    char msg[96];
    int ok;
} fio_t;

#define EX_MAX 4096
typedef struct {   /* a MIDI file to write: filled by the engine, read by the worker */
    char path[512];
    int nev;
    struct { double t, len; uint8_t ch, note, vel; } ev[EX_MAX];
    double bpm, total;
} export_t;

typedef struct {
    int seq;
    chord_t pool[NSUG];
    int pool_n;
    unsigned char seen[HARMONY_GROUPS];
    uint32_t rng;
    chord_t gen[16];
    int gen_n;
    char msg[96];
    job_t job;                    /* REFIT / CRUISE: the request it answers */
    slot_t slots[NSLOT];          /* REFIT result */
    chord_t next;                 /* CRUISE result */
} result_t;

typedef struct { uint8_t ch, note; uint32_t g; } pvn_t;

typedef struct inst {
    int p[NUM_P];
    slot_t slot[NSLOT];
    int count, cursor;
    voiced_t vc[NSLOT];

    chord_t pool[NSUG];
    int pool_n, sel;
    int64_t sel_at, tap_at[3][NSUG], quiet_until;
    unsigned char seen[HARMONY_GROUPS];
    uint32_t rng;

    /* transport / sequencing */
    int64_t now;                  /* frames rendered */
    int playing, play_slot, port_ok;
    double sent_to, bpm, last_ppq;
    queue_t aq, sq;               /* ALSA queue, synth queue */
    uint32_t gen[16][128];
    unsigned char alsa_on[16][128], syn_on[128];
    /* pads held: notes each pad started */
    struct { int active; int n; uint8_t ch[MAX_VOICE + 1], note[MAX_VOICE + 1]; } pad[16];
    /* echo guard: notes we sent that will come back into our own track */
    unsigned char pend_on[128], pend_off[128];
    int64_t pend_at[128];

    synth_t syn;

    /* texts */
    char msg[96];
    int64_t msg_until;
    char export_msg[96];
    int display_rev;
    int learn, last_note, last_pad;
    int listening, host_was;
    double int_ppq;

    /* 2.0: the slot the ideas are for (count = after the last chord); cursor = the selected (previewed) chord */
    int target;
    /* history: a log of progression states; undo/redo move through it, a new change is appended (nothing is lost) */
    struct { slot_t slot[NSLOT]; int count, key, scale, len, style, artist, producer, spice, mix[3]; char label[40], ck[16]; } hist[HMAX];
    int hn, hpos;
    int64_t hist_at;
    /* the chord the little keyboard shows */
    chord_t shown;
    int shown_ok, kb[12];
    /* 3.0: the big keyboard (6 octaves from kb_lo) with the voiced notes as they sound */
    voiced_t shown_v;
    int kbx[72], kb_lo, kb_out, prev_panel;

    /* worker: one job slot per kind, so a pool request never overwrites a pending GENERATE */
    pthread_t th;
    pthread_mutex_t m;
    pthread_cond_t cv;
    int quit, job_seq, pool_seq;
    int pending[NJOB];
    job_t job[NJOB];
    result_t res[NJOB];
    atomic_int ready[NJOB];
    export_t *ex;                 /* heap */
    atomic_int ex_busy, working;
    int want_pool, fresh;
    /* 4.0 */
    voiced_t vsrc[NSLOT];         /* the source chords voiced like the progression: the keyboards show what changed */
    int src_ok_v, kb_cmp;         /* kb_cmp: the keyboard compares the shown chord with its source (an influence change) */
    char kb_diff[96];
    uint32_t pv_gen;              /* preview mute group: the notes of the last tap or idea pad, cut by the next one */
    pvn_t pv[MAX_VOICE + 1];
    int pv_n;
    int learn_first;              /* LEARN: the first pad's note while waiting for the last */
    int64_t seq_until[128];       /* 4.1: until when each note of the sequence sounds (the MORPH while it plays) */
    int padmap[16], padmap_n;     /* 4.1: learned pads by note (any pad layout); 0 = PAD 1 NOTE + PADS USED */
    int learn_seen[16], learn_nseen, learn_last, learn_chrom, learn_multi;
    int64_t learn_at;             /* the last LEARN tap: 4 s of quiet ends it */
    int64_t pad_hit_at;           /* the chord guard: a pad layout that sends chords triggers one pad, not several */
    int pad_trig[16];             /* the note that started each held pad (only its note-off releases it) */
    int grp_note[16], grp_vel[16], ngrp;   /* the pad notes of this audio block */
    fio_t *fio;                   /* heap */
    char unames[NUM_USER_PRESETS][48];
    int fio_busy, preset_loaded;
    int64_t tap_pre[NPRESET];
    /* 3.5: async re-fit and CRUISE, loop sync */
    int prog_rev, rf_seq, rf_pending, cr_seq, want_cruise, cr_ok, cr_target, cr_insert, cr_rev, cr_cursor;
    job_t rf_job;                 /* the last re-fit posted (its base is the base of the next while it runs) */
    chord_t cr_next;
    double host_loop, loop_start, anchor, last_end, hp_last;
    int cr_apply;                 /* a CRUISE press is waiting for its chord */
    /* the source progression: the chords as last chosen or edited, and the settings they were made with. Browser
     * re-fits always start from it, so moving a control back brings the same chords back (reproducible) */
    slot_t src[NSLOT];
    int src_n, src_set[NSET], in_refit;
    int64_t tap_br[14];           /* browser / library tiles: debounce */
    result_t *rtmp;               /* heap: a copy of a REFIT / CRUISE result while it is applied */
    int loop_src;                 /* 0 none, 1 detected from MPC's loop wrap, 2 from the host's cycle info */
    int busy_gen;
    /* desktop */
    double sr;
    cc_midi_sink_t sink;
    void *sink_ctx;
    int64_t out_frame;            /* the frame of the event q_run is sending (0: now) */
} inst_t;

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int strum_pos(const inst_t *s, int i, int n, long hit, long k);
static int64_t ms_frames(int ms) { return (int64_t)(ms * g_sr / 1000.0); }

/* ------------------------------------------------------------------------------------------------ worker */
static void write_midi(const export_t *x, char *msg, int msglen) {
    enum { TPQ = 480 };
    typedef struct { uint32_t tick; uint8_t st, d1, d2; } mev_t;
    mev_t *E = malloc(sizeof(mev_t) * (2 * EX_MAX + 2));
    uint8_t *trk = malloc(2 * EX_MAX * 7 + 128);
    FILE *f = (E && trk) ? cc_fopen(x->path, "wb") : NULL;
    if (!f) {
        snprintf(msg, msglen, "EXPORT FAILED: %s", strerror(errno));
        free(E); free(trk);
        return;
    }
    int ne = 0;
    for (int i = 0; i < x->nev; i++) {
        uint32_t on = (uint32_t)lround(x->ev[i].t * TPQ), off = (uint32_t)lround((x->ev[i].t + x->ev[i].len) * TPQ);
        if (off <= on) off = on + 1;
        E[ne++] = (mev_t){on, (uint8_t)(0x90 | x->ev[i].ch), x->ev[i].note, x->ev[i].vel};
        E[ne++] = (mev_t){off, (uint8_t)(0x80 | x->ev[i].ch), x->ev[i].note, 0};
    }
    for (int i = 1; i < ne; i++) {   /* by tick, offs before ons at the same tick */
        mev_t v = E[i];
        int j = i - 1;
        while (j >= 0 && (E[j].tick > v.tick || (E[j].tick == v.tick && (E[j].st & 0xf0) == 0x90 && (v.st & 0xf0) == 0x80))) {
            E[j + 1] = E[j];
            j--;
        }
        E[j + 1] = v;
    }
    int tn = 0;
    uint32_t usq = (uint32_t)(60000000.0 / (x->bpm > 20 ? x->bpm : 120));
    uint8_t tempo[] = {0x00, 0xFF, 0x51, 0x03, (uint8_t)(usq >> 16), (uint8_t)(usq >> 8), (uint8_t)usq};
    memcpy(trk + tn, tempo, sizeof tempo);
    tn += sizeof tempo;
    const char *nm = "CheatChord";
    trk[tn++] = 0; trk[tn++] = 0xFF; trk[tn++] = 0x03; trk[tn++] = (uint8_t)strlen(nm);
    memcpy(trk + tn, nm, strlen(nm));
    tn += (int)strlen(nm);
    uint32_t last = 0;
    for (int i = 0; i <= ne; i++) {
        uint32_t tick = i < ne ? E[i].tick : (uint32_t)lround(x->total * TPQ);
        uint32_t d = tick > last ? tick - last : 0;
        last = tick > last ? tick : last;
        uint8_t vb[5];
        int vn = 0;
        vb[vn++] = d & 0x7f;
        while (d >>= 7) vb[vn++] = 0x80 | (d & 0x7f);
        while (vn) trk[tn++] = vb[--vn];
        if (i < ne) { trk[tn++] = E[i].st; trk[tn++] = E[i].d1; trk[tn++] = E[i].d2; }
        else { trk[tn++] = 0xFF; trk[tn++] = 0x2F; trk[tn++] = 0x00; }
    }
    uint8_t hdr[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, TPQ >> 8, TPQ & 0xff,
                     'M', 'T', 'r', 'k', (uint8_t)(tn >> 24), (uint8_t)(tn >> 16), (uint8_t)(tn >> 8), (uint8_t)tn};
    int ok = fwrite(hdr, 1, sizeof hdr, f) == sizeof hdr && fwrite(trk, 1, tn, f) == (size_t)tn;
    ok &= fclose(f) == 0;
    const char *base = strrchr(x->path, '/');
    snprintf(msg, msglen, ok ? "SAVED %s" : "EXPORT FAILED (WRITE)", base ? base + 1 : x->path);
    free(E);
    free(trk);
}

/* ------------------------------------------------------------------------------------------------ worker jobs */
static void hctx_of(const job_t *J, hctx_t *h) {
    memset(h, 0, sizeof *h);
    h->key = J->key; h->scale = J->scale; h->style = J->style; h->spice = J->spice;
    h->artist = J->artist; h->producer = J->producer;
    h->mixed = 1; h->wstyle = J->mix[0]; h->wartist = J->mix[1]; h->wprod = J->mix[2];
}

/* EDIT CHORD / EDIT ALL re-fit: the selected chord or every chord moved from the settings bset to the job's */
static void do_refit(const job_t *J, result_t *R) {
    R->job = *J;
    memcpy(R->slots, J->cur, sizeof R->slots);
    int n = J->count, c = J->cursor, bk = J->bset[0], bsc = J->bset[1];
    int recolour = 0;
    for (int i = 1; i < NSET; i++) if (i != 0 && J->bset[i] != J->job_set[i]) recolour = 1;
    for (int k = 0; k < n; k++) {
        if (!J->whole && k != c) continue;
        hctx_t h;
        hctx_of(J, &h);
        chord_t nx;
        if (k > 0) h.prev = &R->slots[k - 1].c;
        if (k > 1) h.prev2 = &R->slots[k - 2].c;
        if (k + 1 < n) {
            if (J->whole) { nx = harmony_refit(&h, bk, bsc, &J->base[k + 1].c, 0); h.next = &nx; }   /* its new root, old colour */
            else h.next = &R->slots[k + 1].c;
        }
        if (k == J->L - 1 && k > 0) h.loop_to = &R->slots[0].c;
        R->slots[k].c = harmony_refit(&h, bk, bsc, &J->base[k].c, recolour);
    }
}

/* CRUISE: the next chord after the one before the target slot */
static void do_cruise(const job_t *J, result_t *R) {
    R->job = *J;
    hctx_t h;
    hctx_of(J, &h);
    if (J->has_prev) h.prev = &J->prev;
    if (J->has_prev2) h.prev2 = &J->prev2;
    if (J->has_next) h.next = &J->next;
    if (J->has_loop) h.loop_to = &J->loop_to;
    R->next = harmony_next(&h, J->used, J->nused);
}

/* ------------------------------------------------------------------------------------------------ user presets on disk */
static int dir_ok(const char *d);
/* MPC Documents/CheatChord/Presets on the internal drive (else next to the plugin); created when missing */
static void preset_dir(char *dir, int len) {   /* desktop: <docs root>/Presets (Documents/JJ DA PLUG/CheatChord) */
    snprintf(dir, len, "%s/Presets", cc_docs_root());
    cc_mkdirs(dir);
}

static void file_job(const job_t *J, fio_t *F) {
    char dir[400], path[480];
    preset_dir(dir, sizeof dir);
    F->ok = 0;
    F->msg[0] = 0;
    if (J->fop == FILE_SAVE) {
        char tmp[500];
        snprintf(path, sizeof path, "%s/User %02d.ccp", dir, J->fslot + 1);
        snprintf(tmp, sizeof tmp, "%s.part", path);
        FILE *f = cc_fopen(tmp, "w");
        int ok = f && fputs(F->text, f) >= 0;
        if (f) ok &= fclose(f) == 0;
        ok = ok && cc_rename_replace(tmp, path) == 0;
        F->ok = ok;
        if (ok) snprintf(F->msg, sizeof F->msg, "SAVED AS USER %d (MPC DOCUMENTS/CHEATCHORD/PRESETS)", J->fslot + 1);
        else snprintf(F->msg, sizeof F->msg, "SAVE FAILED: %s", strerror(errno));
    } else if (J->fop == FILE_LOAD) {
        snprintf(path, sizeof path, "%s/User %02d.ccp", dir, J->fslot + 1);
        FILE *f = cc_fopen(path, "r");
        size_t n = f ? fread(F->text, 1, FIO_MAX - 1, f) : 0;
        if (f) fclose(f);
        F->text[n] = 0;
        F->ok = n > 0;
        if (!F->ok) snprintf(F->msg, sizeof F->msg, "USER %d IS EMPTY - SAVE A PRESET THERE FIRST", J->fslot + 1);
    }
    for (int i = 0; i < NUM_USER_PRESETS; i++) {   /* every job refreshes the names: "name=..." is the first field */
        snprintf(path, sizeof path, "%s/User %02d.ccp", dir, i + 1);
        FILE *f = cc_fopen(path, "r");
        F->names[i][0] = 0;
        if (!f) continue;
        char line[160] = "";
        if (fgets(line, sizeof line, f) && !strncmp(line, "name=", 5)) {
            char *e = strchr(line, ';');
            if (e) *e = 0;
            snprintf(F->names[i], sizeof F->names[i], "%.47s", line + 5);
        } else snprintf(F->names[i], sizeof F->names[i], "USER %d", i + 1);
        fclose(f);
    }
}

static void *worker(void *arg) {
    inst_t *s = arg;
    job_t *J = malloc(sizeof *J);
    result_t *R = malloc(sizeof *R);
    if (!J || !R) { free(J); free(R); return NULL; }
    pthread_mutex_lock(&s->m);
    for (;;) {
        int k = -1;
        while (!s->quit) {
            if (s->pending[JOB_GENERATE]) k = JOB_GENERATE;
            else if (s->pending[JOB_REFIT]) k = JOB_REFIT;
            else if (s->pending[JOB_CRUISE]) k = JOB_CRUISE;
            else if (s->pending[JOB_POOL]) k = JOB_POOL;
            else if (s->pending[JOB_EXPORT]) k = JOB_EXPORT;
            else if (s->pending[JOB_FILE]) k = JOB_FILE;
            if (k >= 0) break;
            pthread_cond_wait(&s->cv, &s->m);
        }
        if (s->quit) break;
        memcpy(J, &s->job[k], sizeof *J);
        s->pending[k] = 0;
        atomic_store(&s->working, 1);
        pthread_mutex_unlock(&s->m);

        memset(R, 0, sizeof *R);
        R->seq = J->seq;
        if (k == JOB_POOL) {
            hctx_t hc = {J->key, J->scale, J->style, J->spice, J->has_prev ? &J->prev : NULL, J->has_prev2 ? &J->prev2 : NULL,
                         J->has_next ? &J->next : NULL, J->has_loop ? &J->loop_to : NULL, J->artist, J->producer,
                         1, J->mix[0], J->mix[1], J->mix[2]};
            memcpy(R->seen, J->seen, sizeof R->seen);
            R->rng = J->rng;
            R->pool_n = harmony_pool(&hc, R->pool, NSUG, R->seen, &R->rng, J->avoid, J->navoid);
        } else if (k == JOB_GENERATE) {
            R->rng = J->rng;
            for (int tries = 0; tries < 12; tries++) {   /* 4.0: never one of the last progressions again */
                harmony_generate(J->key, J->scale, J->style, J->artist, J->producer, J->mix, J->spice, J->len, R->gen, &R->rng);
                int same = 0;
                for (int g = 0; g < J->ngav && !same; g++) {
                    if (J->gavn[g] != J->len) continue;
                    int eq = 1;
                    for (int k2 = 0; k2 < J->len && eq; k2++) eq = chord_equal(&J->gav[g][k2], &R->gen[k2]);
                    same = eq;
                }
                if (!same) break;
            }
            R->gen_n = J->len;
        } else if (k == JOB_REFIT) {
            do_refit(J, R);
        } else if (k == JOB_CRUISE) {
            do_cruise(J, R);
        } else if (k == JOB_FILE) {
            R->job = *J;
            file_job(J, s->fio);
        } else {
            write_midi(s->ex, R->msg, sizeof R->msg);
        }
        pthread_mutex_lock(&s->m);
        memcpy(&s->res[k], R, sizeof *R);
        atomic_store(&s->ready[k], 1);
        atomic_store(&s->working, 0);
        if (k == JOB_EXPORT) atomic_store(&s->ex_busy, 0);
    }
    pthread_mutex_unlock(&s->m);
    free(J);
    free(R);
    return NULL;
}

static int post_job(inst_t *s, int kind, const job_t *j) {
    pthread_mutex_lock(&s->m);
    memcpy(&s->job[kind], j, sizeof *j);
    int seq = s->job[kind].seq = ++s->job_seq;
    s->pending[kind] = 1;
    pthread_cond_signal(&s->cv);
    pthread_mutex_unlock(&s->m);
    return seq;
}

/* ------------------------------------------------------------------------------------------------ helpers */
static void say(inst_t *s, int ms, const char *fmt, ...) CC_PRINTF(3, 4);
#include <stdarg.h>
static void say(inst_t *s, int ms, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s->msg, sizeof s->msg, fmt, ap);
    va_end(ap);
    s->msg_until = s->now + ms_frames(ms);
    s->display_rev++;
}

static void name_of(const inst_t *s, const chord_t *c, char *buf, int len) {
    chord_name_key(c, s->p[P_KEY], s->p[P_SCALE], buf, len);
}

static int act_len(const inst_t *s) { return s->p[P_GEN_LEN] ? 8 : 4; }   /* the loop length: 4 or 8 chords */
static int play_count(const inst_t *s) { return s->count < act_len(s) ? s->count : act_len(s); }
static int artist_prof(const inst_t *s) { return ARTIST_LIST[clampi(s->p[P_ARTIST], 0, NUM_ARTIST_LIST - 1)]; }
static int producer_prof(const inst_t *s) { return PRODUCER_LIST[clampi(s->p[P_PRODUCER], 0, NUM_PRODUCER_LIST - 1)]; }
static float mixw(int v) { return clampi(v, 0, 100) / 50.0f; }   /* 50 % = 1.0 (normal), 100 % = 2.0 */
static void settings_of(const inst_t *s, int *o) {
    o[0] = s->p[P_KEY]; o[1] = s->p[P_SCALE]; o[2] = s->p[P_STYLE]; o[3] = s->p[P_ARTIST]; o[4] = s->p[P_PRODUCER];
    o[5] = s->p[P_SPICE]; o[6] = s->p[P_STYLE_MIX]; o[7] = s->p[P_ARTIST_MIX]; o[8] = s->p[P_PRODUCER_MIX];
}
/* a job with the current harmonic settings */
static void job_settings(const inst_t *s, job_t *j) {
    j->key = s->p[P_KEY]; j->scale = s->p[P_SCALE]; j->style = s->p[P_STYLE];
    j->artist = artist_prof(s); j->producer = producer_prof(s);
    j->spice = s->p[P_SPICE] / 100.0f;
    j->mix[0] = mixw(s->p[P_STYLE_MIX]); j->mix[1] = mixw(s->p[P_ARTIST_MIX]); j->mix[2] = mixw(s->p[P_PRODUCER_MIX]);
    settings_of(s, j->job_set);
}

static int recount(inst_t *s) {
    int n = 0;
    while (n < NSLOT && s->slot[n].filled) n++;
    for (int i = n; i < NSLOT; i++) s->slot[i].filled = 0;
    s->count = n;
    s->cursor = clampi(s->cursor, 0, n < NSLOT ? n : NSLOT - 1);
    s->p[P_CURSOR] = s->cursor + 1;
    int L = act_len(s);
    s->target = clampi(s->target, 0, n < L ? n : L - 1);
    return n;
}

static void voice_all(const inst_t *s, const slot_t *sl, int n, voiced_t *vc) {
    int lead = s->p[P_LEAD], st = s->p[P_VOICING], oct = s->p[P_OCTAVE] - 2, bass = s->p[P_BASS];
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < n; i++) {
            const voiced_t *pv = (i > 0) ? &vc[i - 1] : (pass ? &vc[n - 1] : NULL);
            voice_chord(&sl[i].c, st, oct, lead, sl[i].var, bass, pv, &vc[i]);
        }
}

/* the progression as it sounds, and its source chords voiced the same way (the keyboards compare the two) */
static void revoice(inst_t *s) {
    voice_all(s, s->slot, s->count, s->vc);
    if (s->src_n == s->count && s->count) { voice_all(s, s->src, s->src_n, s->vsrc); s->src_ok_v = 1; }
    else s->src_ok_v = 0;
}

/* the keyboards: the small one shows pitch classes (bass marked), the big one every note as it sounds (6 octaves
 * from the C below the lowest note). v: the voiced notes (NULL: voiced here with the current settings).
 * was: the same chord before an influence change (NULL: nothing to compare): notes it added light up as New, notes it
 * took away stay as Gone, so turning SPICE / STYLE / ARTIST / PRODUCER shows on the keys what it changed. */
static void note_name(const inst_t *s, int n, char *buf, int len);
static int voiced_eq(const voiced_t *a, const voiced_t *b) {
    if (a->n != b->n || a->bass != b->bass) return 0;
    for (int i = 0; i < a->n; i++) if (a->note[i] != b->note[i]) return 0;
    return 1;
}
static void show_chord_cmp(inst_t *s, const chord_t *c, const voiced_t *v, const voiced_t *was) {
    voiced_t tmp;
    memset(&tmp, 0, sizeof tmp);
    if (!v) {
        voice_chord(c, s->p[P_VOICING], s->p[P_OCTAVE] - 2, 0, 0, s->p[P_BASS], NULL, &tmp);
        v = &tmp;
    }
    if (was && voiced_eq(was, v)) was = NULL;   /* nothing changed: plain */
    if (s->shown_ok && chord_equal(&s->shown, c) && voiced_eq(&s->shown_v, v) && (was != NULL) == s->kb_cmp) return;
    s->shown = *c;
    s->shown_v = *v;
    s->shown_ok = 1;
    s->kb_cmp = was != NULL;
    int has[128] = {0}, had[128] = {0};
    for (int i = 0; i < v->n; i++) has[v->note[i]] |= 1;
    if (v->bass >= 0) has[v->bass] |= 2;
    if (was) {
        for (int i = 0; i < was->n; i++) had[was->note[i]] |= 1;
        if (was->bass >= 0) had[was->bass] |= 2;
    }
    /* small keyboard: pitch classes */
    int now_pc[12] = {0}, was_pc[12] = {0}, b = c->bass >= 0 ? c->bass : c->root;
    for (int n = 0; n < 128; n++) { if (has[n]) now_pc[n % 12] = 1; if (had[n]) was_pc[n % 12] = 1; }
    for (int i = 0; i < 12; i++) {
        if (now_pc[i]) s->kb[i] = i == b ? 2 : (was && !was_pc[i]) ? 3 : 1;
        else s->kb[i] = (was && was_pc[i]) ? 4 : 0;
    }
    /* big keyboard: a 6-octave window over everything shown (now and before) */
    int lo = 127, hi = 0;
    for (int n = 0; n < 128; n++) if (has[n] || had[n]) { if (n < lo) lo = n; if (n > hi) hi = n; }
    if (lo > hi) lo = hi = 60;
    int start = lo / 12 * 12;
    if (hi - start >= 72) start = (hi - 71 + 11) / 12 * 12;   /* wider than 6 octaves: keep the top */
    s->kb_lo = start;
    s->kb_out = 0;   /* notes below the window: shown octaves up (the note list keeps their real octave) */
    memset(s->kbx, 0, sizeof s->kbx);
    for (int pass = 0; pass < 2; pass++)   /* Gone first, so a sounding note wins a shared key */
        for (int n = 0; n < 128; n++) {
            int st = 0;
            if (pass == 0 && had[n] && !has[n]) st = 4;
            if (pass == 1 && has[n]) st = (has[n] & 2) ? 2 : (was && !had[n]) ? 3 : 1;
            if (!st) continue;
            int k = n - start;
            if (k < 0) { if (pass) s->kb_out++; while (k < 0) k += 12; }
            if (k < 72 && (pass || !s->kbx[k])) s->kbx[k] = st;
        }
    /* the change as text: "+E5 +F#5  -D5" */
    s->kb_diff[0] = 0;
    if (was) {
        int k = 0, any = 0;
        char nn[8];
        for (int n = 0; n < 128 && k < (int)sizeof s->kb_diff - 8; n++)
            if (has[n] && !had[n]) { note_name(s, n, nn, sizeof nn); k += snprintf(s->kb_diff + k, sizeof s->kb_diff - k, "%s+%s", any ? " " : "", nn); any = 1; }
        for (int n = 0; n < 128 && k < (int)sizeof s->kb_diff - 8; n++)
            if (had[n] && !has[n]) { note_name(s, n, nn, sizeof nn); k += snprintf(s->kb_diff + k, sizeof s->kb_diff - k, "%s-%s", any ? "  " : "", nn); any = 1; }
    }
    s->display_rev++;
}
static void show_chord(inst_t *s, const chord_t *c, const voiced_t *v) { show_chord_cmp(s, c, v, NULL); }
/* a chord of the progression: compared with its source chord when an influence change moved it */
static void show_slot(inst_t *s, int i) {
    if (i < 0 || i >= s->count) return;
    show_chord_cmp(s, &s->slot[i].c, &s->vc[i], s->src_ok_v && i < s->src_n ? &s->vsrc[i] : NULL);
}

static void note_name(const inst_t *s, int n, char *buf, int len) {
    snprintf(buf, len, "%s%d", pc_name_key(n % 12, s->p[P_KEY], s->p[P_SCALE], 0), clampi(n, 0, 127) / 12 - 1);
}

static void request_pool(inst_t *s) {
    s->want_pool = 1;
}

static void send_pool_job(inst_t *s) {
    job_t j;
    memset(&j, 0, sizeof j);
    job_settings(s, &j);
    int c = s->target, L = act_len(s), n = play_count(s);
    if (c > 0 && s->slot[c - 1].filled) { j.prev = s->slot[c - 1].c; j.has_prev = 1; }
    if (c > 1 && s->slot[c - 2].filled) { j.prev2 = s->slot[c - 2].c; j.has_prev2 = 1; }
    if (c < n && c + 1 < n) { j.next = s->slot[c + 1].c; j.has_next = 1; }
    /* the last chord of the loop leads back to the first */
    if (c > 0 && s->slot[0].filled && c == L - 1) {
        j.loop_to = s->slot[0].c;
        j.has_loop = 1;
    }
    memcpy(j.seen, s->seen, sizeof j.seen);
    memcpy(j.avoid, s->pool, sizeof j.avoid);   /* a reroll never deals the same set again */
    j.navoid = s->fresh ? 0 : s->pool_n;
    s->fresh = 0;
    j.rng = rng_next(&s->rng);
    s->pool_seq = post_job(s, JOB_POOL, &j);
    s->want_pool = 0;
}

static void new_spot(inst_t *s) {   /* the edit slot or its context changed: forget what was shown */
    memset(s->seen, 0, sizeof s->seen);
    s->fresh = 1;
    s->sel = -1;
    request_pool(s);
    s->display_rev++;
}

/* ------------------------------------------------------------------------------------------------ note output */
static void q_push_x(queue_t *q, int64_t frame, int on, int ch, int note, int vel, uint32_t gen, int src, int rev, int32_t len) {
    if (q->n >= QCAP || note < 0 || note > 127) return;
    q->e[q->n++] = (qev_t){frame, gen, len, (uint8_t)on, (uint8_t)ch, (uint8_t)note, (uint8_t)vel, (uint8_t)src, (uint8_t)rev};
}
static void q_push(queue_t *q, int64_t frame, int on, int ch, int note, int vel, uint32_t gen, int src) {
    q_push_x(q, frame, on, ch, note, vel, gen, src, 0, 0);
}

/* the built-in sound plays when MIDI OUT is off, or with MIDI OUT when SYNTH WITH MIDI is on */
static int synth_on(const inst_t *s) { return !s->p[P_MIDI_OUT] || s->p[P_SYNTH]; }

/* one note with its off: both outputs. rev: the synth swells it in reverse over its length */
static void sched_note_x(inst_t *s, int64_t on_frame, int64_t off_frame, int ch, int note, int vel, int src, int rev) {
    if (note < 0 || note > 127) return;
    if (vel < 1) vel = 1;
    if (vel > 127) vel = 127;
    uint32_t g = ++s->gen[ch & 15][note];
    if (off_frame <= on_frame) off_frame = on_frame + 64;
    if (src == SRC_SEQ) { if (off_frame > s->seq_until[note]) s->seq_until[note] = off_frame; }
    if (s->p[P_MIDI_OUT]) {
        q_push(&s->aq, on_frame, 1, ch, note, vel, g, src);
        q_push(&s->aq, off_frame, 0, ch, note, 0, g, src);
    }
    if (synth_on(s)) {
        q_push_x(&s->sq, on_frame, 1, ch, note, vel, g, src, rev, (int32_t)(off_frame - on_frame));
        q_push(&s->sq, off_frame, 0, ch, note, 0, g, src);
    }
}
static void sched_note(inst_t *s, int64_t on_frame, int64_t off_frame, int ch, int note, int vel, int src) {
    sched_note_x(s, on_frame, off_frame, ch, note, vel, src, 0);
}
/* REVERSE on the MIDI port: an expression swell (CC 11 from 0 to 127 over the note) for the sound on the other track */
static void sched_swell(inst_t *s, int64_t on_frame, int64_t off_frame, int ch, int src) {
    if (!s->p[P_MIDI_OUT] || off_frame <= on_frame) return;
    enum { STEPS = 16 };
    for (int k = 0; k <= STEPS; k++) {
        double x = k / (double)STEPS;
        q_push(&s->aq, on_frame + (int64_t)((off_frame - on_frame) * x * 0.98), 2, ch, 11, (int)lround(127 * x * x * x), 0, src);
    }
    q_push(&s->aq, off_frame + 32, 2, ch, 11, 127, 0, src);   /* back to full for the chords after it */
}

/* desktop: the notes go to the host's MIDI output at their frame in this block (no echo comes back into a DAW track) */
static void host_out(inst_t *s, int st, int d1, int d2) {
    int off = s->out_frame > s->now ? (int)(s->out_frame - s->now) : 0;
    if (s->sink) s->sink(s->sink_ctx, off, (uint8_t)st, (uint8_t)d1, (uint8_t)d2);
    else if ((st & 0xf0) == 0xb0) seq_out_cc(st & 15, d1, d2);   /* tests */
    else seq_out_note(st & 15, d1, (st & 0xf0) == 0x90 ? d2 : 0);
}
static void alsa_send(inst_t *s, int ch, int note, int vel) {
    host_out(s, (vel > 0 ? 0x90 : 0x80) | (ch & 15), note, vel > 0 ? vel : 0);
}

/* dispatch due events from a queue: ALSA (dispatch = 1) or synth (dispatch = 2) */
static void q_run(inst_t *s, queue_t *q, int64_t upto, int which) {
    /* stable selection of due events in time order */
    for (;;) {
        int bi = -1;
        for (int i = 0; i < q->n; i++)
            if (q->e[i].frame < upto && (bi < 0 || q->e[i].frame < q->e[bi].frame ||
                                         (q->e[i].frame == q->e[bi].frame && !q->e[i].on && q->e[bi].on)))
                bi = i;
        if (bi < 0) break;
        qev_t e = q->e[bi];
        q->e[bi] = q->e[--q->n];
        s->out_frame = e.frame;
        if (e.on == 2) {   /* a controller: the port only (the synth swells by itself) */
            if (which == 1) host_out(s, 0xb0 | (e.ch & 15), e.note, e.vel);
            continue;
        }
        if (which == 1) {
            if (e.on) {
                if (s->alsa_on[e.ch][e.note]) alsa_send(s, e.ch, e.note, 0);   /* retrigger */
                alsa_send(s, e.ch, e.note, e.vel);
                s->alsa_on[e.ch][e.note] = 1;
            } else if (s->alsa_on[e.ch][e.note] && e.gen == s->gen[e.ch][e.note]) {
                alsa_send(s, e.ch, e.note, 0);
                s->alsa_on[e.ch][e.note] = 0;
            }
        } else {
            if (e.on && e.rev) synth_note_on_rev(&s->syn, e.note, e.vel, e.len);
            else if (e.on) { synth_note_on(&s->syn, e.note, e.vel); s->syn_on[e.note] = (uint8_t)(e.gen & 0xff) | 1; }
            else if (e.gen == s->gen[e.ch][e.note]) synth_note_off(&s->syn, e.note);
        }
    }
    s->out_frame = 0;
}

static void all_notes_off(inst_t *s, int src_mask) {
    if (src_mask & SRC_SEQ) memset(s->seq_until, 0, sizeof s->seq_until);
    /* drop pending ons of those sources, play their offs now */
    for (int qi = 0; qi < 2; qi++) {
        queue_t *q = qi ? &s->sq : &s->aq;
        int k = 0;
        for (int i = 0; i < q->n; i++) {
            qev_t e = q->e[i];
            if (e.src & src_mask) {
                if (e.on) continue;
                e.frame = s->now;
            }
            q->e[k++] = e;
        }
        q->n = k;
    }
    q_run(s, &s->aq, s->now + 1, 1);
    if (src_mask == (SRC_SEQ | SRC_LIVE)) {
        for (int ch = 0; ch < 16; ch++)
            for (int n = 0; n < 128; n++)
                if (s->alsa_on[ch][n]) { alsa_send(s, ch, n, 0); s->alsa_on[ch][n] = 0; }
        synth_all_off(&s->syn, 0);
        memset(s->pad, 0, sizeof s->pad);
    }
}

static int chord_ch(const inst_t *s) { return clampi(s->p[P_CHANNEL], 1, 16) - 1; }
static int bass_ch(const inst_t *s) { return s->p[P_BASS_CH] ? clampi(s->p[P_BASS_CH], 1, 16) - 1 : chord_ch(s); }

/* PREVIEW MUTE GROUP: a tapped chord or idea (and the idea pads 9-16) cuts the one before, like pads in one mute group */
static void choke_preview(inst_t *s) {
    for (int qi = 0; qi < 2; qi++) {   /* a strummed note not started yet: never starts */
        queue_t *q = qi ? &s->sq : &s->aq;
        int k = 0;
        for (int j = 0; j < q->n; j++) {
            qev_t e = q->e[j];
            int mine = 0;
            for (int i = 0; i < s->pv_n; i++) if (e.on && e.ch == s->pv[i].ch && e.note == s->pv[i].note && e.gen == s->pv[i].g) mine = 1;
            if (!mine) q->e[k++] = e;
        }
        q->n = k;
    }
    for (int i = 0; i < s->pv_n; i++) {
        int ch = s->pv[i].ch, n = s->pv[i].note;
        if (s->gen[ch][n] != s->pv[i].g) continue;   /* the note was played again since: not ours to cut */
        q_push(&s->aq, s->now, 0, ch, n, 0, s->pv[i].g, SRC_LIVE);
        q_push(&s->sq, s->now, 0, ch, n, 0, s->pv[i].g, SRC_LIVE);
    }
    s->pv_n = 0;
}

/* play a voiced chord now (pads, auditions); hold = 0: length in frames, else held until release.
 * preview: part of the preview mute group */
static void play_now_g(inst_t *s, const voiced_t *v, int vel, int64_t len, int pad_idx, int preview) {
    if (preview) choke_preview(s);
    int64_t t = s->now;
    int64_t strum = ms_frames(s->p[P_STRUM]) / (v->n > 1 ? 1 : 1);
    int64_t off = len > 0 ? t + len : t + (int64_t)SR * 600;   /* held notes: released by the pad */
    if (pad_idx >= 0) {
        s->pad[pad_idx].active = 1;
        s->pad[pad_idx].n = 0;
    }
    if (v->bass >= 0) {
        sched_note(s, t, off, bass_ch(s), v->bass, vel, SRC_LIVE);
        if (pad_idx >= 0) { int k = s->pad[pad_idx].n++; s->pad[pad_idx].ch[k] = bass_ch(s); s->pad[pad_idx].note[k] = v->bass; }
    }
    for (int i = 0; i < v->n; i++) {
        sched_note(s, t + strum * strum_pos(s, i, v->n, 0, 0), off, chord_ch(s), v->note[i], vel, SRC_LIVE);
        if (pad_idx >= 0 && s->pad[pad_idx].n < MAX_VOICE + 1) {
            int k = s->pad[pad_idx].n++;
            s->pad[pad_idx].ch[k] = chord_ch(s);
            s->pad[pad_idx].note[k] = v->note[i];
        }
    }
    if (preview) {   /* remember them for the next preview to cut */
        s->pv_n = 0;
        if (v->bass >= 0) s->pv[s->pv_n++] = (pvn_t){(uint8_t)bass_ch(s), (uint8_t)v->bass, s->gen[bass_ch(s)][v->bass]};
        for (int i = 0; i < v->n && s->pv_n < MAX_VOICE + 1; i++)
            s->pv[s->pv_n++] = (pvn_t){(uint8_t)chord_ch(s), (uint8_t)v->note[i], s->gen[chord_ch(s)][v->note[i]]};
    }
}
static void play_now(inst_t *s, const voiced_t *v, int vel, int64_t len, int pad_idx) { play_now_g(s, v, vel, len, pad_idx, 0); }

static void release_pad(inst_t *s, int pad_idx) {
    if (!s->pad[pad_idx].active) return;
    for (int i = 0; i < s->pad[pad_idx].n; i++) {
        int ch = s->pad[pad_idx].ch[i], n = s->pad[pad_idx].note[i];
        uint32_t g = s->gen[ch][n];
        q_push(&s->aq, s->now, 0, ch, n, 0, g, SRC_LIVE);
        q_push(&s->sq, s->now, 0, ch, n, 0, g, SRC_LIVE);
    }
    /* their long offs stay queued but carry an old generation once anything retriggers; drop them now */
    for (int qi = 0; qi < 2; qi++) {
        queue_t *q = qi ? &s->sq : &s->aq;
        int k = 0;
        for (int i = 0; i < q->n; i++) {
            qev_t e = q->e[i];
            if (!e.on && e.src == SRC_LIVE && e.frame > s->now + (int64_t)SR * 100) {
                int mine = 0;
                for (int j = 0; j < s->pad[pad_idx].n; j++) if (s->pad[pad_idx].note[j] == e.note && s->pad[pad_idx].ch[j] == e.ch) mine = 1;
                if (mine) continue;
            }
            q->e[k++] = e;
        }
        q->n = k;
    }
    s->pad[pad_idx].active = 0;
}

static void voice_suggestion(inst_t *s, int i, voiced_t *v) {
    int c = s->target;
    const voiced_t *pv = c > 0 && s->slot[c - 1].filled ? &s->vc[c - 1] : NULL;
    voice_chord(&s->pool[i], s->p[P_VOICING], s->p[P_OCTAVE] - 2, s->p[P_LEAD], 0, s->p[P_BASS], pv, v);
}

static void audition_slot(inst_t *s, int i) {
    if (i < 0 || i >= s->count) return;
    show_slot(s, i);
    play_now_g(s, &s->vc[i], s->p[P_VELOCITY], (int64_t)(SR * 1.4), -1, 1);
}

static void audition_sug(inst_t *s, int i) {
    if (i < 0 || i >= s->pool_n) return;
    voiced_t v;
    voice_suggestion(s, i, &v);
    show_chord(s, &s->pool[i], &v);
    play_now_g(s, &v, s->p[P_VELOCITY], (int64_t)(SR * 1.4), -1, 1);
}

/* MORPH: while the loop plays, a change to the chord that is sounding glides into it: notes it lost stop now, notes
 * it gained start now and last as long as the chord (no new attack, no audition) */
static void morph_playing(inst_t *s, int i, const voiced_t *old) {
    if (!s->playing || i < 0 || i != s->play_slot || i >= s->count) return;
    const voiced_t *nv = &s->vc[i];
    int64_t now = s->now, until = 0;
    int was[128] = {0}, is[128] = {0};
    for (int k = 0; k < old->n; k++) was[old->note[k]] = 1;
    if (old->bass >= 0) was[old->bass] = 2;
    for (int k = 0; k < nv->n; k++) is[nv->note[k]] = 1;
    if (nv->bass >= 0) is[nv->bass] = 2;
    for (int n = 0; n < 128; n++) if (was[n] && s->seq_until[n] > now && s->seq_until[n] > until) until = s->seq_until[n];
    if (until <= now) return;   /* nothing is held (short rhythm hits): the next hit plays the new chord */
    for (int n = 0; n < 128; n++) {
        if (was[n] && !is[n] && s->seq_until[n] > now) {
            int ch = was[n] == 2 ? bass_ch(s) : chord_ch(s);
            q_push(&s->aq, now, 0, ch, n, 0, s->gen[ch][n], SRC_SEQ);
            q_push(&s->sq, now, 0, ch, n, 0, s->gen[ch][n], SRC_SEQ);
            s->seq_until[n] = now;
        }
    }
    for (int n = 0; n < 128; n++)
        if (is[n] && !was[n]) sched_note(s, now, until, is[n] == 2 ? bass_ch(s) : chord_ch(s), n, (int)(s->p[P_VELOCITY] * 0.85), SRC_SEQ);
}

/* ------------------------------------------------------------------------------------------------ history */
static void prog_names(const inst_t *s, const slot_t *sl, int n, int key, int scale, char *buf, int len) {
    int k = 0;
    buf[0] = 0;
    for (int i = 0; i < n && k < len - 1; i++) {
        char nm[32];
        chord_name_key(&sl[i].c, key, scale, nm, sizeof nm);
        k += snprintf(buf + k, len - k, "%s%s", i ? " " : "", nm);
    }
    if (!n) snprintf(buf, len, "(EMPTY)");
}

/* record the current progression. ck: changes with the same ck within 2 s merge into one entry (a Q-Link turn) */
static void hist_push(inst_t *s, const char *ck, const char *fmt, ...) CC_PRINTF(3, 4);
static void hist_push(inst_t *s, const char *ck, const char *fmt, ...) {
    int i;
    if (ck && *ck && s->hn > 0 && s->hpos == s->hn - 1 && !strcmp(s->hist[s->hn - 1].ck, ck) && s->now - s->hist_at < (int64_t)SR * 2)
        i = s->hn - 1;
    else {
        if (s->hn == HMAX) { memmove(&s->hist[0], &s->hist[1], (HMAX - 1) * sizeof s->hist[0]); s->hn--; }
        i = s->hn++;
    }
    memcpy(s->hist[i].slot, s->slot, sizeof s->slot);
    s->hist[i].count = s->count;
    s->hist[i].key = s->p[P_KEY];
    s->hist[i].scale = s->p[P_SCALE];
    s->hist[i].len = s->p[P_GEN_LEN];
    s->hist[i].style = s->p[P_STYLE];
    s->hist[i].artist = s->p[P_ARTIST];
    s->hist[i].producer = s->p[P_PRODUCER];
    s->hist[i].spice = s->p[P_SPICE];
    s->hist[i].mix[0] = s->p[P_STYLE_MIX]; s->hist[i].mix[1] = s->p[P_ARTIST_MIX]; s->hist[i].mix[2] = s->p[P_PRODUCER_MIX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s->hist[i].label, sizeof s->hist[i].label, fmt, ap);
    va_end(ap);
    for (char *c = s->hist[i].label; *c; c++) if (*c == ';' || *c == '=' || *c == '|' || *c == '~' || *c == '^') *c = ' ';
    snprintf(s->hist[i].ck, sizeof s->hist[i].ck, "%s", ck ? ck : "");
    s->hpos = i;
    s->hist_at = s->now;
    s->display_rev++;
}

static void settings_of(const inst_t *s, int *o);
static void rebase(inst_t *s) {   /* the progression as it is now becomes the source of later re-fits */
    memcpy(s->src, s->slot, sizeof s->src);
    s->src_n = s->count;
    memcpy(s->vsrc, s->vc, sizeof s->vsrc);
    s->src_ok_v = s->count > 0;
    settings_of(s, s->src_set);
}

static void after_change(inst_t *s, int new_ideas) {
    s->prog_rev++;
    s->want_cruise = 1;
    recount(s);
    revoice(s);
    if (new_ideas) new_spot(s);
    if (!s->in_refit) rebase(s);
    s->display_rev++;
}

static void hist_restore(inst_t *s, int i) {
    if (i < 0 || i >= s->hn) return;
    memcpy(s->slot, s->hist[i].slot, sizeof s->slot);
    s->count = s->hist[i].count;
    s->p[P_KEY] = s->hist[i].key;
    s->p[P_SCALE] = s->hist[i].scale;
    s->p[P_GEN_LEN] = s->hist[i].len;
    s->p[P_STYLE] = clampi(s->hist[i].style, 0, NUM_STYLES - 1);
    s->p[P_ARTIST] = clampi(s->hist[i].artist, 0, NUM_ARTIST_LIST - 1);
    s->p[P_PRODUCER] = clampi(s->hist[i].producer, 0, NUM_PRODUCER_LIST - 1);
    s->p[P_SPICE] = clampi(s->hist[i].spice, 0, 100);
    s->p[P_STYLE_MIX] = clampi(s->hist[i].mix[0], 0, 100);
    s->p[P_ARTIST_MIX] = clampi(s->hist[i].mix[1], 0, 100);
    s->p[P_PRODUCER_MIX] = clampi(s->hist[i].mix[2], 0, 100);
    s->rf_pending = 0;   /* a re-fit still on its way belongs to the state we just left */
    s->hpos = i;
    voiced_t old = s->play_slot >= 0 && s->play_slot < NSLOT ? s->vc[s->play_slot] : (voiced_t){0};
    recount(s);
    s->target = s->count < act_len(s) ? s->count : act_len(s) - 1;
    after_change(s, 1);
    morph_playing(s, s->play_slot, &old);
}

/* ------------------------------------------------------------------------------------------------ progression edits */
/* an idea goes to the target slot: replace a chord there, or add after the last chord */
static void apply_idea(inst_t *s, const chord_t *c) {
    int L = act_len(s), t = s->target;
    char nm[32];
    name_of(s, c, nm, sizeof nm);
    if (t < s->count) {   /* replace: the ideas stay, so the next idea can be tried in the same spot */
        voiced_t old = s->vc[s->play_slot >= 0 && s->play_slot < NSLOT ? s->play_slot : 0];
        s->slot[t].c = *c;
        s->cursor = t;
        after_change(s, 0);
        hist_push(s, NULL, "CHORD %d: %s", t + 1, nm);
        say(s, 2500, "CHORD %d IS NOW %s (UNDO BRINGS THE OLD ONE BACK)", t + 1, nm);
        if (s->playing) morph_playing(s, s->play_slot, &old); else audition_slot(s, t);
        return;
    }
    if (s->count >= L) { say(s, 3500, "THE %d-CHORD LOOP IS FULL: TAP A CHORD AND REROLL TO REPLACE IT", L); return; }
    slot_t *sl = &s->slot[s->count];
    sl->c = *c; sl->filled = 1; sl->var = 0; sl->len = 1; sl->rev = 0;
    s->cursor = s->count;
    recount(s);
    s->target = s->count < L ? s->count : L - 1;   /* full: the next ideas replace the last chord */
    after_change(s, 1);
    hist_push(s, NULL, "ADD %s", nm);
    if (s->count >= L) say(s, 3500, "ADDED %s - THE %d-CHORD LOOP IS COMPLETE", nm, L);
    else say(s, 2500, "ADDED %s - IDEAS FOR CHORD %d", nm, s->count + 1);
}

static void delete_at_cursor(inst_t *s) {
    int i = s->cursor;
    if (i >= s->count) {
        if (!s->count) return;
        i = s->count - 1;
    }
    char nm[32];
    name_of(s, &s->slot[i].c, nm, sizeof nm);
    memmove(&s->slot[i], &s->slot[i + 1], (NSLOT - 1 - i) * sizeof(slot_t));
    s->slot[NSLOT - 1].filled = 0;
    if (s->target > i) s->target--;
    s->cursor = i;
    recount(s);
    after_change(s, 1);
    hist_push(s, NULL, "DELETE %s", nm);
    say(s, 2500, "DELETED %s (UNDO BRINGS IT BACK)", nm);
}

static void clear_all(inst_t *s) {
    if (!s->count) return;
    memset(s->slot, 0, sizeof s->slot);
    s->cursor = 0;
    s->target = 0;
    after_change(s, 1);
    hist_push(s, NULL, "CLEAR");
    say(s, 2500, "CLEARED (UNDO BRINGS IT BACK) - PICK A FIRST CHORD");
}

static void set_progression(inst_t *s, const chord_t *c, int n, const char *label) {
    memset(s->slot, 0, sizeof s->slot);
    for (int i = 0; i < n && i < NSLOT; i++) { s->slot[i].c = c[i]; s->slot[i].filled = 1; s->slot[i].len = 1; }
    s->cursor = 0;
    recount(s);
    s->target = s->count < act_len(s) ? s->count : act_len(s) - 1;
    after_change(s, 1);
    hist_push(s, NULL, "%s", label);
}

static void load_template(inst_t *s, int t) {
    t = clampi(t, 0, NUM_TEMPLATES - 1);
    const template_t *T = &TEMPLATES[t];
    /* keep the key; switch to a major or minor scale if the formula needs the other */
    if (scale_is_minor(s->p[P_SCALE]) != T->minor) s->p[P_SCALE] = T->minor ? 1 : 0;
    if (T->n > 4) s->p[P_GEN_LEN] = 1;
    chord_t c[8];
    for (int i = 0; i < T->n; i++) {
        c[i].root = (s->p[P_KEY] + T->c[i].deg) % 12;
        c[i].q = T->c[i].q;
        c[i].bass = T->c[i].bass >= 0 ? (s->p[P_KEY] + T->c[i].bass) % 12 : -1;
    }
    char lb[40];
    snprintf(lb, sizeof lb, "TEMPLATE %s", T->name);
    set_progression(s, c, T->n, lb);
    say(s, 3000, "LOADED %s (UNDO BRINGS THE OLD ONE BACK)", T->name);
}

/* ------------------------------------------------------------------------------------------------ timeline */
static const double LEN_MULT[3] = {0.5, 1, 2};

/* CHORD LENGTH (MANUAL): a note value from a 1/64 triplet to 16 bars (DUR_TICKS, 96 per beat) */
static double slot_beats(const inst_t *s, int i) {
    return DUR_TICKS[clampi(s->p[P_BARS], 0, NUM_DURS - 1)] / (double)PPQ * LEN_MULT[clampi(s->slot[i].len, 0, 2)];
}

static int build_order(const inst_t *s, long cycle, int *ord) {
    int n = play_count(s), k = 0;
    switch (s->p[P_DIRECTION]) {
    case 1: for (int i = n - 1; i >= 0; i--) ord[k++] = i; break;
    case 2:
        for (int i = 0; i < n; i++) ord[k++] = i;
        for (int i = n - 2; i >= 1; i--) ord[k++] = i;
        break;
    case 3: {
        for (int i = 0; i < n; i++) ord[k++] = i;
        uint32_t r = (uint32_t)(cycle * 2654435761u) ^ 0x5bd1e995u;
        for (int i = k - 1; i > 0; i--) {
            int j = (int)(rng_next(&r) % (uint32_t)(i + 1));
            int t = ord[i]; ord[i] = ord[j]; ord[j] = t;
        }
        break;
    }
    default: for (int i = 0; i < n; i++) ord[k++] = i;
    }
    return k;
}

/* LOOP SYNC: the loop the chords share, in beats (0: MANUAL, every chord has its own CHORD LENGTH). AUTO uses the
 * MPC's loop: from the host's cycle range when it sends one, else measured when the sequence wraps; until then
 * 4 bars are assumed (and shown). */
static double sync_beats(const inst_t *s) {
    static const double B[6] = {4, 8, 16, 32, 64, 128};
    switch (s->p[P_LOOP_SYNC]) {
    case 0: return 0;
    case 1: return s->host_loop > 0 ? s->host_loop : 16;
    default: return B[clampi(s->p[P_LOOP_SYNC] - 2, 0, 5)];
    }
}

/* the order of slots in a cycle and where each starts (bound[0..k], bound[k] = cycle length). With LOOP SYNC the
 * loop is shared out by each chord's LENGTH (half / normal / double as weights) and every boundary sits on the
 * sequencer grid (1/96 beat), so nothing is cut and there are no gaps */
static int timeline(const inst_t *s, long cycle, int *ord, double *bound) {
    int k = build_order(s, cycle, ord);
    double L = sync_beats(s);
    bound[0] = 0;
    if (L > 0) {
        double tot = 0, acc = 0;
        for (int i = 0; i < k; i++) tot += LEN_MULT[clampi(s->slot[ord[i]].len, 0, 2)];
        for (int i = 0; i < k; i++) {
            acc += LEN_MULT[clampi(s->slot[ord[i]].len, 0, 2)];
            bound[i + 1] = i == k - 1 ? L : floor(L * acc / tot * PPQ + 0.5) / PPQ;
            if (bound[i + 1] < bound[i] + 1.0 / PPQ) bound[i + 1] = bound[i] + 1.0 / PPQ;
        }
        if (k && bound[k] < bound[k - 1] + 1.0 / PPQ) bound[k] = bound[k - 1] + 1.0 / PPQ;
    } else
        for (int i = 0; i < k; i++) bound[i + 1] = bound[i] + slot_beats(s, ord[i]);
    return k;
}

static double cycle_beats(const inst_t *s) {
    int ord[2 * NSLOT];
    double b[2 * NSLOT + 1];
    int k = timeline(s, 0, ord, b);
    return b[k];
}

/* which slot sounds at song position t (beats); the loop starts at s->anchor (the MPC loop's start) */
static int slot_at(const inst_t *s, double t, double *start, double *end) {
    if (!play_count(s)) return -1;
    double cyc = cycle_beats(s);
    if (cyc <= 0) return -1;
    t -= s->anchor;
    double c = floor(t / cyc + 1e-9);
    double r = t - c * cyc;
    int ord[2 * NSLOT];
    double b[2 * NSLOT + 1];
    int k = timeline(s, (long)c, ord, b);
    for (int i = 0; i < k; i++) {
        if (r < b[i + 1] - 1e-9 || i == k - 1) {
            *start = s->anchor + c * cyc + b[i];
            *end = s->anchor + c * cyc + b[i + 1];
            return ord[i];
        }
    }
    return -1;
}

static void dur_text(double beats, char *b, int len) {
    double bars = beats / 4.0;
    static const struct { double v; const char *t; } F[] = {{0.125, "1/8 BAR"}, {0.25, "1/4 BAR"}, {0.5, "1/2 BAR"},
                                                           {0.75, "3/4 BAR"}, {1, "1 BAR"}, {1.5, "1 1/2 BARS"}, {2, "2 BARS"},
                                                           {3, "3 BARS"}, {4, "4 BARS"}, {8, "8 BARS"}};
    for (unsigned i = 0; i < sizeof F / sizeof F[0]; i++)
        if (fabs(bars - F[i].v) < 1e-6) { snprintf(b, len, "%s", F[i].t); return; }
    snprintf(b, len, "%.2g BEATS", beats);
}

static void loop_text(const inst_t *s, char *b, int len) {
    double L = sync_beats(s);
    int n = play_count(s);
    if (L <= 0) {
        static const char *const BL[4] = {"1/2 BAR", "1 BAR", "2 BARS", "4 BARS"};
        snprintf(b, len, "MANUAL: %s PER CHORD (CHORD LENGTH)", BL[clampi(s->p[P_BARS], 0, 3)]);
        return;
    }
    const char *src = s->p[P_LOOP_SYNC] != 1 ? "SET" : s->loop_src == 2 ? "MPC" : s->loop_src == 1 ? "MPC, MEASURED"
                                                                          : "ASSUMED TILL MPC LOOPS";
    int bars = (int)lround(L / 4.0);
    if (!n) { snprintf(b, len, "LOOP %d BAR%s (%s)", bars, bars > 1 ? "S" : "", src); return; }
    int even = 1;
    for (int i = 0; i < n; i++) if (s->slot[i].len != 1) even = 0;
    char d[24];
    dur_text(L / n, d, sizeof d);
    if (even) snprintf(b, len, "LOOP %d BAR%s (%s): %d x %s", bars, bars > 1 ? "S" : "", src, n, d);
    else snprintf(b, len, "LOOP %d BAR%s (%s): %d CHORDS, OWN LENGTHS", bars, bars > 1 ? "S" : "", src, n);
}

/* rhythm patterns of 3.x, one bar of 16ths: X accent, x hit, - hold, . rest. The genre grooves (4.0, GROOVES) add
 * B / b (bass note only) and U / u (upper notes only) */
static const char *const PATTERN[11] = {
    NULL, "X---------------", "X-------X-------", "X---x---X---x---", "X-x-X-x-X-x-X-x-", "..x...x...x...x.",
    "X-----x-........", "X--x--x-X--x--x-", "X.xx..x.X..x.x..", "X..x..x...x..x..", "X-------..x-----"};
enum { R_SUSTAIN = 0, R_ARP_UP = 11, R_ARP_DOWN, R_ARP_UPDOWN, R_ARP_RANDOM, R_BROKEN };

typedef void (*emit_fn)(void *ctx, double t, int ch, int note, int vel, double len, int strum_i, int rev);

/* STRUM DIRECTION: the position (0 = first) of chord note i of n in the strum. hit: which hit of the rhythm (Alternate),
 * k: its tick (Guitar: downstrokes, low to high, on the eighths; upstrokes, high to low, between them) */
static int strum_pos(const inst_t *s, int i, int n, long hit, long k) {
    if (n <= 1) return 0;
    switch (s->p[P_STRUM_MODE]) {
    case 1: return n - 1 - i;                                   /* Down: high to low */
    case 2: return (hit & 1) ? n - 1 - i : i;                   /* Alternate */
    case 3: return (((k % (PPQ / 2)) + PPQ / 2) % (PPQ / 2)) ? n - 1 - i : i;   /* Guitar */
    case 4: {                                                   /* Random: a new order for every hit */
        int ord[MAX_VOICE + 1];
        for (int j = 0; j < n && j <= MAX_VOICE; j++) ord[j] = j;
        uint32_t r = (uint32_t)(k * 2654435761u) ^ 0x9e3779b9u;
        for (int j = n - 1; j > 0; j--) { int q = (int)(rng_next(&r) % (uint32_t)(j + 1)); int t = ord[j]; ord[j] = ord[q]; ord[q] = t; }
        for (int j = 0; j < n; j++) if (ord[j] == i) return j;
        return i;
    }
    default: return i;                                          /* Up: low to high */
    }
}

typedef struct { inst_t *s; uint32_t hum; } gen_ctx_t;

static uint32_t hash3(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
    return h;
}

/* the rhythm's pattern and its step in ticks: RHYTHM RATE Auto = the pattern's own grid (16ths, or the groove's) */
static const char *rhythm_pattern(const inst_t *s, int *nsteps, int *step) {
    int rh = clampi(s->p[P_RHYTHM], 0, NUM_OLD_RHYTHMS + NUM_GROOVES - 1);
    const char *pat = NULL;
    int nat = PPQ / 4;
    if (rh >= NUM_OLD_RHYTHMS) {
        const groove_t *g = &GROOVES[rh - NUM_OLD_RHYTHMS];
        pat = g->pat; *nsteps = g->steps; nat = PPQ / g->per_beat;
    } else if (rh > 0 && rh < R_ARP_UP) { pat = PATTERN[rh]; *nsteps = 16; }
    *step = s->p[P_RRATE] > 0 ? DUR_TICKS[clampi(s->p[P_RRATE] - 1, 0, NUM_DURS - 1)] : nat;
    return pat;
}

/* SWING: every second step of a straight grid comes late, (swing - 50) / 50 of a step (75 % = a dotted feel);
 * triplet grids already swing */
static double swing_of(const inst_t *s, long step_idx, int step) {
    if (!(step_idx & 1) || step % 3) return 0;
    return (clampi(s->p[P_SWING], 50, 75) - 50) / 50.0 * step / (double)PPQ;
}

/* emit every note that starts in [t0, t1) of the song (beats); the grid is 96 ticks per beat */
static void generate_span(inst_t *s, double t0, double t1, emit_fn emit, void *ctx) {
    if (!play_count(s) || t1 <= t0) return;
    int rh = clampi(s->p[P_RHYTHM], 0, NUM_OLD_RHYTHMS + NUM_GROOVES - 1);
    double push = s->p[P_PUSH] ? 0.5 : 0.0;
    double gate = s->p[P_GATE] / 100.0;
    int vel = s->p[P_VELOCITY];
    int ch = chord_ch(s), bch = bass_ch(s);
    int nsteps = 16, step = PPQ / 4;
    const char *pat = rhythm_pattern(s, &nsteps, &step);
    int own_bass = pat && strpbrk(pat, "Bb") != NULL;
    int is_arp = rh >= R_ARP_UP && rh < NUM_OLD_RHYTHMS;
    int R = DUR_TICKS[clampi(s->p[P_RATE], 0, NUM_DURS - 1)];
    double maxsw = 0.75 * (pat ? step : is_arp ? R : 0) / (double)PPQ;   /* how late a swung note can come */
    long k0 = (long)floor((t0 - maxsw) * PPQ) - 2, k1 = (long)ceil(t1 * PPQ) + 1;
    /* only the ticks where something can start: the rhythm's steps, chord changes and the arp's grid (a 16-bar step
     * or a long swing look-back would otherwise walk thousands of empty ticks per audio block) */
    int mode = rh == R_SUSTAIN ? 0 : is_arp ? 2 : 1;
    for (long p = k0, k; p <= k1; p = k + 1) {
        double cst, cen;
        if (slot_at(s, p / (double)PPQ + push, &cst, &cen) < 0) break;
        long kc = lround((cst - push) * PPQ), ke = lround((cen - push) * PPQ);
        if (ke <= p) ke = p + 1;
        k = kc >= p ? kc : ke;   /* the next chord change */
        if (mode == 1) { long ks = (p >= 0 ? (p + step - 1) / step : -((-p) / step)) * step; if (ks < k) k = ks; }
        if (mode == 2 && kc < p) { long ka = kc + ((p - kc + R - 1) / R) * R; if (ka < k) k = ka; }
        if (k > k1) break;
        double tn = k / (double)PPQ;
        double st, en;
        if (rh == R_SUSTAIN) {
            if (tn < t0 || tn >= t1) continue;
            int si = slot_at(s, tn + push, &st, &en);
            if (si < 0 || fabs(tn + push - st) >= 1e-6) continue;
            const voiced_t *v = &s->vc[si];
            double left = en - (tn + push);
            int rv = s->slot[si].rev;
            if (v->bass >= 0) emit(ctx, tn, bch, v->bass, vel, left * gate, 0, rv);
            for (int i = 0; i < v->n; i++) emit(ctx, tn, ch, v->note[i], vel, left * gate, strum_pos(s, i, v->n, k, k) + 1, rv);
            continue;
        }
        if (!is_arp) {
            int off = (int)(((k % step) + step) % step);   /* ticks after the last step of the pattern */
            long sidx = (k - off) / step;
            double t = tn + (off ? 0 : swing_of(s, sidx, step));
            if (t < t0 || t >= t1) continue;
            int si = slot_at(s, tn + push, &st, &en);
            if (si < 0) continue;
            int cstart = fabs(tn + push - st) < 1e-6;
            if (off && !cstart) continue;   /* between steps only a chord change sounds (chords shorter than a step) */
            const voiced_t *v = &s->vc[si];
            double left = en - (tn + push);
            int pos = (int)(((sidx % nsteps) + nsteps) % nsteps);
            char c = pat[pos];
            /* bass: once per chord, held (a groove with its own bass hits plays those instead) */
            int rv = s->slot[si].rev;
            if (cstart && v->bass >= 0 && !own_bass) emit(ctx, t, bch, v->bass, vel, left * gate, 0, rv);
            if (off) { if (c == '.') continue; c = c == 'B' || c == 'b' ? c : 'x'; }
            int hit = c != '-' && c != '.';
            if (!hit && cstart && (c == '-' || push > 0)) hit = 1, c = 'x';   /* a chord change inside a held note */
            if (!hit) continue;
            int run = 1;
            while (run < nsteps && pat[(pos + run) % nsteps] == '-') run++;
            double len = (run * step - off) / (double)PPQ;
            if (len > left) len = left;
            int accent = c == 'X' || c == 'B' || c == 'U';
            int vv = accent ? vel : (int)(vel * 0.82);
            if (c == 'B' || c == 'b') {   /* the bass note alone (else the lowest chord note an octave down) */
                int bn = v->bass >= 0 ? v->bass : (v->n ? v->note[0] - 12 : -1);
                if (bn >= 0) emit(ctx, t, bch, bn, vv, len * gate, 0, rv);
                continue;
            }
            int from = 0;
            if (c == 'U' || c == 'u') from = v->n >= 4 ? v->n / 2 : v->n > 1 ? 1 : 0;   /* the upper notes: an offbeat skank */
            for (int i = from; i < v->n; i++)
                emit(ctx, t, ch, v->note[i], vv, len * gate, strum_pos(s, i - from, v->n - from, sidx, k) + 1, rv);
            continue;
        }
        /* arpeggios: ARP RATE from a 1/64 triplet to 16 bars */
        int si = slot_at(s, tn + push, &st, &en);
        if (si < 0) continue;
        const voiced_t *v = &s->vc[si];
        long stick = lround(st * PPQ);
        long rel = k + lround(push * PPQ) - stick;
        if (rel < 0 || rel % R) continue;
        long idx = rel / R;
        double t = tn + swing_of(s, idx, R);
        if (t < t0 || t >= t1) continue;
        double left = en - (tn + push);
        if (idx == 0 && v->bass >= 0) emit(ctx, t, bch, v->bass, vel, left * gate, 0, s->slot[si].rev);
        int notes[MAX_VOICE * 3], nn = 0, oct = clampi(s->p[P_ARP_OCT], 0, 2) + 1;
        for (int o = 0; o < oct; o++)
            for (int i = 0; i < v->n && nn < MAX_VOICE * 3; i++) notes[nn++] = v->note[i] + 12 * o;
        if (!nn) continue;
        int pick;
        switch (rh) {
        case R_ARP_DOWN: pick = nn - 1 - (int)(idx % nn); break;
        case R_ARP_UPDOWN: {
            int per = nn > 1 ? 2 * nn - 2 : 1;
            int x = (int)(idx % per);
            pick = x < nn ? x : per - x;
            break;
        }
        case R_ARP_RANDOM: pick = (int)(hash3((uint32_t)idx, (uint32_t)stick, (uint32_t)si) % (uint32_t)nn); break;
        case R_BROKEN: {
            int lo = 0, hi = nn - 1, mid = nn / 2;
            static const int B[4] = {0, 2, 1, 2};
            int w = B[idx % 4];
            pick = w == 0 ? lo : w == 2 ? hi : mid;
            break;
        }
        default: pick = (int)(idx % nn);
        }
        double len = R / (double)PPQ;
        if (len > left) len = left;
        int vv = (idx % 4 == 0) ? vel : (int)(vel * 0.85);
        emit(ctx, t, ch, notes[pick], vv, len * gate, idx == 0 ? 1 : 2, s->slot[si].rev);
    }
}

/* real-time emit: beats -> frames of this block */
typedef struct { inst_t *s; double ppq0, spb; int64_t frame0; } rt_ctx_t;

static void rt_emit(void *vctx, double t, int ch, int note, int vel, double len, int strum_i, int rev) {
    rt_ctx_t *c = vctx;
    inst_t *s = c->s;
    int64_t f = c->frame0 + (int64_t)llround((t - c->ppq0) * c->spb);
    if (f < c->frame0) f = c->frame0;
    int hum = s->p[P_HUMANIZE];
    uint32_t h = hash3((uint32_t)note, (uint32_t)(int64_t)(t * 96), (uint32_t)ch);
    if (strum_i > 1) f += ms_frames(s->p[P_STRUM]) * (strum_i - 1);
    if (hum) {   /* 4.1: up to a loose human at 100 %: 40 ms late, velocity +-45, lengths +-25 % (gentle at low values) */
        double hx = hum / 100.0;
        f += (int64_t)((h & 1023) / 1023.0 * pow(hx, 1.5) * ms_frames(40));
        vel += (int)((((h >> 10) & 1023) / 1023.0 - 0.5) * 2 * pow(hx, 1.2) * 45);
        len *= 1.0 + ((((h >> 20) & 1023) / 1023.0) - 0.5) * 0.5 * hx;
    }
    int64_t off = f + (int64_t)llround(len * c->spb);
    if (s->p[P_MUTE]) return;
    sched_note_x(s, f, off, ch, note, vel, SRC_SEQ, rev);
    if (rev && strum_i == 1) sched_swell(s, f, off, ch, SRC_SEQ);
}

/* PLAY MODE: 0 OFF (pads and taps only), 1 MPC (follows MPC's transport), 2 LISTEN (our own PLAY/STOP loops the
 * progression at the project tempo; MPC's PLAY also drives it, in time with the sequence). In every mode MPC's STOP
 * ends playback, and nothing keeps running afterwards. */
static void stop_seq(inst_t *s) {
    if (s->playing) { all_notes_off(s, SRC_SEQ); s->playing = 0; }
    s->listening = 0;
    s->play_slot = -1;
    s->display_rev++;
}

static void song_pos(void *inst, double ppq, double bpm, int playing, int frames) {
    inst_t *s = inst;
    if (bpm > 0) s->bpm = bpm;
    int host = playing && ppq >= 0 && bpm > 0;
    if (host) {   /* LOOP SYNC: a jump back while MPC keeps playing is its loop wrapping: measure the loop */
        /* MPC loops on a bar line: the old position ends on one and the new one starts on one. A locate while playing
         * almost never does both, so it is not taken for a loop */
        double eb = s->last_end / 4.0, sb = ppq / 4.0;
        if (s->host_was && ppq < s->hp_last - 0.5 && s->loop_src != 2 && fabs(eb - floor(eb + 0.5)) < 0.02 &&
            fabs(sb - floor(sb + 0.5)) < 0.02) {
            int bars = (int)lround((s->last_end - ppq) / 4.0);
            double ns = floor(ppq / 4.0 + 0.5) * 4.0;
            if (bars >= 1 && bars <= 64 && (s->loop_src != 1 || fabs(s->host_loop - bars * 4.0) > 1e-6 || fabs(s->loop_start - ns) > 1e-6)) {
                s->host_loop = bars * 4.0;
                s->loop_start = ns;
                s->loop_src = 1;
                if (s->p[P_LOOP_SYNC] == 1) say(s, 3500, "MPC LOOP: %d BAR%s - THE CHORD LENGTHS FOLLOW", bars, bars > 1 ? "S" : "");
                s->display_rev++;
            }
        }
        s->hp_last = ppq;
        s->last_end = ppq + frames * bpm / (60.0 * SR);
    }
    int mode = s->p[P_LISTEN];
    if (host && !s->host_was && s->p[P_RUN]) { s->p[P_RUN] = 0; s->display_rev++; }   /* MPC's PLAY takes over */
    if (!host && s->host_was) {   /* MPC's STOP: everything stops */
        if (s->p[P_RUN]) { s->p[P_RUN] = 0; s->display_rev++; }
        stop_seq(s);
    }
    s->host_was = host;
    int follow = host && (mode == 1 || mode == 2);
    if (!host && mode == 2 && s->p[P_RUN] && play_count(s)) {   /* LISTEN on our own clock */
        if (!s->listening) { s->int_ppq = 0; s->listening = 1; s->playing = 0; }
        follow = 1;
        ppq = s->int_ppq;
        s->anchor = 0;
        if (bpm <= 0) bpm = s->bpm > 0 ? s->bpm : 120;
        s->int_ppq += frames * bpm / (60.0 * SR);
    } else if (s->listening) stop_seq(s);
    if (!follow) {
        if (s->playing) stop_seq(s);
        q_run(s, &s->aq, s->now + frames, 1);
        return;
    }
    if (host) s->anchor = s->p[P_LOOP_SYNC] == 1 && s->loop_src ? s->loop_start : 0;
    double blk = frames * bpm / (60.0 * SR);
    double start;
    if (!s->playing) { start = ppq - 0.001; s->playing = 1; s->display_rev++; }
    else if (fabs(ppq - s->sent_to) > 1.0) start = ppq < s->sent_to ? ppq : ppq - 0.002;   /* loop wrap / locate */
    else start = s->sent_to;
    double end = ppq + blk;
    if (end > start) {
        rt_ctx_t c = {s, ppq, 60.0 * SR / bpm, s->now};
        generate_span(s, start, end, rt_emit, &c);
        s->sent_to = end;
    } else if (end > s->sent_to) s->sent_to = end;
    s->last_ppq = ppq;
    double st, en;
    int ps = slot_at(s, ppq + (s->p[P_PUSH] ? 0.5 : 0), &st, &en);
    if (ps != s->play_slot) {
        s->play_slot = ps;
        if (ps >= 0) show_slot(s, ps);
    }
    q_run(s, &s->aq, s->now + frames, 1);
}

/* ------------------------------------------------------------------------------------------------ export */
typedef struct { export_t *j; double spb_ms; inst_t *s; } ex_ctx_t;
static void ex_emit(void *vctx, double t, int ch, int note, int vel, double len, int strum_i, int rev) {
    (void)rev;
    ex_ctx_t *c = vctx;
    if (c->j->nev >= EX_MAX) return;
    double strum_beats = strum_i > 1 ? c->s->p[P_STRUM] * (strum_i - 1) / c->spb_ms : 0;
    c->j->ev[c->j->nev].t = t + strum_beats;
    c->j->ev[c->j->nev].len = len;
    c->j->ev[c->j->nev].ch = (uint8_t)ch;
    c->j->ev[c->j->nev].note = (uint8_t)note;
    c->j->ev[c->j->nev].vel = (uint8_t)clampi(vel, 1, 127);
    c->j->nev++;
}

static int CC_UNUSED dir_ok(const char *d) { return cc_is_dir(d); }

static void start_export(inst_t *s) {
    if (!s->count) { snprintf(s->export_msg, sizeof s->export_msg, "NOTHING TO EXPORT YET"); s->display_rev++; return; }
    if (!s->ex || atomic_load(&s->ex_busy)) { snprintf(s->export_msg, sizeof s->export_msg, "BUSY, TRY AGAIN"); s->display_rev++; return; }
    export_t *x = s->ex;
    memset(x, 0, sizeof *x);
    x->bpm = s->bpm > 20 ? s->bpm : 120;
    /* two passes of the progression (at least 8 bars) */
    double cyc = cycle_beats(s);
    int loops = 1;
    while (cyc * loops < 32 && loops < 8) loops++;
    x->total = cyc * loops;
    ex_ctx_t c = {x, 60000.0 / x->bpm, s};
    int mute = s->p[P_MUTE];
    s->p[P_MUTE] = 0;
    double an = s->anchor;
    s->anchor = 0;
    generate_span(s, -0.001, x->total - 0.001, ex_emit, &c);
    s->anchor = an;
    s->p[P_MUTE] = mute;
    char dir[400];
    snprintf(dir, sizeof dir, "%s/MIDI Exports", cc_docs_root());   /* desktop */
    cc_mkdirs(dir);
    char nm[32];
    snprintf(nm, sizeof nm, "%s%s", pc_name_key(s->p[P_KEY], s->p[P_KEY], s->p[P_SCALE], 0), scale_is_minor(s->p[P_SCALE]) ? "m" : "");
    for (int n = 1; n < 1000; n++) {
        snprintf(x->path, sizeof x->path, "%s/CheatChord %s %03d.mid", dir, nm, n);
        if (!cc_exists(x->path)) break;
    }
    snprintf(s->export_msg, sizeof s->export_msg, "SAVING...");
    s->display_rev++;
    atomic_store(&s->ex_busy, 1);
    job_t j;
    memset(&j, 0, sizeof j);
    post_job(s, JOB_EXPORT, &j);
}

/* ------------------------------------------------------------------------------------------------ MIDI in */
static void midi(void *inst, const uint8_t *msg, int len) {
    inst_t *s = inst;
    if (len < 3) return;
    int st = msg[0] & 0xf0, ch = msg[0] & 15, note = msg[1] & 127, vel = msg[2];
    if (st != 0x90 && st != 0x80) return;
    int on = st == 0x90 && vel > 0;
    /* our own port's notes come back into this track (MPC enables the port for tracks by itself).
     * 1. they keep their channel: anything on our output channels (other than 1, the pads' channel) is ours;
     * 2. for channel 1, or if MPC rewrites the channel: the same note we sent a moment ago is its echo. */
    (void)ch;   /* desktop: a DAW does not route our MIDI output back in, so every note is a key or pad of the user */
    /* pad hits are collected and resolved once per audio block (resolve_hits): a pad layout that sends a chord per pad
     * (Progressions, Chords) then still triggers ONE pad, named by its lowest note */
    if (on) {
        if (s->ngrp < 16) { s->grp_note[s->ngrp] = note; s->grp_vel[s->ngrp] = vel; s->ngrp++; }
        return;
    }
    for (int p = 0; p < 16; p++) if (s->pad[p].active && s->pad_trig[p] == note) release_pad(s, p);
}

static int pad_of_note(const inst_t *s, int note) {
    if (s->padmap_n) {
        for (int i = 0; i < s->padmap_n; i++) if (s->padmap[i] == note) return i;
        return -1;
    }
    int pad = note - s->p[P_PAD_BASE];
    return pad >= 0 && pad < s->p[P_PAD_COUNT] ? pad : -1;
}

static void learn_finish(inst_t *s) {
    if (s->learn_chrom) {   /* only first and last: the chromatic row set in step 2 stands */
        s->learn = 0;
        say(s, 5000, "LEARNED %d PADS (NOTES %d-%d)", s->p[P_PAD_COUNT], s->p[P_PAD_BASE], s->p[P_PAD_BASE] + s->p[P_PAD_COUNT] - 1);
        s->display_rev++;
        return;
    }
    int n = s->learn_nseen;
    for (int i = 1; i < n; i++) for (int j = i; j > 0 && s->learn_seen[j] < s->learn_seen[j - 1]; j--) {
        int t = s->learn_seen[j]; s->learn_seen[j] = s->learn_seen[j - 1]; s->learn_seen[j - 1] = t; }
    memcpy(s->padmap, s->learn_seen, sizeof s->padmap);
    s->padmap_n = n;
    s->p[P_PAD_BASE] = clampi(s->learn_seen[0], 0, 112);
    s->p[P_PAD_COUNT] = clampi(n, 1, 16);
    s->learn = 0;
    if (n <= 8) say(s, 7000, "LEARNED %d PADS (PAD LAYOUT, BY NOTE): ALL PLAY THE PROGRESSION", n);
    else say(s, 7000, "LEARNED %d PADS (PAD LAYOUT, BY NOTE): 1-8 PROGRESSION, 9-%d IDEAS", n, n);
    s->display_rev++;
}

/* the pad hits of this block: LEARN steps, or a pad */
static void resolve_hits(inst_t *s) {
    if (s->learn == 3 && s->now - s->learn_at > ms_frames(4000)) learn_finish(s);   /* 4 s without a tap: done */
    int n = s->ngrp;
    s->ngrp = 0;
    if (!n) return;
    int note = s->grp_note[0], vel = s->grp_vel[0];
    for (int i = 1; i < n; i++) if (s->grp_note[i] < note) note = s->grp_note[i];
    s->last_note = note;
    s->display_rev++;
    if (s->learn) { s->learn_at = s->now; if (n > 1) s->learn_multi = 1; }
    if (s->learn == 1) {   /* LEARN, step 1: the first pad (any bank, any pad layout) */
        s->learn = 2;
        s->learn_first = note;
        s->learn_seen[0] = note; s->learn_nseen = 1;
        s->last_pad = -1;
        say(s, 12000, "LEARN: FIRST PAD = NOTE %d. NOW HIT THE LAST PAD YOU WANT TO USE", note);
        return;
    }
    if (s->learn == 2) {   /* step 2: the last pad */
        if (note == s->learn_first) return;
        int a = s->learn_first < note ? s->learn_first : note, b = s->learn_first < note ? note : s->learn_first;
        s->learn_last = note;
        s->learn_seen[s->learn_nseen++] = note;
        s->learn = 3;
        if (b - a + 1 <= 16 && !s->learn_multi) {   /* most likely the plain chromatic pads: every note between is a pad */
            int c = b - a + 1;
            s->learn_chrom = 1;
            s->padmap_n = 0;
            s->p[P_PAD_BASE] = clampi(a, 0, 112);
            s->p[P_PAD_COUNT] = c;
            s->last_pad = note - a;
            say(s, 8000, "LEARNED %d PADS (NOTES %d-%d). A SCALE / CHORD PAD LAYOUT? TAP THE OTHER PADS TOO", c, a, b);
            return;
        }
        /* the pads are not a row of notes (a scale or chord pad layout): learn each one by its note */
        s->learn_chrom = 0;
        say(s, 15000, "PAD LAYOUT FOUND: TAP EVERY PAD BETWEEN FIRST AND LAST ONCE (2 OF 16) - LEARN AGAIN = DONE");
        return;
    }
    if (s->learn == 3) {   /* step 3: the pads between, any order, until 16 (or LEARN again) */
        int lo = s->learn_first < s->learn_last ? s->learn_first : s->learn_last, hi = lo == s->learn_first ? s->learn_last : s->learn_first;
        if (note < lo || note > hi) return;
        for (int i = 0; i < s->learn_nseen; i++) if (s->learn_seen[i] == note) return;
        s->learn_seen[s->learn_nseen++] = note;
        s->learn_chrom = 0;   /* more pads tapped: they are learned one by one */
        if (s->learn_nseen >= 16) learn_finish(s);
        else say(s, 15000, "LEARN: %d OF 16 PADS - TAP THE REST, OR LEARN AGAIN WHEN DONE", s->learn_nseen);
        return;
    }
    int pad = pad_of_note(s, note);
    s->last_pad = pad;
    if (pad < 0) return;
    int v = s->p[P_VEL_MODE] ? s->p[P_VELOCITY] : (vel ? vel : 100);
    if (s->pad[pad].active) release_pad(s, pad);
    s->pad_trig[pad] = note;
    if (pad < 8) {
        if (pad >= s->count) return;
        s->cursor = pad;   /* a pad only plays and selects: the ideas stay */
        recount(s);
        show_slot(s, pad);
        play_now(s, &s->vc[pad], v, 0, pad);
        s->display_rev++;
    } else {
        int i = pad - 8;
        if (i >= s->pool_n) return;
        voiced_t vv;
        voice_suggestion(s, i, &vv);
        show_chord(s, &s->pool[i], &vv);
        play_now_g(s, &vv, v, 0, pad, 1);   /* the idea pads are one mute group with the idea taps */
        s->sel = i;
        s->sel_at = s->now;
        s->display_rev++;
    }
}

/* ------------------------------------------------------------------------------------------------ texts */
CC_UNUSED static int tint_of(const inst_t *s, const chord_t *c) {
    int cls = harmony_class(s->p[P_KEY], s->p[P_SCALE], c);
    if (cls == CLS_DIATONIC) return c->root == s->p[P_KEY] ? 5 : 4;
    if (cls == CLS_BORROWED) return 3;
    if (cls == CLS_SECONDARY) return 2;
    return 1;
}

static const char *class_word(int cls) {
    switch (cls) {
    case CLS_DIATONIC: return "IN KEY";
    case CLS_BORROWED: return "BORROWED";
    case CLS_SECONDARY: return "LEADS TO NEXT";
    case CLS_SUBV: return "SUBSTITUTE";
    default: return "SPICY";
    }
}

static void progression_text(const inst_t *s, char *buf, int len) {
    int n = 0;
    if (!s->count) { snprintf(buf, len, "START: TAP AN IDEA, OR PRESS GENERATE"); return; }
    for (int i = 0; i < s->count && n < len - 1; i++) {
        char nm[32];
        name_of(s, &s->slot[i].c, nm, sizeof nm);
        n += snprintf(buf + n, len - n, "%s%s", i ? "  " : "", nm);
    }
}

/* ------------------------------------------------------------------------------------------------ library */
/* LIBRARY: what a profile does, read from the very numbers and lists the harmony model uses (artists.c, refs.c) */
static void qname(int q, char *b, int len) {
    const char *y = QUALITIES[q].sym;
    snprintf(b, len, "%s", !y[0] ? "MAJ" : !strcmp(y, "m") ? "MIN" : y);
}
static void deg_roman(int deg, int fam, char *b, int len) {
    static const char *const R[12] = {"I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII"};
    int lower = (fam & (FM_m | FM_h | FM_o)) && !(fam & (FM_M | FM_D));
    int n = 0;
    for (const char *p = R[((deg % 12) + 12) % 12]; *p && n < len - 1; p++) b[n++] = lower && *p >= 'A' && *p <= 'Z' ? (char)(*p - 'A' + 'a') : *p;
    b[n] = 0;
}
static void profile_line(const inst_t *s, int line, char *buf, int len) {
    int prod = s->p[P_LIB_KIND] == 1, pi = prod ? producer_prof(s) : artist_prof(s);
    buf[0] = 0;
    if (!pi) {
        if (line == 1) snprintf(buf, len, "NO %s CHOSEN - PICK ONE ON THE LEFT", prod ? "PRODUCER" : "ARTIST");
        if (line == 2) snprintf(buf, len, "THE STYLE ALONE SHAPES THE CHORDS");
        return;
    }
    const style_t *P = &ARTISTS[pi];
    const ref_t *r = ref_of(pi);
    int st = ARTIST_INFO[pi].style;
    int infl = prod ? s->p[P_PRODUCER_MIX] : s->p[P_ARTIST_MIX];
    switch (line) {
    case 1:
        snprintf(buf, len, "%s - %s - HOME STYLE %s - %d%%", ARTIST_INFO[pi].name, prod ? "PRODUCER" : "ARTIST",
                 st >= 0 ? STYLE_NAMES[st] : "-", infl);
        break;
    case 2: {   /* the chord colours it prefers, strongest first */
        int idx[16], n = 0;
        for (const qpref_t *q = P->qp; q && q->q >= 0 && n < 16; q++) idx[n++] = (int)(q - P->qp);
        for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) if (P->qp[idx[b]].w > P->qp[idx[a]].w) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
        int k = snprintf(buf, len, "CHORDS:");
        for (int a = 0; a < n && a < 6 && k < len; a++) { char q[16]; qname(P->qp[idx[a]].q, q, sizeof q); k += snprintf(buf + k, len - k, " %s", q); }
        break;
    }
    case 3: {   /* its favourite moves */
        int idx[16], n = 0;
        for (const idiom_t *i = P->idioms; i && i->frame >= 0 && n < 16; i++) idx[n++] = (int)(i - P->idioms);
        for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) if (P->idioms[idx[b]].w > P->idioms[idx[a]].w) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
        int k = snprintf(buf, len, "MOVES:");
        for (int a = 0; a < n && a < 5 && k < len; a++) {
            const idiom_t *i = &P->idioms[idx[a]];
            char x[8], y[8];
            deg_roman(i->from, i->ffam, x, sizeof x);
            deg_roman(i->to, i->tfam, y, sizeof y);
            k += snprintf(buf + k, len - k, " %s-%s", x, y);
        }
        break;
    }
    case 4: {   /* where it pushes the model's numbers */
        static const char *const NM[12] = {"RICHER CHORDS", "BORROWED", "SEC. DOMINANTS", "CHROMATIC", "VAMPS", "SLASH BASS",
                                           "SUS", "5TH MOVES", "STEPS", "3RD MOVES", "MEDIANTS", "V-IV"};
        const float v[12] = {P->base_cx, P->borrow, P->secdom, P->chroma, P->vamp, P->slash, P->sus, P->fifths, P->steps,
                             P->thirds, P->mediant, P->retro};
        int k = snprintf(buf, len, "MORE:"), any = 0;
        for (int i = 0, c = 0; i < 12 && k < len && c < 3; i++) if (v[i] >= 0.25f) { k += snprintf(buf + k, len - k, "%s %s", any ? "," : "", NM[i]); any = 1; c++; }
        if (!any && k < len) k += snprintf(buf + k, len - k, " -");
        any = 0;
        if (k < len) k += snprintf(buf + k, len - k, "  LESS:");
        for (int i = 0, c = 0; i < 12 && k < len && c < 2; i++) if (v[i] <= -0.25f) { k += snprintf(buf + k, len - k, "%s %s", any ? "," : "", i ? NM[i] : "SIMPLER"); any = 1; c++; }
        if (!any && k < len) snprintf(buf + k, len - k, " -");
        break;
    }
    case 5:
        if (r) snprintf(buf, len, "REF: %s - %s", r->track, r->artist);
        else snprintf(buf, len, "BASED ON: %s", ARTIST_INFO[pi].why);
        break;
    case 6:
        if (!r) { snprintf(buf, len, "SOUND: %s", ARTIST_INFO[pi].traits); break; }
        if (r->roman[0]) snprintf(buf, len, "%s: %s  =  %s", r->key, r->chords, r->roman);
        else snprintf(buf, len, "%s: %s", r->key, r->chords[0] ? r->chords : "CHORD ORDER NOT DOCUMENTED");
        break;
    default:
        if (!r) { snprintf(buf, len, "NO REFERENCE TRACK STORED - HABITS AS WIDELY DESCRIBED, NO SONG COPIED"); break; }
        snprintf(buf, len, "%s - %s", r->verified >= 2 ? "VERIFIED" : r->n ? "PARTLY VERIFIED" : "KEY ONLY, NO MOVES USED", r->source);
        break;
    }
    for (char *c = buf; *c; c++) if (*c >= 'a' && *c <= 'z' && line == 1) *c = (char)(*c - 'a' + 'A');
    if (len > 63 && strlen(buf) > 63) { buf[61] = '.'; buf[62] = '.'; buf[63] = 0; }   /* the readouts hold 63 */
}

/* ------------------------------------------------------------------------------------------------ params */
static int find_param(const char *key) {
    for (int i = 0; i < NUM_P; i++)
        if (!strcmp(PDEF[i].key, key)) return i;
    return -1;
}

static int tile_index(const char *key, const char *prefix, const char *suffix) {
    size_t pl = strlen(prefix);
    if (strncmp(key, prefix, pl)) return -1;
    char *end;
    long n = strtol(key + pl, &end, 10);
    if (end == key + pl || strcmp(end, suffix)) return -1;
    return n >= 1 && n <= 8 ? (int)n - 1 : -1;
}

static void apply_synth_params(inst_t *s) {
    s->syn.sound = clampi(s->p[P_SOUND], 0, NUM_SOUNDS - 1);
    s->syn.level = s->p[P_LEVEL] / 100.0f;
    s->syn.tone = s->p[P_TONE] / 100.0f;
    s->syn.release = s->p[P_RELEASE] / 100.0f;
    s->syn.space = s->p[P_SPACE] / 100.0f;
}

static void load_state(inst_t *s, const char *st);

static const char *mode_help(int m) {
    switch (m) {
    case 0: return "OFF: NO AUTOMATIC PLAYBACK. PADS AND TAPS PLAY THE CHORDS";
    case 1: return "MPC: THE LOOP FOLLOWS MPC PLAY AND STOP, IN TIME WITH THE SEQUENCE";
    default: return "LISTEN: PLAY / STOP HERE LOOP IT AT PROJECT TEMPO. MPC PLAY / STOP WORK TOO";
    }
}
static const char *len_help(int l) {
    return l ? "8 CHORDS: THE LOOP PLAYS SLOTS 1-8 AND GENERATE WRITES 8 CHORDS"
             : "4 CHORDS: THE LOOP PLAYS SLOTS 1-4, GENERATE WRITES 4. CHORDS 5-8 ARE KEPT (GREY)";
}

static void retarget(inst_t *s, int t) {   /* ideas for another spot: an explicit action (REROLL, the + tile) */
    int L = act_len(s);
    t = clampi(t, 0, s->count < L ? s->count : L - 1);
    if (t != s->target) { s->target = t; new_spot(s); }
    else { s->sel = -1; request_pool(s); }
}

/* ------------------------------------------------------------------------------------------------ browser -> progression */

static const char *scope_help(const inst_t *s) {
    static char b[96];
    if (s->p[P_SCOPE]) return "EDIT ALL: KEY, SCALE, STYLE, ARTIST, PRODUCER RE-FIT EVERY CHORD";
    if (s->cursor < s->count) {
        snprintf(b, sizeof b, "EDIT CHORD: KEY, SCALE, STYLE, ARTIST, PRODUCER CHANGE ONLY CHORD %d", s->cursor + 1);
        return b;
    }
    return "EDIT CHORD: TAP A CHORD FIRST - BROWSER CHANGES THEN ONLY CHANGE THAT CHORD";
}

/* KEY / SCALE / STYLE / ARTIST / PRODUCER / SPICE / an INFLUENCE changed (o: the settings before): re-fit the
 * selected chord (EDIT CHORD) or every chord (EDIT ALL) to the new setting. The work runs on the worker thread (never
 * in the audio path); take_result() applies it. One history entry per change; turning a Q-Link through several
 * values merges into one entry and always re-fits from the state before the turn, so nothing drifts. */
enum { BR_KEY, BR_SCALE, BR_STYLE, BR_ARTIST, BR_PRODUCER, BR_SPICE, BR_SMIX, BR_AMIX, BR_PMIX };
static const char *const BR_NAMES[] = {"KEY", "SCALE", "STYLE", "ARTIST", "PRODUCER", "SPICE", "STYLE MIX", "ARTIST MIX",
                                       "PRODUCER MIX"};
static void br_value(const inst_t *s, int which, char *val, int len) {
    switch (which) {
    case BR_KEY: snprintf(val, len, "%s", KEY_LABELS[s->p[P_KEY]]); break;
    case BR_SCALE: snprintf(val, len, "%s", SCALE_LABELS[s->p[P_SCALE]]); break;
    case BR_STYLE: snprintf(val, len, "%s", STYLE_NAMES[s->p[P_STYLE]]); break;
    case BR_ARTIST: snprintf(val, len, "%s", ARTIST_INFO[artist_prof(s)].name); break;
    case BR_PRODUCER: snprintf(val, len, "%s", ARTIST_INFO[producer_prof(s)].name); break;
    case BR_SPICE: snprintf(val, len, "%d%%", s->p[P_SPICE]); break;
    case BR_SMIX: snprintf(val, len, "%d%%", s->p[P_STYLE_MIX]); break;
    case BR_AMIX: snprintf(val, len, "%d%%", s->p[P_ARTIST_MIX]); break;
    default: snprintf(val, len, "%d%%", s->p[P_PRODUCER_MIX]); break;
    }
}

static void post_refit(inst_t *s, job_t *j) {
    s->rf_job = *j;
    s->rf_pending = 1;
    s->rf_seq = post_job(s, JOB_REFIT, j);
}

static void browser_change(inst_t *s, int which, const int *o) {
    int whole = s->p[P_SCOPE] == 1, c = s->cursor;
    char val[32];
    br_value(s, which, val, sizeof val);
    if (!s->count || (!whole && c >= s->count)) {   /* nothing to re-fit: the setting is for the next ideas */
        new_spot(s);
        s->want_cruise = 1;
        if (!s->count) say(s, 3000, "%s %s - FOR THE NEXT IDEAS", BR_NAMES[which], val);
        else say(s, 4000, "%s %s - NO CHORD SELECTED: TAP A CHORD, OR SWITCH TO EDIT ALL", BR_NAMES[which], val);
        return;
    }
    job_t j;
    memset(&j, 0, sizeof j);
    job_settings(s, &j);
    snprintf(j.ck, sizeof j.ck, "br%d%c%d", which, whole ? 'w' : 'c', whole ? 0 : c);
    memcpy(j.cur, s->slot, sizeof j.cur);
    if (s->src_n == s->count) {   /* from the source chords and the settings they were made with */
        memcpy(j.base, s->src, sizeof j.base);
        memcpy(j.bset, s->src_set, sizeof j.bset);
    } else {
        memcpy(j.base, s->slot, sizeof j.base);
        memcpy(j.bset, o, sizeof j.bset);
    }
    j.count = s->count;
    j.whole = whole;
    j.cursor = c;
    j.which = which;
    j.L = play_count(s);
    j.prog_rev = s->prog_rev;
    char lb[96];
    if (whole) snprintf(lb, sizeof lb, "%s %s (ALL)", BR_NAMES[which], val);
    else snprintf(lb, sizeof lb, "%s %s (CHORD %d)", BR_NAMES[which], val, c + 1);
    snprintf(j.label, sizeof j.label, "%.39s", lb);
    post_refit(s, &j);
}

/* the re-fit came back: apply it (if nothing else changed the progression meanwhile) */
static void apply_refit(inst_t *s, const result_t *r) {
    const job_t *j = &r->job;
    if (r->seq != s->rf_seq || !s->rf_pending) return;   /* outdated: a newer one is on its way, or UNDO came first */
    if (j->prog_rev != s->prog_rev || j->count != s->count) {
        /* the progression was changed meanwhile (an idea, a template, GENERATE ...): that change already used the new
         * setting, so the re-fit is dropped rather than applied to chords it was not computed for */
        s->rf_pending = 0;
        char val[32];
        br_value(s, j->which, val, sizeof val);
        say(s, 3500, "%s %s: KEPT FOR NEW IDEAS (THE PROGRESSION CHANGED MEANWHILE)", BR_NAMES[j->which], val);
        new_spot(s);
        return;
    }
    s->rf_pending = 0;
    int n = s->count, c = j->cursor, moved = 0;
    chord_t before = s->slot[c < n ? c : 0].c;
    voiced_t oldp = s->play_slot >= 0 && s->play_slot < NSLOT ? s->vc[s->play_slot] : (voiced_t){0};
    for (int k = 0; k < n; k++) {
        if (!chord_equal(&s->slot[k].c, &r->slots[k].c)) moved++;
        s->slot[k].c = r->slots[k].c;
    }
    s->in_refit = 1;
    after_change(s, 1);
    s->in_refit = 0;
    morph_playing(s, s->play_slot, &oldp);   /* 4.1: silent - it plays: the sounding chord morphs; it stops: only the keys show it */
    hist_push(s, j->ck, "%s", j->label);
    char val[32], nm[32], nb[32];
    br_value(s, j->which, val, sizeof val);
    if (j->whole) {
        if (moved) say(s, 3500, "%s %s: %d OF %d CHORDS RE-FIT (UNDO BRINGS THEM BACK)", BR_NAMES[j->which], val, moved, n);
        else say(s, 3500, "%s %s: ALL %d CHORDS ALREADY FIT", BR_NAMES[j->which], val, n);
        if (s->playing && s->play_slot >= 0) show_slot(s, s->play_slot);
        else if (s->cursor < n) show_slot(s, s->cursor);
    } else if (c < n) {
        name_of(s, &before, nb, sizeof nb);
        name_of(s, &s->slot[c].c, nm, sizeof nm);
        if (chord_equal(&before, &s->slot[c].c)) say(s, 3500, "%s %s: CHORD %d (%s) ALREADY FITS", BR_NAMES[j->which], val, c + 1, nm);
        else say(s, 3500, "%s %s: CHORD %d %s -> %s, THE OTHERS STAY (UNDO BRINGS IT BACK)", BR_NAMES[j->which], val, c + 1, nb, nm);
        show_slot(s, s->playing && s->play_slot >= 0 ? s->play_slot : c);   /* shown on the keys, not played */
    }
}

/* ------------------------------------------------------------------------------------------------ cruise */
/* where CRUISE puts the next chord: after the selected chord (or the last one). In the middle of the progression the
 * new chord is inserted (nothing is overwritten); returns 0 when the loop is full */
static int cruise_spot(const inst_t *s, int *target, int *insert) {
    int L = act_len(s), n = s->count;
    int c = s->cursor < n ? s->cursor : n - 1;
    *target = n ? c + 1 : 0;
    *insert = *target < n;
    return n < L;
}

static void cruise_job(inst_t *s, int apply) {
    job_t j;
    memset(&j, 0, sizeof j);
    job_settings(s, &j);
    int t, ins, ok = cruise_spot(s, &t, &ins);
    j.target = t; j.insert = ins; j.apply = apply && ok;
    if (t > 0) { j.prev = s->slot[t - 1].c; j.has_prev = 1; }
    if (t > 1) { j.prev2 = s->slot[t - 2].c; j.has_prev2 = 1; }
    if (ins) { j.next = s->slot[t].c; j.has_next = 1; }
    int L = act_len(s);
    if (t > 0 && (ins ? s->count : t) == L - 1) { j.loop_to = s->slot[0].c; j.has_loop = 1; }
    for (int i = 0; i < s->count; i++) j.used[i] = s->slot[i].c;
    j.nused = s->count;
    j.prog_rev = s->prog_rev;
    j.cursor = s->cursor;
    s->cr_seq = post_job(s, JOB_CRUISE, &j);
    s->want_cruise = 0;
    if (j.apply) s->cr_apply = 1;
}

static void cruise_insert(inst_t *s, const chord_t *c, int t, int ins) {
    if (ins) memmove(&s->slot[t + 1], &s->slot[t], (NSLOT - 1 - t) * sizeof(slot_t));
    slot_t *sl = &s->slot[t];
    sl->c = *c; sl->filled = 1; sl->var = 0; sl->len = 1; sl->rev = 0;
    s->cursor = t;
    recount(s);
    int L = act_len(s);
    s->target = s->count < L ? s->count : L - 1;
    after_change(s, 1);
    char nm[32];
    name_of(s, c, nm, sizeof nm);
    hist_push(s, NULL, "CRUISE %s", nm);
    say(s, 3000, "CRUISE: %s %s CHORD %d (UNDO TAKES IT BACK)", nm, ins ? "INSERTED AS" : "IS", t + 1);
    if (!s->playing) audition_slot(s, t); else show_slot(s, t);   /* while it plays, the loop is the preview */
}

static void apply_cruise(inst_t *s, const result_t *r) {
    const job_t *j = &r->job;
    if (r->seq != s->cr_seq) return;
    if (j->prog_rev != s->prog_rev || j->cursor != s->cursor) {   /* stale: ask again (keeps an apply request) */
        cruise_job(s, j->apply);
        return;
    }
    s->cr_next = r->next;
    s->cr_ok = 1;
    s->cr_target = j->target;
    s->cr_insert = j->insert;
    s->display_rev++;
    if (j->apply) { s->cr_apply = 0; cruise_insert(s, &r->next, j->target, j->insert); }
}

static void edit_slot(inst_t *s, int i, int v) {
    int c = s->cursor;
    slot_t *sl = &s->slot[c];
    voiced_t oldp = s->vc[s->play_slot >= 0 && s->play_slot < NSLOT ? s->play_slot : 0];
    if (!sl->filled) {   /* building a chord by hand in the next empty slot */
        if (c > s->count) return;
        sl->filled = 1; sl->c.root = s->p[P_KEY]; sl->c.q = scale_is_minor(s->p[P_SCALE]) ? Q_MIN : Q_MAJ;
        sl->c.bass = -1; sl->var = 0; sl->len = 1; sl->rev = 0;
    }
    /* EDIT ALL: ROOT moves every chord by the same step (the progression is transposed, its shape stays), TYPE gives
     * every chord the new type, VARIATION and LENGTH set every chord. BASS stays per chord. */
    int all = s->p[P_SCOPE] == 1 && i != P_SLOT_BASS;
    int droot = i == P_SLOT_ROOT ? ((v - sl->c.root) % 12 + 12) % 12 : 0;
    for (int k = 0; k < NSLOT; k++) {
        slot_t *x = &s->slot[k];
        if (!x->filled || (k != c && !all)) continue;
        if (i == P_SLOT_ROOT) {
            x->c.root = (x->c.root + droot) % 12;
            if (x->c.bass >= 0) x->c.bass = (x->c.bass + droot) % 12;
        }
        if (i == P_SLOT_TYPE) x->c.q = clampi(v, 0, NUM_QUALITIES - 1);
        if (i == P_SLOT_REV) x->rev = v ? 1 : 0;
        if (i == P_SLOT_VAR) x->var = v;
        if (i == P_SLOT_LEN) x->len = v;
        if (x->c.bass == x->c.root) x->c.bass = -1;
    }
    if (i == P_SLOT_BASS) sl->c.bass = v ? v - 1 : -1;
    if (sl->c.bass == sl->c.root) sl->c.bass = -1;
    after_change(s, 0);
    char nm[32], ck[16];
    name_of(s, &sl->c, nm, sizeof nm);
    if (all) {
        static const char *const WHAT[] = {"ROOT", "TYPE", "BASS", "VARIATION", "LENGTH", "REVERSE"};
        int w = i == P_SLOT_ROOT ? 0 : i == P_SLOT_TYPE ? 1 : i == P_SLOT_VAR ? 3 : i == P_SLOT_REV ? 5 : 4;
        char pr[120];
        prog_names(s, s->slot, play_count(s), s->p[P_KEY], s->p[P_SCALE], pr, sizeof pr);
        snprintf(ck, sizeof ck, "editall%d", i);
        hist_push(s, ck, "%s FOR ALL CHORDS", WHAT[w]);
        if (i == P_SLOT_ROOT || i == P_SLOT_TYPE) say(s, 3500, "EDIT ALL %s: %s", WHAT[w], pr);
        else say(s, 2500, "%s FOR ALL CHORDS (EDIT ALL)", WHAT[w]);
    } else {
        snprintf(ck, sizeof ck, "edit%d", c);
        hist_push(s, ck, "EDIT CHORD %d: %s", c + 1, nm);
    }
    /* stopped: hear the chord; playing: the sounding chord morphs into the new one (no extra attack) */
    if (s->playing) { if (i != P_SLOT_LEN && i != P_SLOT_REV) morph_playing(s, s->play_slot, &oldp); }
    else if (i != P_SLOT_LEN) audition_slot(s, c);
}

/* the touch browser: big tiles with the options of the chosen category (KEY, SCALE, STYLE, ARTIST, PRODUCER) */
static int tile14(const char *key, const char *prefix, const char *suffix) {
    size_t pl = strlen(prefix);
    if (strncmp(key, prefix, pl)) return -1;
    char *end;
    long n = strtol(key + pl, &end, 10);
    if (end == key + pl || strcmp(end, suffix)) return -1;
    return n >= 1 && n <= 14 ? (int)n - 1 : -1;
}
static const int BROWSE_P[5] = {P_KEY, P_SCALE, P_STYLE, P_ARTIST, P_PRODUCER};
static int browse_count(int cat) {
    static const int N[5] = {12, NUM_SCALES, NUM_STYLES, NUM_ARTIST_LIST, NUM_PRODUCER_LIST};
    return N[clampi(cat, 0, 4)];
}
static const char *browse_name(int cat, int i) {
    switch (clampi(cat, 0, 4)) {
    case 0: return KEY_LABELS[i];
    case 1: return SCALE_LABELS[i];
    case 2: return STYLE_NAMES[i];
    case 3: return i ? ARTIST_INFO[ARTIST_LIST[i]].name : "No Artist";
    default: return i ? ARTIST_INFO[PRODUCER_LIST[i]].name : "No Producer";
    }
}

/* ------------------------------------------------------------------------------------------------ presets */
static int state_text(const inst_t *s, char *buf, int len, int hist);
/* PRESETS: 0..NUM_FACTORY_PRESETS-1 factory (tools/cc_data.py), then the user slots (files in MPC Documents/CheatChord/
 * Presets). MPC's PRESET menu lists the same numbers (vst.json "programs" on the "preset" parameter). */
static void load_state_ex(inst_t *s, const char *st, int preset);
static void user_name(const inst_t *s, int u, char *b, int len) {
    if (s->unames[u][0]) snprintf(b, len, "U%d %s", u + 1, s->unames[u]);
    else snprintf(b, len, "USER %d (EMPTY)", u + 1);
}
static void preset_name(const inst_t *s, int i, char *b, int len) {
    if (i < NUM_FACTORY_PRESETS) snprintf(b, len, "%s", FPRESETS[clampi(i, 0, NUM_FACTORY_PRESETS - 1)].name);
    else user_name(s, clampi(i - NUM_FACTORY_PRESETS, 0, NUM_USER_PRESETS - 1), b, len);
}

static void post_file(inst_t *s, int op, int slot) {
    job_t j;
    memset(&j, 0, sizeof j);
    j.fop = op;
    j.fslot = slot;
    s->fio_busy = 1;
    post_job(s, JOB_FILE, &j);
}

static void load_factory(inst_t *s, int i) {
    const fpreset_t *F = &FPRESETS[clampi(i, 0, NUM_FACTORY_PRESETS - 1)];
    all_notes_off(s, SRC_SEQ | SRC_LIVE);
    s->p[P_KEY] = F->key; s->p[P_SCALE] = F->scale; s->p[P_STYLE] = clampi(F->style, 0, NUM_STYLES - 1);
    s->p[P_ARTIST] = clampi(F->artist, 0, NUM_ARTIST_LIST - 1); s->p[P_PRODUCER] = clampi(F->producer, 0, NUM_PRODUCER_LIST - 1);
    s->p[P_SPICE] = F->spice; s->p[P_VOICING] = F->voicing; s->p[P_RHYTHM] = F->rhythm; s->p[P_BARS] = F->bars;
    s->p[P_SWING] = F->swing; s->p[P_SOUND] = F->sound; s->p[P_RRATE] = 0;
    s->p[P_STYLE_MIX] = s->p[P_ARTIST_MIX] = s->p[P_PRODUCER_MIX] = 50;
    s->p[P_GEN_LEN] = F->n > 4;
    apply_synth_params(s);
    chord_t c[8];
    for (int k = 0; k < F->n && k < 8; k++) { c[k].root = (F->key + F->c[k].deg) % 12; c[k].q = F->c[k].q; c[k].bass = -1; }
    s->p[P_PRESET] = i;
    s->preset_loaded = 1;
    char lb[40];
    snprintf(lb, sizeof lb, "PRESET %s", F->name);
    set_progression(s, c, F->n, lb);
    say(s, 4000, "PRESET %s: %s", F->name, F->desc);
}

static void choose_preset(inst_t *s, int i) {
    if (i < NUM_FACTORY_PRESETS) { load_factory(s, i); return; }
    int u = i - NUM_FACTORY_PRESETS;
    if (s->fio_busy) { say(s, 2000, "BUSY - TRY AGAIN"); return; }
    s->p[P_PRESET] = i;
    post_file(s, FILE_LOAD, u);
    say(s, 2000, "LOADING USER %d...", u + 1);
}

static void save_user(inst_t *s) {
    int u = clampi(s->p[P_SAVE_SLOT] - 1, 0, NUM_USER_PRESETS - 1);
    if (s->fio_busy) { say(s, 2000, "BUSY - TRY AGAIN"); return; }
    if (!s->count) { say(s, 3000, "NOTHING TO SAVE YET: MAKE A PROGRESSION FIRST"); return; }
    char nm[48];
    snprintf(nm, sizeof nm, "%s %s %s", KEY_LABELS[s->p[P_KEY]], STYLE_NAMES[s->p[P_STYLE]],
             s->p[P_ARTIST] ? ARTIST_INFO[artist_prof(s)].name : s->p[P_PRODUCER] ? ARTIST_INFO[producer_prof(s)].name : "");
    for (char *c = nm; *c; c++) if (*c == ';' || *c == '=' || *c == '\n') *c = ' ';
    int n = snprintf(s->fio->text, FIO_MAX, "name=%s;", nm);
    state_text(s, s->fio->text + n, FIO_MAX - n, 0);
    post_file(s, FILE_SAVE, u);
    s->p[P_PRESET] = NUM_FACTORY_PRESETS + u;
    s->preset_loaded = 1;
    say(s, 2000, "SAVING USER %d...", u + 1);
}

/* "pre_3" -> 2 (n tiles) */
static int tile_n(const char *key, const char *prefix, const char *suffix, int n) {
    size_t pl = strlen(prefix);
    if (strncmp(key, prefix, pl)) return -1;
    char *end;
    long v = strtol(key + pl, &end, 10);
    if (end == key + pl || strcmp(end, suffix)) return -1;
    return v >= 1 && v <= n ? (int)v - 1 : -1;
}

/* the worker read or wrote a preset file */
static void apply_file(inst_t *s, const result_t *r) {
    memcpy(s->unames, s->fio->names, sizeof s->unames);
    s->fio_busy = 0;
    s->display_rev++;
    if (r->job.fop == FILE_LOAD) {
        if (!s->fio->ok) { say(s, 3500, "%s", s->fio->msg); return; }
        all_notes_off(s, SRC_SEQ | SRC_LIVE);
        load_state_ex(s, s->fio->text, 1);
        s->p[P_PRESET] = NUM_FACTORY_PRESETS + r->job.fslot;
        s->preset_loaded = 1;
        hist_push(s, NULL, "PRESET USER %d", r->job.fslot + 1);
        say(s, 3500, "USER %d LOADED: %s (UNDO BRINGS THE OLD STATE BACK)", r->job.fslot + 1, s->unames[r->job.fslot]);
    } else if (r->job.fop == FILE_SAVE) say(s, 4000, "%s", s->fio->msg);
}

static void set_param(void *inst, const char *key, const char *val) {
    inst_t *s = inst;
    int i = find_param(key), t;
    double fv = atof(val);
    if (!strcmp(key, "state")) { load_state(s, val); return; }
    if (!strcmp(key, "host_cycle")) {   /* the host's loop range in beats, "start end" (wrapper, when MPC sends it) */
        double a = 0, b = 0;
        if (sscanf(val, "%lf %lf", &a, &b) == 2 && b - a >= 1.0 && b - a <= 256.0) {
            if (s->loop_src != 2 || fabs(s->host_loop - (b - a)) > 1e-6 || fabs(s->loop_start - a) > 1e-6) s->display_rev++;
            s->host_loop = b - a; s->loop_start = a; s->loop_src = 2;
        }
        return;
    }
    if (!strcmp(key, "mains")) {   /* MPC runs this on STOP (and on insert): end LISTEN, nothing keeps playing */
        if (atoi(val) == 0 && (s->p[P_RUN] || s->listening)) { s->p[P_RUN] = 0; stop_seq(s); }
        return;
    }
    if ((t = tile_index(key, "sug_", "")) >= 0) {
        if (s->now < s->quiet_until) return;
        if (s->now - s->tap_at[0][t] < ms_frames(TAP_DEBOUNCE_MS)) return;
        s->tap_at[0][t] = s->now;
        if (t >= s->pool_n) return;
        if (s->sel == t && s->now - s->sel_at > ms_frames(TAP_DEBOUNCE_MS)) {   /* second tap: keep it */
            chord_t c = s->pool[t];
            apply_idea(s, &c);
            return;
        }
        s->sel = t;
        s->sel_at = s->now;
        audition_sug(s, t);
        s->display_rev++;
        return;
    }
    if ((t = tile_index(key, "prog_", "")) >= 0) {   /* PREVIEW ONLY: the progression and the ideas stay */
        if (s->now < s->quiet_until) return;
        if (s->now - s->tap_at[1][t] < ms_frames(TAP_DEBOUNCE_MS)) return;
        s->tap_at[1][t] = s->now;
        if (t < s->count) {   /* select it: hear it, and the IDEAS are now ideas to REPLACE it (tap an idea twice) */
            s->cursor = t;
            recount(s);
            audition_slot(s, t);
            s->want_cruise = 1;
            if (s->target != t) {   /* the ideas stay as they are (REROLL deals ideas made for this spot) */
                s->target = t;
                s->sel = -1;
                say(s, 3000, "CHORD %d SELECTED: AN IDEA TAPPED TWICE REPLACES IT", t + 1);
            }
            s->display_rev++;
        } else if (t == s->count && t < act_len(s)) {   /* the + tile: ideas for the next chord */
            s->cursor = t;
            recount(s);
            retarget(s, t);
        }
        return;
    }
    if ((t = tile14(key, "bri_", "")) >= 0 || (t = tile14(key, "lib_", "")) >= 0) {   /* browser / library tiles */
        if (atof(val) <= 0.5 || s->now < s->quiet_until) return;
        if (s->now - s->tap_br[t] < ms_frames(TAP_DEBOUNCE_MS)) return;
        s->tap_br[t] = s->now;
        int cat = key[0] == 'b' ? s->p[P_BROWSE] : (s->p[P_LIB_KIND] ? 4 : 3);
        if (t >= browse_count(cat)) return;
        char v[16];
        snprintf(v, sizeof v, "%d", t);
        set_param(s, PDEF[BROWSE_P[clampi(cat, 0, 4)]].key, v);   /* the same path as the control itself */
        s->display_rev++;
        return;
    }
    if ((t = tile_n(key, "pre_", "", NUM_FACTORY_PRESETS)) >= 0 || (t = tile_n(key, "usr_", "", NUM_USER_PRESETS)) >= 0) {
        if (atof(val) <= 0.5 || s->now < s->quiet_until) return;   /* the PRESETS panel: a tap loads */
        int idx = key[0] == 'p' ? t : NUM_FACTORY_PRESETS + t;
        if (s->now - s->tap_pre[idx] < ms_frames(TAP_DEBOUNCE_MS)) return;
        s->tap_pre[idx] = s->now;
        choose_preset(s, idx);
        s->display_rev++;
        return;
    }
    if ((t = tile_index(key, "hist_", "")) >= 0) {   /* newest first */
        if (s->now < s->quiet_until) return;
        if (s->now - s->tap_at[2][t] < ms_frames(TAP_DEBOUNCE_MS)) return;
        s->tap_at[2][t] = s->now;
        int e = s->hn - 1 - t;   /* any entry, also the current one again: it becomes the progression */
        if (e >= 0) { hist_restore(s, e); say(s, 3000, "ACTIVE: #%d %s (UNDO / REDO STILL WORK)", e + 1, s->hist[e].label); }
        return;
    }
    if (i < 0) return;
    if (PDEF[i].kind == 2) return;
    if (PDEF[i].kind == 1) {
        if (fv <= 0.5) return;
        if (s->now < s->quiet_until) return;
        switch (i) {
        case P_ADD:
            if (s->sel >= 0 && s->sel < s->pool_n) { chord_t c = s->pool[s->sel]; apply_idea(s, &c); }
            else if (s->pool_n) { chord_t c = s->pool[0]; apply_idea(s, &c); }
            break;
        case P_REROLL:   /* the only way to new ideas for a spot: for the selected chord if one is selected */
            if (s->cursor != s->target && s->cursor <= s->count) {
                retarget(s, s->cursor);
                if (s->target < s->count) say(s, 3000, "IDEAS TO REPLACE CHORD %d", s->target + 1);
                else say(s, 2000, "IDEAS FOR CHORD %d", s->target + 1);
            } else {
                retarget(s, s->target);
                say(s, 1500, "NEW IDEAS");
            }
            break;
        case P_GENERATE: {
            job_t j;
            memset(&j, 0, sizeof j);
            job_settings(s, &j);
            j.len = act_len(s);
            j.rng = rng_next(&s->rng) ^ (uint32_t)time(NULL);
            for (int e = s->hn - 1; e >= 0 && j.ngav < GAVOID; e--)   /* the progressions already in the history */
                if (s->hist[e].count) {
                    for (int q = 0; q < s->hist[e].count && q < NSLOT; q++) j.gav[j.ngav][q] = s->hist[e].slot[q].c;
                    j.gavn[j.ngav++] = s->hist[e].count;
                }
            post_job(s, JOB_GENERATE, &j);
            s->busy_gen = 1;
            say(s, 1500, "GENERATING...");
            break;
        }
        case P_UNDO:
            if (s->hpos > 0) { hist_restore(s, s->hpos - 1); say(s, 3000, "UNDO: BACK TO #%d %s", s->hpos + 1, s->hist[s->hpos].label); }
            else say(s, 1500, "NOTHING TO UNDO");
            break;
        case P_REDO:
            if (s->hpos < s->hn - 1) { hist_restore(s, s->hpos + 1); say(s, 3000, "REDO: #%d %s", s->hpos + 1, s->hist[s->hpos].label); }
            else say(s, 1500, "NOTHING TO REDO");
            break;
        case P_STOP:
            s->p[P_RUN] = 0;
            stop_seq(s);
            say(s, 1500, "STOPPED");
            break;
        case P_DELETE: delete_at_cursor(s); break;
        case P_CLEAR: clear_all(s); break;
        case P_PANIC: all_notes_off(s, SRC_SEQ | SRC_LIVE); synth_all_off(&s->syn, 1); say(s, 1500, "ALL NOTES OFF"); break;
        case P_EXPORT: start_export(s); break;
        case P_LOAD: load_template(s, s->p[P_TEMPLATE]); break;
        case P_SAVE_PRESET: save_user(s); break;
        case P_CRUISE: {   /* the next chord after the selected one, into the next slot (never over a chord) */
            int t, ins;
            if (!cruise_spot(s, &t, &ins)) {
                say(s, 4000, act_len(s) < 8 ? "THE 4-CHORD LOOP IS FULL: SWITCH TO 8 CHORDS OR DELETE ONE" : "ALL 8 CHORDS ARE SET: DELETE ONE FIRST");
                break;
            }
            if (s->cr_ok && !s->want_cruise && s->cr_target == t && s->cr_insert == ins && !s->rf_pending) {
                chord_t c = s->cr_next;   /* the shown suggestion is current: take it */
                cruise_insert(s, &c, t, ins);
            } else cruise_job(s, 1);
            break;
        }
        case P_KEYS_OPEN:   /* the small keyboard's BIG KEYS: open the big one, again: back */
            if (s->p[P_PANEL] == 3) s->p[P_PANEL] = s->prev_panel != 3 ? s->prev_panel : 0;
            else { s->prev_panel = s->p[P_PANEL]; s->p[P_PANEL] = 3; }
            s->display_rev++;
            break;
        case P_BR_MORE: s->prev_panel = s->p[P_PANEL] = 2; s->display_rev++; break;
        case P_BR_LESS: s->prev_panel = s->p[P_PANEL] = 0; s->display_rev++; break;
        case P_LEARN:   /* the first pad, then the last; every pad between them gets a chord */
            if (s->learn == 3) learn_finish(s);
            else if (s->learn) { s->learn = 0; say(s, 2000, "LEARN CANCELLED"); }
            else { s->learn = 1; s->learn_multi = 0; s->learn_chrom = 0; s->learn_nseen = 0; say(s, 12000, "LEARN: HIT THE FIRST PAD YOU WANT TO USE (ANY BANK)"); }
            break;
        default: break;   /* steppers are handled by the wrapper (step_of) */
        }
        return;
    }
    int v = clampi((int)lround(fv), PDEF[i].min, PDEF[i].max);
    int old = s->p[i];
    if (i == P_SLOT_ROOT || i == P_SLOT_TYPE || i == P_SLOT_BASS || i == P_SLOT_VAR || i == P_SLOT_LEN || i == P_SLOT_REV) {
        if (s->now < s->quiet_until) return;
        edit_slot(s, i, v);
        return;
    }
    if (i == P_CURSOR) {   /* select a chord: preview, no new ideas */
        s->cursor = clampi(v - 1, 0, s->count < NSLOT ? s->count : NSLOT - 1);
        recount(s);
        if (old != s->p[P_CURSOR]) audition_slot(s, s->cursor);
        s->want_cruise = 1;
        s->display_rev++;
        return;
    }
    if ((i >= P_KB_1 && i <= P_KB_12) || (i >= P_KBX_1 && i <= P_KBX_72)) return;   /* display only */
    int o5[NSET];
    settings_of(s, o5);
    s->p[i] = v;
    if (v == old) return;
    int quiet = s->now < s->quiet_until;
    if (i == P_KEY || i == P_SCALE || i == P_STYLE || i == P_ARTIST || i == P_PRODUCER)   /* the info line follows the last pick */
        s->p[P_BROWSE] = i == P_KEY ? 0 : i == P_SCALE ? 1 : i == P_STYLE ? 2 : i == P_ARTIST ? 3 : 4;
    switch (i) {
    case P_KEY: case P_SCALE: case P_STYLE:   /* re-fit the selected chord or all (EDIT CHORD / EDIT ALL) */
        if (quiet) { new_spot(s); break; }
        browser_change(s, i == P_KEY ? BR_KEY : i == P_SCALE ? BR_SCALE : BR_STYLE, o5);
        break;
    case P_ARTIST: case P_PRODUCER: {   /* a profile brings its usual style when it is the only profile chosen */
        if (quiet) { new_spot(s); break; }
        int prof = i == P_ARTIST ? artist_prof(s) : producer_prof(s);
        int other = i == P_ARTIST ? s->p[P_PRODUCER] : s->p[P_ARTIST];
        if (v > 0 && !other && ARTIST_INFO[prof].style >= 0) s->p[P_STYLE] = ARTIST_INFO[prof].style;
        s->p[P_LIB_KIND] = i == P_PRODUCER;   /* the LIBRARY shows the one just chosen */
        browser_change(s, i == P_ARTIST ? BR_ARTIST : BR_PRODUCER, o5);
        if (!s->count) say(s, 4000, "%s: %s", v ? ARTIST_INFO[prof].name : "NO PROFILE", ARTIST_INFO[prof].traits);
        break;
    }
    case P_SPICE: case P_STYLE_MIX: case P_ARTIST_MIX: case P_PRODUCER_MIX:   /* the progression follows these too */
        if (quiet) { new_spot(s); break; }
        browser_change(s, i == P_SPICE ? BR_SPICE : i == P_STYLE_MIX ? BR_SMIX : i == P_ARTIST_MIX ? BR_AMIX : BR_PMIX, o5);
        break;
    case P_LOOP_SYNC:
        if (!quiet) { char b[64]; loop_text(s, b, sizeof b); say(s, 4000, "%s", b); }
        s->display_rev++;
        break;
    case P_BROWSE: case P_LIB_KIND: s->display_rev++; break;
    case P_SCOPE:
        if (!quiet) say(s, 5000, "%s", scope_help(s));
        s->display_rev++;
        break;
    case P_GEN_LEN: {   /* the loop length: nothing is deleted, chords beyond it wait in grey */
        recount(s);
        int L = act_len(s);
        if (s->target > (s->count < L ? s->count : L - 1)) retarget(s, s->count < L ? s->count : L - 1);
        else if (s->target == L - 1 || s->target == 3) request_pool(s);
        if (!quiet) say(s, 6000, "%s", len_help(v));
        s->display_rev++;
        break;
    }
    case P_VOICING: case P_LEAD: case P_OCTAVE: case P_BASS: revoice(s); s->display_rev++; break;
    case P_LEVEL: case P_SOUND: case P_TONE: case P_RELEASE: case P_SPACE: apply_synth_params(s); break;
    case P_SYNTH: if (!synth_on(s)) synth_all_off(&s->syn, 0); break;
    case P_MIDI_OUT:
        if (!v) {   /* end what is sounding on the port, then stay quiet there */
            for (int c = 0; c < 16; c++)
                for (int n = 0; n < 128; n++)
                    if (s->alsa_on[c][n]) { alsa_send(s, c, n, 0); s->alsa_on[c][n] = 0; }
            s->aq.n = 0;
        } else if (!synth_on(s)) synth_all_off(&s->syn, 0);
        if (!quiet) say(s, 2500, v ? (s->p[P_SYNTH] ? "MIDI OUT ON + SYNTH" : "MIDI OUT ON - SYNTH MUTED") : "MIDI OUT OFF - SYNTH ON");
        break;
    case P_LISTEN:
        s->p[P_FOLLOW] = v == 1;
        if (v != 2) s->p[P_RUN] = 0;
        stop_seq(s);
        if (!quiet) say(s, 6000, "%s", mode_help(v));
        break;
    case P_RUN:   /* LISTEN's own PLAY (a toggle: tap again or STOP to stop) */
        if (v) {
            if (s->p[P_LISTEN] != 2) { s->p[P_LISTEN] = 2; s->p[P_FOLLOW] = 0; }
            if (!play_count(s)) { s->p[P_RUN] = 0; say(s, 2500, "NOTHING TO PLAY YET: ADD OR GENERATE CHORDS"); }
            else if (!quiet) say(s, 2000, "LISTEN: PLAYING");
        } else stop_seq(s);
        s->display_rev++;
        break;
    case P_CHANNEL: case P_BASS_CH: all_notes_off(s, SRC_SEQ | SRC_LIVE); break;
    case P_FOLLOW: s->p[P_LISTEN] = v ? 1 : 0; s->p[P_RUN] = 0; stop_seq(s); break;
    case P_MUTE: if (v) all_notes_off(s, SRC_SEQ); break;
    case P_PANEL: if (v != 3) s->prev_panel = v; s->display_rev++; break;
    case P_BARS: case P_DIRECTION: case P_PUSH: case P_RHYTHM: case P_RATE: case P_RRATE: case P_SWING: case P_PAD_COUNT:
        s->display_rev++;   /* the sequencer reads these on every block: they act at once, also while it plays */
        break;
    case P_PRESET:   /* MPC's PRESET menu or the PRESETS panel */
        if (!quiet) choose_preset(s, v);
        s->display_rev++;
        break;
    case P_SAVE_SLOT: s->display_rev++; break;
    default: break;
    }
}

/* the state as text: every option / value, the chords, and (hist) the history. A user preset is the same without
 * the history, with its name first */
static int state_text(const inst_t *s, char *buf, int len, int hist) {
    int n = snprintf(buf, len, "v=5;");
    for (int i = 0; i < NUM_P && n < len; i++)
        if (PDEF[i].kind == 0 && strncmp(PDEF[i].key, "tint_", 5) && strncmp(PDEF[i].key, "slot_", 5) &&
            strncmp(PDEF[i].key, "kb", 2) && (hist || (i != P_PRESET && i != P_SAVE_SLOT && i != P_PANEL)))
            n += snprintf(buf + n, len - n, "%s=%d;", PDEF[i].key, s->p[i]);
    n += snprintf(buf + n, len - n, "slots=");
    for (int i = 0; i < s->count && n < len; i++)
        n += snprintf(buf + n, len - n, "%d,%d,%d,%d,%d,%d%s", s->slot[i].c.root, s->slot[i].c.q, s->slot[i].c.bass,
                      s->slot[i].var, s->slot[i].len, s->slot[i].rev, i < s->count - 1 ? "|" : "");
    if (n < len) n += snprintf(buf + n, len - n, ";");
    if (hist) {
        n += snprintf(buf + n, len - n, "hpos=%d;hist=", s->hpos);
        int from = s->hn > HSAVE ? s->hn - HSAVE : 0;
        for (int e = from; e < s->hn && n < len; e++) {
            n += snprintf(buf + n, len - n, "%s%s~%d~%d~%d~%d~%d~%d~%d~%d~%d~%d~", e > from ? "^" : "", s->hist[e].label,
                          s->hist[e].key, s->hist[e].scale, s->hist[e].len, s->hist[e].style, s->hist[e].artist,
                          s->hist[e].producer, s->hist[e].spice, s->hist[e].mix[0], s->hist[e].mix[1], s->hist[e].mix[2]);
            for (int k = 0; k < s->hist[e].count && n < len; k++) {
                const slot_t *sl = &s->hist[e].slot[k];
                n += snprintf(buf + n, len - n, "%s%d,%d,%d,%d,%d,%d", k ? "|" : "", sl->c.root, sl->c.q, sl->c.bass, sl->var, sl->len, sl->rev);
            }
        }
        n += snprintf(buf + n, len - n, ";hfrom=%d;", from);
        if (s->loop_src && n < len) n += snprintf(buf + n, len - n, "hloop=%g;hstart=%g;", s->host_loop, s->loop_start);
        if (s->padmap_n && n < len) {
            n += snprintf(buf + n, len - n, "padmap=");
            for (int k = 0; k < s->padmap_n && n < len; k++) n += snprintf(buf + n, len - n, "%s%d", k ? "," : "", s->padmap[k]);
            if (n < len) n += snprintf(buf + n, len - n, ";");
        }
    }
    return n < len ? n + 1 : 0;
}

static int get_param(void *inst, const char *key, char *buf, int len) {
    inst_t *s = inst;
    int t, i;
    char nm[48], ro[24];
    if (!strncmp(key, "preset:", 7)) {   /* MPC's PRESET menu asks for a program's name without loading it */
        preset_name(s, clampi(atoi(key + 7), 0, NPRESET - 1), nm, sizeof nm);
        return snprintf(buf, len, "%s", nm) + 1;
    }
    if ((t = tile_n(key, "pre_", "_on", NUM_FACTORY_PRESETS)) >= 0) return snprintf(buf, len, "%d", s->preset_loaded && s->p[P_PRESET] == t) + 1;
    if ((t = tile_n(key, "usr_", "_on", NUM_USER_PRESETS)) >= 0)
        return snprintf(buf, len, "%d", s->preset_loaded && s->p[P_PRESET] == NUM_FACTORY_PRESETS + t) + 1;
    if ((t = tile_n(key, "pre_", "", NUM_FACTORY_PRESETS)) >= 0) return snprintf(buf, len, "%s", FPRESETS[t].name) + 1;
    if ((t = tile_n(key, "usr_", "", NUM_USER_PRESETS)) >= 0) { user_name(s, t, nm, sizeof nm); return snprintf(buf, len, "%s", nm) + 1; }
    if (!strcmp(key, "preset_info")) {
        int i = s->p[P_PRESET];
        if (!s->preset_loaded) return snprintf(buf, len, "TAP A PRESET TO LOAD IT (UNDO BRINGS YOUR STATE BACK). MPC'S PRESET MENU HAS THEM TOO") + 1;
        if (i < NUM_FACTORY_PRESETS) return snprintf(buf, len, "%s: %s", FPRESETS[i].name, FPRESETS[i].desc) + 1;
        user_name(s, i - NUM_FACTORY_PRESETS, nm, sizeof nm);
        return snprintf(buf, len, "%s - YOUR OWN, SAVED IN MPC DOCUMENTS/CHEATCHORD/PRESETS", nm) + 1;
    }
    if (!strcmp(key, "save_slot_name")) {
        int u = clampi(s->p[P_SAVE_SLOT] - 1, 0, NUM_USER_PRESETS - 1);
        if (s->unames[u][0]) return snprintf(buf, len, "USER %d (REPLACES %s)", u + 1, s->unames[u]) + 1;
        return snprintf(buf, len, "USER %d (EMPTY)", u + 1) + 1;
    }
    if (!strcmp(key, "kb_diff")) {
        if (!s->shown_ok) return snprintf(buf, len, "TURN A KNOB WHILE IT PLAYS - THE KEYS SHOW WHAT CHANGES") + 1;
        if (!s->kb_cmp) return snprintf(buf, len, "GREEN = NOTE ADDED BY THE KNOBS   DARK RED = NOTE TAKEN AWAY") + 1;
        char was[32];
        int si = -1;
        for (int i = 0; i < s->count; i++) if (chord_equal(&s->slot[i].c, &s->shown)) { si = i; break; }
        if (si >= 0 && si < s->src_n) { name_of(s, &s->src[si].c, was, sizeof was); }
        else snprintf(was, sizeof was, "-");
        return snprintf(buf, len, "WAS %s:  %s", was, s->kb_diff[0] ? s->kb_diff : "SAME NOTES, NEW SPELLING") + 1;
    }
    if (!strcmp(key, "style_info")) return snprintf(buf, len, "%s", STYLE_DESC[clampi(s->p[P_STYLE], 0, NUM_STYLES - 1)]) + 1;
    if (!strcmp(key, "learn_info")) {
        if (s->learn == 1) return snprintf(buf, len, "LEARN: HIT THE FIRST PAD") + 1;
        if (s->learn == 2) return snprintf(buf, len, "FIRST = NOTE %d - HIT THE LAST PAD", s->learn_first) + 1;
        if (s->learn == 3) return snprintf(buf, len, "TAP THE PADS BETWEEN: %d OF 16 - LEARN = DONE", s->learn_nseen) + 1;
        int n = s->p[P_PAD_COUNT], a = s->p[P_PAD_BASE];
        if (s->padmap_n && n <= 8) return snprintf(buf, len, "%d PADS BY NOTE (PAD LAYOUT): PROGRESSION 1-%d", n, n) + 1;
        if (s->padmap_n) return snprintf(buf, len, "%d PADS BY NOTE (PAD LAYOUT): PROGRESSION 1-8, IDEAS 9-%d", n, n) + 1;
        if (n <= 8) return snprintf(buf, len, "%d PADS, NOTES %d-%d: PROGRESSION 1-%d", n, a, a + n - 1, n) + 1;
        return snprintf(buf, len, "%d PADS, NOTES %d-%d: PROGRESSION 1-8, IDEAS 9-%d", n, a, a + n - 1, n) + 1;
    }
    if ((t = tile_index(key, "sug_", "_on")) >= 0) return snprintf(buf, len, "%d", s->sel == t) + 1;
    if ((t = tile_index(key, "prog_", "_on")) >= 0) {
        if (s->playing && s->play_slot >= 0) return snprintf(buf, len, "%d", s->play_slot == t) + 1;
        if (s->p[P_SCOPE] == 1) return snprintf(buf, len, "%d", t < s->count) + 1;   /* EDIT ALL: every chord is in play */
        return snprintf(buf, len, "%d", s->cursor == t) + 1;
    }
    if ((t = tile_index(key, "sug_", "")) >= 0) {
        if (t >= s->pool_n) return snprintf(buf, len, "%s", s->want_pool || atomic_load(&s->ready[JOB_POOL]) || s->pending[JOB_POOL] ? "..." : "") + 1;
        name_of(s, &s->pool[t], nm, sizeof nm);
        harmony_roman(s->p[P_KEY], s->p[P_SCALE], &s->pool[t], ro, sizeof ro);
        return snprintf(buf, len, "%s   %s", nm, ro) + 1;
    }
    if ((t = tile_index(key, "prog_", "")) >= 0) {
        int L = act_len(s);
        const char *mark = t == s->cursor ? "> " : "";
        if (!s->slot[t].filled) {
            if (t >= L) return snprintf(buf, len, "-") + 1;   /* outside the loop */
            return snprintf(buf, len, "%s", t == s->count ? (t == s->cursor ? "> +" : "+") : "") + 1;
        }
        name_of(s, &s->slot[t].c, nm, sizeof nm);
        if (t >= L) return snprintf(buf, len, "(%s)", nm) + 1;   /* kept, but not in the 4-chord loop */
        const char *ln = s->slot[t].len == 0 ? " 1/2" : s->slot[t].len == 2 ? " x2" : "";
        const char *tg = t == s->target && t != s->cursor ? "* " : "";
        return snprintf(buf, len, "%s%s%s%s%s", mark, tg, nm, ln, s->slot[t].rev ? " REV" : "") + 1;
    }
    if ((t = tile14(key, "bri_", "_on")) >= 0 || (t = tile14(key, "lib_", "_on")) >= 0) {
        int cat = key[0] == 'b' ? s->p[P_BROWSE] : (s->p[P_LIB_KIND] ? 4 : 3);
        return snprintf(buf, len, "%d", t < browse_count(cat) && s->p[BROWSE_P[clampi(cat, 0, 4)]] == t) + 1;
    }
    if ((t = tile14(key, "bri_", "")) >= 0 || (t = tile14(key, "lib_", "")) >= 0) {
        int cat = key[0] == 'b' ? s->p[P_BROWSE] : (s->p[P_LIB_KIND] ? 4 : 3);
        return snprintf(buf, len, "%s", t < browse_count(cat) ? browse_name(cat, t) : "") + 1;
    }
    if ((t = tile_index(key, "hist_", "_on")) >= 0) return snprintf(buf, len, "%d", s->hn - 1 - t == s->hpos) + 1;
    if ((t = tile_index(key, "hist_", "")) >= 0) {
        int e = s->hn - 1 - t;
        if (e < 0) { if (len > 0) buf[0] = 0; return 1; }
        char pr[120];
        prog_names(s, s->hist[e].slot, s->hist[e].count, s->hist[e].key, s->hist[e].scale, pr, sizeof pr);
        return snprintf(buf, len, "#%d %s: %s", e + 1, s->hist[e].label, pr) + 1;
    }
    if (!strncmp(key, "kb_", 3) && key[3] >= '0' && key[3] <= '9') {
        int k = atoi(key + 3);
        if (k >= 1 && k <= 12) return snprintf(buf, len, "%d", s->kb[k - 1]) + 1;
    }
    if (!strncmp(key, "kbx_", 4)) {
        int k = atoi(key + 4);
        if (k >= 1 && k <= 72) return snprintf(buf, len, "%d", s->kbx[k - 1]) + 1;
    }
    if (!strcmp(key, "notes")) {   /* every note as it sounds: the bass, then the chord from low to high */
        if (!s->shown_ok) return snprintf(buf, len, "TAP A CHORD TO SEE ITS NOTES") + 1;
        int n = 0;
        char nn[8];
        if (s->shown_v.bass >= 0) { note_name(s, s->shown_v.bass, nn, sizeof nn); n += snprintf(buf + n, len - n, "BASS %s   |  ", nn); }
        for (int k = 0; k < s->shown_v.n && n < len; k++) { note_name(s, s->shown_v.note[k], nn, sizeof nn); n += snprintf(buf + n, len - n, " %s", nn); }
        return (n < len ? n : len - 1) + 1;
    }
    if (!strcmp(key, "kb_range")) {
        char a[8], b[8];
        note_name(s, s->kb_lo, a, sizeof a);
        note_name(s, s->kb_lo + 71, b, sizeof b);
        if (s->kb_out) return snprintf(buf, len, "%s - %s  (%d SHOWN 8VA)", a, b, s->kb_out) + 1;
        return snprintf(buf, len, "%s - %s", a, b) + 1;
    }
    if (!strcmp(key, "kb_name")) {
        if (!s->shown_ok) return snprintf(buf, len, "-") + 1;
        name_of(s, &s->shown, nm, sizeof nm);
        int cnt = s->shown_v.n + (s->shown_v.bass >= 0);
        return snprintf(buf, len, "%s   (%d NOTES)", nm, cnt) + 1;
    }
    if (!strcmp(key, "scope_help")) return snprintf(buf, len, "%s", scope_help(s)) + 1;
    if (!strncmp(key, "prof_", 5)) {
        int k = atoi(key + 5);
        char b[160];
        profile_line(s, k, b, sizeof b);
        return snprintf(buf, len, "%s", b) + 1;
    }
    if (!strcmp(key, "loop_info")) { char b[96]; loop_text(s, b, sizeof b); return snprintf(buf, len, "%s", b) + 1; }
    if (!strcmp(key, "cruise_next")) {
        if (!s->cr_ok || s->want_cruise) return snprintf(buf, len, "...") + 1;
        name_of(s, &s->cr_next, nm, sizeof nm);
        return snprintf(buf, len, "NEXT: %s", nm) + 1;
    }
    if (!strcmp(key, "mix_info"))
        return snprintf(buf, len, "STYLE %d%%  ARTIST %d%%  PRODUCER %d%%  SPICE %d%%", s->p[P_STYLE_MIX], s->p[P_ARTIST_MIX],
                        s->p[P_PRODUCER_MIX], s->p[P_SPICE]) + 1;
    if (!strcmp(key, "producer_info")) return snprintf(buf, len, "%s", ARTIST_INFO[producer_prof(s)].traits) + 1;
    if (!strcmp(key, "hist_pos")) return snprintf(buf, len, "STEP %d / %d", s->hpos + 1, s->hn) + 1;
    if (!strcmp(key, "br_key")) return snprintf(buf, len, "KEY  %s", KEY_LABELS[s->p[P_KEY]]) + 1;
    if (!strcmp(key, "br_scale")) return snprintf(buf, len, "%s", SCALE_LABELS[s->p[P_SCALE]]) + 1;
    if (!strcmp(key, "br_style")) return snprintf(buf, len, "%s", STYLE_NAMES[s->p[P_STYLE]]) + 1;
    if (!strcmp(key, "br_artist")) return snprintf(buf, len, "%s", s->p[P_ARTIST] ? ARTIST_INFO[artist_prof(s)].name : "NO ARTIST") + 1;
    if (!strcmp(key, "br_producer")) return snprintf(buf, len, "%s", s->p[P_PRODUCER] ? ARTIST_INFO[producer_prof(s)].name : "NO PRODUCER") + 1;
    if (!strcmp(key, "sel1")) return snprintf(buf, len, "%s %s", KEY_LABELS[s->p[P_KEY]], SCALE_LABELS[s->p[P_SCALE]]) + 1;
    if (!strcmp(key, "sel2")) {
        int n = snprintf(buf, len, "%s", STYLE_NAMES[s->p[P_STYLE]]);
        if (s->p[P_ARTIST] && n < len) n += snprintf(buf + n, len - n, "  +  %s", ARTIST_INFO[artist_prof(s)].name);
        if (s->p[P_PRODUCER] && n < len) n += snprintf(buf + n, len - n, "  +  %s", ARTIST_INFO[producer_prof(s)].name);
        return (n < len ? n : len - 1) + 1;
    }
    if (!strcmp(key, "shown")) {
        if (!s->shown_ok) return snprintf(buf, len, "-") + 1;
        name_of(s, &s->shown, nm, sizeof nm);
        return snprintf(buf, len, "%s", nm) + 1;
    }
    if (!strcmp(key, "mode_help")) return snprintf(buf, len, "%s", mode_help(s->p[P_LISTEN])) + 1;
    if (!strcmp(key, "artist_info")) return snprintf(buf, len, "%s", ARTIST_INFO[artist_prof(s)].traits) + 1;
    if ((t = tile_index(key, "tint_", "")) >= 0) return snprintf(buf, len, "0") + 1;   /* retired: tinted tiles crashed MPC when they changed under a tap */
    if (!strcmp(key, "display_rev")) return snprintf(buf, len, "%d", s->display_rev) + 1;
    if (!strcmp(key, "state")) return state_text(s, buf, len, 1);
    if (!strcmp(key, "status")) {
        if (s->now < s->msg_until) return snprintf(buf, len, "%s", s->msg) + 1;
        char big[160];
        if (s->count && s->cr_ok && !s->want_cruise) {
            char nm[32];
            name_of(s, &s->cr_next, nm, sizeof nm);
            int L = act_len(s);
            if (s->count >= L) return snprintf(buf, len, "LOOP FULL (%d CHORDS) - CRUISE NEEDS A FREE SLOT", L) + 1;
            return snprintf(buf, len, "CRUISE SUGGESTS %s FOR CHORD %d - PRESS CRUISE TO ADD IT", nm, s->cr_target + 1) + 1;
        }
        progression_text(s, big, sizeof big);
        return snprintf(buf, len, "%s", big) + 1;
    }
    if (!strcmp(key, "info")) {
        if (s->p[P_PANEL] == 2) {   /* the browser: what the profile last chosen does, else the style */
            if (s->p[P_BROWSE] == 3 && s->p[P_ARTIST]) return snprintf(buf, len, "%s: %s", ARTIST_INFO[artist_prof(s)].name, ARTIST_INFO[artist_prof(s)].traits) + 1;
            if (s->p[P_BROWSE] == 4 && s->p[P_PRODUCER]) return snprintf(buf, len, "%s: %s", ARTIST_INFO[producer_prof(s)].name, ARTIST_INFO[producer_prof(s)].traits) + 1;
            return snprintf(buf, len, "%s", STYLE_DESC[clampi(s->p[P_STYLE], 0, NUM_STYLES - 1)]) + 1;
        }
        if (s->p[P_PANEL] == 3) return snprintf(buf, len, "EVERY NOTE AS IT SOUNDS - BASS DARK ORANGE, NEW NOTES GREEN, GONE DARK RED") + 1;
        if (s->p[P_PANEL] == 4) {
            int i = s->p[P_PRESET];
            if (s->preset_loaded && i < NUM_FACTORY_PRESETS) return snprintf(buf, len, "%s: %s", FPRESETS[i].name, FPRESETS[i].desc) + 1;
            return snprintf(buf, len, "TAP A PRESET TO LOAD IT. SAVE: PICK A USER SLOT, THEN SAVE. UNDO BRINGS YOUR STATE BACK") + 1;
        }
        if (s->p[P_PANEL] == 1)
            return snprintf(buf, len, "TAP A LINE TO GO BACK TO IT. NOTHING IS LOST: EVERY CHANGE STAYS IN THE LIST (%d)", s->hn) + 1;
        if (s->sel >= 0 && s->sel < s->pool_n) {
            name_of(s, &s->pool[s->sel], nm, sizeof nm);
            harmony_roman(s->p[P_KEY], s->p[P_SCALE], &s->pool[s->sel], ro, sizeof ro);
            return snprintf(buf, len, "%s (%s, %s): TAP AGAIN OR ADD TO %s", nm, ro,
                            class_word(harmony_class(s->p[P_KEY], s->p[P_SCALE], &s->pool[s->sel])),
                            s->target < s->count ? "REPLACE" : "KEEP IT") + 1;
        }
        if (s->busy_gen) return snprintf(buf, len, "WRITING A PROGRESSION...") + 1;
        if (!s->count) return snprintf(buf, len, "TAP AN IDEA TO HEAR IT, TAP AGAIN TO START WITH IT") + 1;
        if (s->cursor != s->target && s->cursor < s->count)
            return snprintf(buf, len, "CHORD %d SELECTED: PREVIEW ONLY. REROLL GIVES IDEAS TO REPLACE IT", s->cursor + 1) + 1;
        if (s->target < s->count) {
            name_of(s, &s->slot[s->target].c, nm, sizeof nm);
            return snprintf(buf, len, "IDEAS TO REPLACE CHORD %d (%s), FITTING ITS NEIGHBOURS", s->target + 1, nm) + 1;
        }
        name_of(s, &s->slot[s->count - 1].c, nm, sizeof nm);
        return snprintf(buf, len, "IDEAS FOR CHORD %d, AFTER %s", s->target + 1, nm) + 1;
    }
    if (!strcmp(key, "now")) {
        if (!s->playing || s->play_slot < 0) return snprintf(buf, len, s->p[P_LISTEN] == 1 ? "PRESS PLAY ON THE MPC OR LISTEN" : "PLAY IS OFF: PADS AND TAPS ONLY") + 1;
        name_of(s, &s->slot[s->play_slot].c, nm, sizeof nm);
        return snprintf(buf, len, "NOW: %s  (%d/%d)", nm, s->play_slot + 1, s->count) + 1;
    }
    if (!strcmp(key, "port")) {
        if (!s->p[P_MIDI_OUT]) return snprintf(buf, len, "MIDI OUT IS OFF (CRUISE PAGE) - ONLY THE SYNTH PLAYS") + 1;
        return snprintf(buf, len, "%s  -  CH %d%s", seq_out_status(), s->p[P_CHANNEL], s->p[P_SYNTH] ? "  +  SYNTH" : "") + 1;
    }
    if (!strcmp(key, "export_status")) return snprintf(buf, len, "%s", s->export_msg[0] ? s->export_msg : "WRITES A .MID FILE MPC CAN LOAD") + 1;
    if (!strcmp(key, "tpl_info")) {
        int ti = clampi(s->p[P_TEMPLATE], 0, NUM_TEMPLATES - 1);
        const template_t *T = &TEMPLATES[ti];
        int n = 0;
        int sc = T->minor ? (scale_is_minor(s->p[P_SCALE]) ? s->p[P_SCALE] : 1) : (scale_is_minor(s->p[P_SCALE]) ? 0 : s->p[P_SCALE]);
        for (int k = 0; k < T->n && n < len; k++) {
            chord_t c = {(s->p[P_KEY] + T->c[k].deg) % 12, T->c[k].q, T->c[k].bass >= 0 ? (s->p[P_KEY] + T->c[k].bass) % 12 : -1};
            chord_name_key(&c, s->p[P_KEY], sc, nm, sizeof nm);
            n += snprintf(buf + n, len - n, "%s%s", k ? "  " : "", nm);
        }
        return (n < len ? n : len - 1) + 1;
    }
    if (!strcmp(key, "cursor_name")) return snprintf(buf, len, "CHORD %d%s", s->cursor + 1, s->slot[s->cursor].filled ? "" : " (NEW)") + 1;
    if (!strcmp(key, "slot_root_name")) {
        if (!s->slot[s->cursor].filled) return snprintf(buf, len, "-") + 1;
        return snprintf(buf, len, "%s", pc_name_key(s->slot[s->cursor].c.root, s->p[P_KEY], s->p[P_SCALE], 0)) + 1;
    }
    if (!strcmp(key, "slot_type_name")) {
        if (!s->slot[s->cursor].filled) return snprintf(buf, len, "-") + 1;
        const char *q = QUALITIES[s->slot[s->cursor].c.q].sym;
        return snprintf(buf, len, "%s", *q ? q : "maj") + 1;
    }
    if (!strcmp(key, "slot_bass_name")) {
        const slot_t *sl = &s->slot[s->cursor];
        if (!sl->filled || sl->c.bass < 0) return snprintf(buf, len, "ROOT") + 1;
        return snprintf(buf, len, "/%s", pc_name_key(sl->c.bass, s->p[P_KEY], s->p[P_SCALE], 0)) + 1;
    }
    if (!strcmp(key, "slot_var_name")) {
        static const char *const V[4] = {"SMOOTHEST", "ALT 1", "ALT 2", "ALT 3"};
        return snprintf(buf, len, "%s", V[clampi(s->slot[s->cursor].var, 0, 3)]) + 1;
    }
    if (!strcmp(key, "bass_ch_name")) {
        if (!s->p[P_BASS_CH]) return snprintf(buf, len, "SAME AS CHORDS") + 1;
        return snprintf(buf, len, "CH %d", s->p[P_BASS_CH]) + 1;
    }
    if (!strcmp(key, "last_note")) {
        if (s->learn == 1) return snprintf(buf, len, "LEARN: HIT THE FIRST PAD NOW") + 1;
        if (s->learn == 2) return snprintf(buf, len, "FIRST = NOTE %d. HIT THE LAST PAD NOW", s->learn_first) + 1;
        if (s->last_note < 0) return snprintf(buf, len, "LAST PAD NOTE: NONE YET") + 1;
        if (s->last_pad < 0) return snprintf(buf, len, "LAST NOTE %d: NOT A PAD - PRESS LEARN", s->last_note) + 1;
        return snprintf(buf, len, "LAST NOTE %d = PAD %d", s->last_note, s->last_pad + 1) + 1;
    }
    if (!strcmp(key, "pad_name")) {
        int n = s->p[P_PAD_BASE];
        return snprintf(buf, len, "%d (%s%d)", n, pc_name(n % 12, 0), n / 12 - 2) + 1;
    }
    i = find_param(key);
    if (i < 0) return 0;
    if (PDEF[i].kind == 1) return snprintf(buf, len, "0") + 1;
    if (PDEF[i].kind == 2) { if (len > 0) buf[0] = 0; return 1; }
    {
        const slot_t *sl = &s->slot[s->cursor];
        switch (i) {
        case P_SLOT_ROOT: return snprintf(buf, len, "%d", sl->filled ? sl->c.root : s->p[P_KEY]) + 1;
        case P_SLOT_TYPE: return snprintf(buf, len, "%d", sl->filled ? sl->c.q : 0) + 1;
        case P_SLOT_BASS: return snprintf(buf, len, "%d", sl->filled && sl->c.bass >= 0 ? sl->c.bass + 1 : 0) + 1;
        case P_SLOT_VAR: return snprintf(buf, len, "%d", sl->filled ? sl->var : 0) + 1;
        case P_SLOT_LEN: return snprintf(buf, len, "%d", sl->filled ? sl->len : 1) + 1;
        case P_SLOT_REV: return snprintf(buf, len, "%d", sl->filled ? sl->rev : 0) + 1;
        case P_CURSOR: return snprintf(buf, len, "%d", s->cursor + 1) + 1;
        default: break;
        }
    }
    return snprintf(buf, len, "%d", s->p[i]) + 1;
}

static int parse_slots(char *v, slot_t *out) {   /* "r,q,b,var,len|..." -> count */
    char *s2 = NULL;
    int n = 0;
    memset(out, 0, NSLOT * sizeof(slot_t));
    for (char *it = strtok_r(v, "|", &s2); it && n < NSLOT; it = strtok_r(NULL, "|", &s2)) {
        int r, q, b, var, ln, rv = 0;
        if (sscanf(it, "%d,%d,%d,%d,%d,%d", &r, &q, &b, &var, &ln, &rv) >= 5) {
            out[n].rev = rv ? 1 : 0;
            out[n].c.root = clampi(r, 0, 11);
            out[n].c.q = clampi(q, 0, NUM_QUALITIES - 1);
            out[n].c.bass = b < 0 ? -1 : clampi(b, 0, 11);
            out[n].var = clampi(var, 0, 3);
            out[n].len = clampi(ln, 0, 2);
            out[n].filled = 1;
            n++;
        }
    }
    return n;
}

/* projects from before 4.0: the artist / producer lists were shorter and in another order, CHORD LENGTH had 4 values
 * and ARP RATE 6 */
static int conv_artist(int v) { return artist_list_index(OLD_ARTIST_LIST[clampi(v, 0, NUM_OLD_ARTIST_LIST - 1)]); }
static int conv_producer(int v) { return producer_list_index(OLD_PRODUCER_LIST[clampi(v, 0, NUM_OLD_PRODUCER_LIST - 1)]); }

static void load_state_ex(inst_t *s, const char *st, int preset);
static void load_state(inst_t *s, const char *st) { load_state_ex(s, st, 0); }

/* preset: a user preset (settings and chords) - the history stays and gets one entry */
static void load_state_ex(inst_t *s, const char *st, int preset) {
    char *buf = strdup(st), *save = NULL;
    if (!buf) return;
    int version = 1, hpos = -1, had_hist = 0, hfrom = 0;
    int raw_bars = -1, raw_rate = -1, raw_art = -1, raw_prod = -1;
    for (char *kv = strtok_r(buf, ";", &save); kv; kv = strtok_r(NULL, ";", &save)) {
        char *eq = strchr(kv, '=');
        if (!eq) continue;
        *eq = 0;
        const char *k = kv;
        char *v = eq + 1;
        if (!strcmp(k, "v")) { version = atoi(v); continue; }
        if (!strcmp(k, "slots")) { parse_slots(v, s->slot); continue; }
        if (!strcmp(k, "hpos")) { hpos = atoi(v); continue; }
        if (!strcmp(k, "hfrom")) { hfrom = atoi(v); continue; }
        if (!strcmp(k, "hloop")) { s->host_loop = atof(v); if (s->host_loop >= 1 && s->host_loop <= 256) s->loop_src = 1; else s->host_loop = 0; continue; }
        if (!strcmp(k, "hstart")) { s->loop_start = atof(v); continue; }
        if (!strcmp(k, "padmap")) {
            if (preset) continue;
            s->padmap_n = 0;
            for (char *q = v; *q && s->padmap_n < 16; ) { s->padmap[s->padmap_n++] = clampi(atoi(q), 0, 127); while (*q && *q != ',') q++; if (*q) q++; }
            continue;
        }
        if (preset && (!strcmp(k, "hist") || !strcmp(k, "hpos") || !strcmp(k, "hfrom"))) continue;
        if (!strcmp(k, "hist")) {   /* label~key~scale~len~style~artist~producer~slots ^ ... (v2: label~key~scale~len~slots) */
            s->hn = 0;
            char *s3 = NULL;
            for (char *e = strtok_r(v, "^", &s3); e && s->hn < HMAX; e = strtok_r(NULL, "^", &s3)) {
                char *f[12] = {0};
                int nf = 0, mx = version >= 4 ? 12 : version >= 3 ? 8 : 5;
                f[nf++] = e;
                for (char *c = e; *c && nf < mx; c++) if (*c == '~') { *c = 0; f[nf++] = c + 1; }
                if (nf < 4) continue;
                int i = s->hn++;
                memset(&s->hist[i], 0, sizeof s->hist[i]);
                snprintf(s->hist[i].label, sizeof s->hist[i].label, "%s", f[0]);
                s->hist[i].key = clampi(atoi(f[1]), 0, 11);
                s->hist[i].scale = clampi(atoi(f[2]), 0, NUM_SCALES - 1);
                s->hist[i].len = clampi(atoi(f[3]), 0, 1);
                s->hist[i].spice = s->p[P_SPICE];
                s->hist[i].mix[0] = s->hist[i].mix[1] = s->hist[i].mix[2] = 50;
                if (version >= 4) {
                    if (nf < 11) { s->hn--; continue; }
                    s->hist[i].style = clampi(atoi(f[4]), 0, NUM_STYLES - 1);
                    s->hist[i].artist = clampi(atoi(f[5]), 0, NUM_ARTIST_LIST - 1);
                    s->hist[i].producer = clampi(atoi(f[6]), 0, NUM_PRODUCER_LIST - 1);
                    s->hist[i].spice = clampi(atoi(f[7]), 0, 100);
                    for (int q = 0; q < 3; q++) s->hist[i].mix[q] = clampi(atoi(f[8 + q]), 0, 100);
                    s->hist[i].count = nf > 11 ? parse_slots(f[11], s->hist[i].slot) : 0;
                } else if (version >= 3) {
                    if (nf < 7) { s->hn--; continue; }
                    s->hist[i].style = clampi(atoi(f[4]), 0, NUM_STYLES - 1);
                    s->hist[i].artist = clampi(atoi(f[5]), 0, NUM_ARTIST_LIST - 1);
                    s->hist[i].producer = clampi(atoi(f[6]), 0, NUM_PRODUCER_LIST - 1);
                    s->hist[i].count = nf > 7 ? parse_slots(f[7], s->hist[i].slot) : 0;
                } else {
                    s->hist[i].style = s->p[P_STYLE];
                    s->hist[i].count = nf > 4 ? parse_slots(f[4], s->hist[i].slot) : 0;
                }
            }
            had_hist = s->hn > 0;
            continue;
        }
        if (preset && !strcmp(k, "name")) continue;
        int i = find_param(k);
        if (i == P_BARS) raw_bars = atoi(v);
        if (i == P_RATE) raw_rate = atoi(v);
        if (i == P_ARTIST) raw_art = atoi(v);
        if (i == P_PRODUCER) raw_prod = atoi(v);
        if (preset && (i == P_PRESET || i == P_SAVE_SLOT || i == P_PANEL || i == P_LISTEN || i == P_RUN || i == P_MIDI_OUT ||
                       i == P_CHANNEL || i == P_BASS_CH || i == P_PAD_BASE || i == P_PAD_COUNT)) continue;   /* the setup stays */
        if (i >= 0 && PDEF[i].kind == 0 && i != P_CURSOR) s->p[i] = clampi(atoi(v), PDEF[i].min, PDEF[i].max);
        if (i == P_CURSOR) s->cursor = atoi(v) - 1;
    }
    free(buf);
    if (version < 5) {   /* 3.x projects */
        static const int RATE_OLD[6] = {9, 7, 6, 5, 4, 3};
        if (raw_bars >= 0) s->p[P_BARS] = 11 + clampi(raw_bars, 0, 3);
        if (raw_rate >= 0) s->p[P_RATE] = RATE_OLD[clampi(raw_rate, 0, 5)];
        if (raw_art >= 0) s->p[P_ARTIST] = conv_artist(raw_art);
        if (raw_prod >= 0) s->p[P_PRODUCER] = conv_producer(raw_prod);
        for (int e = 0; e < s->hn; e++) { s->hist[e].artist = conv_artist(s->hist[e].artist); s->hist[e].producer = conv_producer(s->hist[e].producer); }
    }
    s->p[P_RUN] = 0;   /* a project never opens playing */
    recount(s);
    if (version < 2 && s->count > 4) s->p[P_GEN_LEN] = 1;   /* 1.x projects had no loop length: keep every chord playing */
    if (version < 4) s->p[P_LOOP_SYNC] = 0;   /* projects from before LOOP SYNC keep their chord lengths */
    s->rf_pending = 0;
    s->want_cruise = 1;
    rebase(s);
    s->target = s->count < act_len(s) ? s->count : act_len(s) - 1;
    recount(s);
    revoice(s);
    apply_synth_params(s);
    if (preset) { new_spot(s); return; }   /* the caller logs it */
    s->quiet_until = s->now + ms_frames(QUIET_MS);
    if (had_hist) s->hpos = clampi(hpos - hfrom, 0, s->hn - 1);
    if (!had_hist || s->hpos != s->hn - 1) {   /* make sure the state we opened with is in the list */
        if (!had_hist) s->hn = 0;
        hist_push(s, NULL, "OPENED");
    }
    new_spot(s);
}

/* ------------------------------------------------------------------------------------------------ lifecycle */
static void *create(const char *data_dir) {
    (void)data_dir;
    inst_t *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    for (int i = 0; i < NUM_P; i++) s->p[i] = PDEF[i].def;
    s->p[P_MIDI_OUT] = 1;   /* desktop: MIDI to the host and the synth at once; channel 1 */
    s->p[P_SYNTH] = 1;
    s->p[P_CHANNEL] = 1;
    s->sr = g_sr;
    s->sel = -1;
    s->play_slot = -1;
    s->last_note = -1;
    s->last_pad = -1;
    s->bpm = 120;
    s->rng = (uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)s;
    synth_init(&s->syn, SR);
    apply_synth_params(s);
    recount(s);
    s->ex = calloc(1, sizeof *s->ex);
    s->rtmp = calloc(1, sizeof *s->rtmp);
    s->fio = calloc(1, sizeof *s->fio);
    if (!s->ex || !s->rtmp || !s->fio) { free(s->ex); free(s->rtmp); free(s->fio); free(s); return NULL; }
    s->port_ok = 1;
    pthread_mutex_init(&s->m, NULL);
    pthread_cond_init(&s->cv, NULL);
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, 256 * 1024);
    if (pthread_create(&s->th, &at, worker, s) != 0) { pthread_attr_destroy(&at); free(s->ex); free(s->rtmp); free(s->fio); free(s); return NULL; }
    pthread_attr_destroy(&at);
    s->quiet_until = ms_frames(QUIET_MS);
    hist_push(s, NULL, "START");
    new_spot(s);
    send_pool_job(s);
    post_file(s, FILE_SCAN, 0);   /* the user presets' names (worker thread) */
    return s;
}

static void destroy(void *inst) {
    inst_t *s = inst;
    if (!s) return;
    all_notes_off(s, SRC_SEQ | SRC_LIVE);
    pthread_mutex_lock(&s->m);
    s->quit = 1;
    pthread_cond_signal(&s->cv);
    pthread_mutex_unlock(&s->m);
    pthread_join(s->th, NULL);
    pthread_mutex_destroy(&s->m);
    pthread_cond_destroy(&s->cv);
    free(s->ex);
    free(s->rtmp);
    free(s->fio);
    free(s);
}

static void take_result(inst_t *s) {
    for (int k = 0; k < NJOB; k++) {
        if (!atomic_load(&s->ready[k])) continue;
        if (pthread_mutex_trylock(&s->m) != 0) return;
        result_t *r = &s->res[k];
        atomic_store(&s->ready[k], 0);
        if (k == JOB_POOL) {
            if (r->seq == s->pool_seq) {   /* else outdated: a newer request is on its way */
                memcpy(s->pool, r->pool, sizeof s->pool);
                s->pool_n = r->pool_n;
                memcpy(s->seen, r->seen, sizeof s->seen);
                if (s->sel >= s->pool_n) s->sel = -1;
                s->display_rev++;
            }
            pthread_mutex_unlock(&s->m);
        } else if (k == JOB_REFIT || k == JOB_CRUISE) {
            result_t *cp = s->rtmp;
            memcpy(cp, r, sizeof *cp);
            pthread_mutex_unlock(&s->m);
            if (k == JOB_REFIT) apply_refit(s, cp);
            else apply_cruise(s, cp);
        } else if (k == JOB_FILE) {
            result_t *cp = s->rtmp;
            memcpy(cp, r, sizeof *cp);
            pthread_mutex_unlock(&s->m);
            apply_file(s, cp);
        } else if (k == JOB_GENERATE) {
            chord_t g[16];
            int n = r->gen_n;
            memcpy(g, r->gen, sizeof g);
            pthread_mutex_unlock(&s->m);
            s->busy_gen = 0;
            set_progression(s, g, n, "GENERATE");
            say(s, 3000, "NEW PROGRESSION (UNDO BRINGS THE OLD ONE BACK)");
        } else {
            snprintf(s->export_msg, sizeof s->export_msg, "%s", r->msg);
            pthread_mutex_unlock(&s->m);
            s->display_rev++;
        }
    }
}

/* desktop: the block in float (render() below keeps the MPC contract: int16) */
void cc_render_float(void *inst, float *l, float *r, int frames) {
    inst_t *s = inst;
    if (frames > 128) frames = 128;
    resolve_hits(s);
    take_result(s);
    if (s->want_pool) send_pool_job(s);
    if (s->want_cruise && !s->cr_apply) cruise_job(s, 0);
    /* the synth: events at their exact frames inside this block */
    int done = 0;
    while (done < frames) {
        int64_t at = s->now + done;
        int64_t next = s->now + frames;
        for (int i = 0; i < s->sq.n; i++) if (s->sq.e[i].frame > at && s->sq.e[i].frame < next) next = s->sq.e[i].frame;
        q_run(s, &s->sq, at + 1, 2);
        int nfr = (int)(next - at);
        if (nfr <= 0) nfr = 1;
        if (nfr > frames - done) nfr = frames - done;
        synth_render(&s->syn, l + done, r + done, nfr);
        done += nfr;
    }
    /* live notes going out of the port (pads, auditions) when the transport isn't calling song_pos */
    q_run(s, &s->aq, s->now + frames, 1);
    s->now += frames;
}

static void render(void *inst, int16_t *out, int frames) {
    float l[128], r[128];
    if (frames > 128) frames = 128;
    cc_render_float(inst, l, r, frames);
    for (int i = 0; i < frames; i++) {
        float a = l[i] * 32767.0f, b = r[i] * 32767.0f;
        out[2 * i] = (int16_t)(a > 32767 ? 32767 : a < -32768 ? -32768 : a);
        out[2 * i + 1] = (int16_t)(b > 32767 ? 32767 : b < -32768 ? -32768 : b);
    }
}

static const mpc_engine_t ENGINE = {create, destroy, midi, set_param, get_param, render, NULL, song_pos};
const mpc_engine_t *mpc_engine(void) { return &ENGINE; }

/* tests (hidden in the device build by -fvisibility=hidden) */
int cp_test_count(void *inst) { return ((inst_t *)inst)->count; }
int cp_test_pool(void *inst) { return ((inst_t *)inst)->pool_n; }
void cp_test_wait(void *inst) {
    inst_t *s = inst;
    for (int i = 0; i < 1500; i++) {
        if (s->want_pool) send_pool_job(s);
        if (s->want_cruise && !s->cr_apply) cruise_job(s, 0);
        pthread_mutex_lock(&s->m);
        int busy = atomic_load(&s->ex_busy) || atomic_load(&s->working), any = 0;
        for (int k = 0; k < NJOB; k++) { busy |= s->pending[k]; any |= atomic_load(&s->ready[k]); }
        pthread_mutex_unlock(&s->m);
        if (any) take_result(s);
        if (!busy && !any && !s->busy_gen && !s->want_pool && !s->want_cruise && !s->rf_pending && !s->cr_apply && !s->fio_busy) break;
        cc_sleep_ms(2);
    }
}
/* tools/make_xpl.py: a factory preset's state without history, for MPC's Instruments-browser presets */
int cp_test_state(void *inst, char *buf, int len) { return state_text((inst_t *)inst, buf, len, 0); }

/* ------------------------------------------------------------------------------------------------ desktop API (cc_desktop.h) */
static char g_docs[700] = "";
void cc_set_docs_root(const char *d) { snprintf(g_docs, sizeof g_docs, "%s", d ? d : ""); }
const char *cc_docs_root(void) {
    if (!g_docs[0]) snprintf(g_docs, sizeof g_docs, "CheatChord Data");
    return g_docs;
}
void cc_set_midi_sink(void *inst, cc_midi_sink_t fn, void *ctx) {
    inst_t *s = inst;
    s->sink = fn;
    s->sink_ctx = ctx;
}
void cc_set_sample_rate(void *inst, double sr) {
    inst_t *s = inst;
    if (sr < 8000 || sr > 384000 || fabs(sr - s->sr) < 0.5) return;
    all_notes_off(s, SRC_SEQ | SRC_LIVE);
    s->aq.n = s->sq.n = 0;
    /* the frame clocks keep running, rescaled so pending timeouts (messages, debounce) stay as long in time */
    double k = sr / s->sr;
    s->now = (int64_t)(s->now * k);
    s->quiet_until = (int64_t)(s->quiet_until * k);
    s->msg_until = (int64_t)(s->msg_until * k);
    s->sr = sr;
    g_sr = sr;
    synth_init(&s->syn, (float)sr);
    apply_synth_params(s);
}
int cc_write_midi(void *inst, const char *path, char *msg, int msglen) {
    inst_t *s = inst;
    if (!s->count) { snprintf(msg, msglen, "NOTHING TO EXPORT YET"); return 0; }
    export_t *x = calloc(1, sizeof *x);
    if (!x) { snprintf(msg, msglen, "OUT OF MEMORY"); return 0; }
    x->bpm = s->bpm > 20 ? s->bpm : 120;
    double cyc = cycle_beats(s);   /* one pass, at least 4 bars: a clip that loops in the DAW */
    int loops = 1;
    while (cyc * loops < 16 && loops < 8) loops++;
    x->total = cyc * loops;
    ex_ctx_t c = {x, 60000.0 / x->bpm, s};
    int mute = s->p[P_MUTE];
    s->p[P_MUTE] = 0;
    double an = s->anchor;
    s->anchor = 0;
    generate_span(s, -0.001, x->total - 0.001, ex_emit, &c);
    s->anchor = an;
    s->p[P_MUTE] = mute;
    snprintf(x->path, sizeof x->path, "%s", path);
    write_midi(x, msg, msglen);
    int ok = strncmp(msg, "SAVED", 5) == 0;
    free(x);
    return ok;
}
int cc_count(void *inst) { return ((inst_t *)inst)->count; }
int cc_loop_len(void *inst) { return act_len((inst_t *)inst); }
int cc_play_slot(void *inst) { inst_t *s = inst; return s->playing ? s->play_slot : -1; }
int cc_selected_idea(void *inst) { inst_t *s = inst; return s->sel >= 0 && s->sel < s->pool_n ? s->sel : -1; }
int cc_cursor(void *inst) { return ((inst_t *)inst)->cursor; }
int cc_target(void *inst) { return ((inst_t *)inst)->target; }
double cc_bpm(void *inst) { return ((inst_t *)inst)->bpm; }
int cc_busy(void *inst) {
    inst_t *s = inst;
    int b = s->busy_gen || s->want_pool || s->want_cruise || s->rf_pending || s->cr_apply || s->fio_busy;
    if (pthread_mutex_trylock(&s->m) == 0) {
        for (int k = 0; k < NJOB; k++) b |= s->pending[k];
        pthread_mutex_unlock(&s->m);
    }
    return b || atomic_load(&s->working);
}
int cc_shown_notes(void *inst, int *notes, int max, int *bass) {
    inst_t *s = inst;
    if (!s->shown_ok) return -1;
    *bass = s->shown_v.bass;
    int n = 0;
    for (int k = 0; k < s->shown_v.n && n < max; k++) notes[n++] = s->shown_v.note[k];
    return n;
}
int cc_kb_lo(void *inst) { return ((inst_t *)inst)->kb_lo; }
/* an idea's harmonic family: 0 in key, 1 borrowed, 2 leads to next (secondary), 3 substitute, 4 spicy; -1 none */
int cc_idea_class(void *inst, int i) {
    inst_t *s = inst;
    if (i < 0 || i >= s->pool_n) return -1;
    int c = harmony_class(s->p[P_KEY], s->p[P_SCALE], &s->pool[i]);
    return c == CLS_DIATONIC ? 0 : c == CLS_BORROWED ? 1 : c == CLS_SECONDARY ? 2 : c == CLS_SUBV ? 3 : 4;
}
int cc_slot_class(void *inst, int i) {
    inst_t *s = inst;
    if (i < 0 || i >= s->count) return -1;
    int c = harmony_class(s->p[P_KEY], s->p[P_SCALE], &s->slot[i].c);
    return c == CLS_DIATONIC ? 0 : c == CLS_BORROWED ? 1 : c == CLS_SECONDARY ? 2 : c == CLS_SUBV ? 3 : 4;
}
/* the roman numeral of chord slot i in the key */
int cc_slot_roman(void *inst, int i, char *buf, int len) {
    inst_t *s = inst;
    if (len > 0) buf[0] = 0;
    if (i < 0 || i >= s->count) return 0;
    harmony_roman(s->p[P_KEY], s->p[P_SCALE], &s->slot[i].c, buf, len);
    return 1;
}
