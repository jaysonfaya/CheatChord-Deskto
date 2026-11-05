/* CheatChord desktop synth: 16 instruments so the plugin sounds good on its own track.
 * Each voice keeps the instrument it started with (a sound change never touches a ringing note), every block passes a
 * peak limiter and a NaN guard, and the reverb is an 8-line feedback delay network. */
#pragma once
#include <stdint.h>

enum {
    S_EPIANO, S_PAD, S_ORGAN, S_KEYS, S_STRINGS, S_BELL,          /* 0-5: the MPC build's sounds, rebuilt */
    S_PIANO, S_WURLI, S_LOFI, S_GUITAR, S_HARP, S_VIBES, S_CLAV, S_CHOIR, S_BRASS, S_GLASS,
    NUM_SOUNDS
};
extern const char *const SOUND_NAMES[NUM_SOUNDS];

#define SYN_VOICES 32
#define SYN_PART 40          /* piano partials per voice */
#define SYN_KS 8192          /* plucked-string delay per voice (23 Hz at 192 kHz) */
#define SYN_CH_N 4096        /* chorus buffer: 10 ms at 192 kHz fits */
#define SYN_FDN 8
#define SYN_FDN_MAX 16384
#define SYN_AP_MAX 4096

typedef struct {
    int active, note, held, age, snd, stage;   /* stage 0 attack, 1 decay/sustain, 2 release, 3 reverse swell */
    int rev_len, rev_t, fast_rel, t;
    float vel, freq, env, pan, gain;
    float ph[8];             /* oscillator phases, always in [0, 1) */
    float menv, fenv, aux;   /* modulation envelopes */
    float z[12];             /* filter states */
    float lfo, lfo2;
    uint32_t rng;
    /* piano: partial phases, increments, amplitudes and two decay multipliers each */
    int np;
    float pph[SYN_PART], pinc[SYN_PART], pa1[SYN_PART], pa2[SYN_PART], pd1[SYN_PART], pd2[SYN_PART];
    /* plucked strings */
    int ks_len, ks_pos;
    float ks_frac, ks_g, ks_bright, ks_last, ks_ap;
} voice_t;

typedef struct {
    float sr;
    int sound;
    float level, tone, release, space;
    voice_t v[SYN_VOICES];
    float ks[SYN_VOICES][SYN_KS];
    int age;
    /* chorus */
    float ch_buf[2][SYN_CH_N];
    int ch_pos;
    float ch_ph, ch_mix;
    /* reverb */
    float fdn[SYN_FDN][SYN_FDN_MAX];
    int fdn_len[SYN_FDN], fdn_p[SYN_FDN];
    float fdn_lp[SYN_FDN];
    float ap[4][SYN_AP_MAX];
    int ap_len[4], ap_pos[4];
    /* output */
    float dc_l, dc_r, dcx_l, dcx_r, lim, wet_s, gain_s;
    uint32_t rng;
} synth_t;

void synth_init(synth_t *s, float sr);
void synth_note_on(synth_t *s, int note, int vel);
void synth_note_off(synth_t *s, int note);
void synth_note_on_rev(synth_t *s, int note, int vel, int len);   /* len: samples until the note's end */
void synth_all_off(synth_t *s, int hard);
void synth_render(synth_t *s, float *l, float *r, int n);
