/* Chord Pilot preview synth: a small polyphonic instrument so the plugin sounds good on its own track. */
#pragma once

enum { S_EPIANO, S_PAD, S_ORGAN, S_KEYS, S_STRINGS, S_BELL, NUM_SOUNDS };
extern const char *const SOUND_NAMES[NUM_SOUNDS];

#define SYN_VOICES 16
#define SYN_REV_MAX 7600   /* desktop: room for 192 kHz (the comb lengths scale with the rate) */
#define SYN_CH_N 4096      /* chorus buffer: 10 ms at 192 kHz fits */
#define SYN_AP_MAX 2700

typedef struct {
    int active, note, held, age;
    float vel, freq, ph1, ph2, ph3, ph4, env, aenv_state, menv, fenv, lp1, lp2, bp, detune, pan;
    int stage;   /* 0 attack 1 decay/sustain 2 release 3 reverse swell */
    int rev_len, rev_t, fast_rel;   /* 4.1: a reversed note swells up over rev_len samples and stops short */
} voice_t;

typedef struct {
    float sr;
    int sound;
    float level, tone, release, space;
    voice_t v[SYN_VOICES];
    int age;
    /* chorus */
    float ch_buf[2][SYN_CH_N];
    int ch_pos;
    float ch_ph;
    /* reverb */
    float comb[8][SYN_REV_MAX];
    int comb_len[8], comb_pos[8];
    float comb_lp[8];
    float ap[4][SYN_AP_MAX];
    int ap_len[4], ap_pos[4];
    float dc_l, dc_r, dcx_l, dcx_r;
} synth_t;

void synth_init(synth_t *s, float sr);
void synth_note_on(synth_t *s, int note, int vel);
void synth_note_off(synth_t *s, int note);
void synth_note_on_rev(synth_t *s, int note, int vel, int len);   /* len: samples until the note's end */
void synth_all_off(synth_t *s, int hard);
void synth_render(synth_t *s, float *l, float *r, int n);
