#include "synth.h"
#include <math.h>
#include <string.h>

const char *const SOUND_NAMES[NUM_SOUNDS] = {"E-Piano", "Soft Pad", "Organ", "Synth Keys", "Strings", "Bell Keys"};

#define TWO_PI 6.28318530718f
#define TAB 4096
static float SINE[TAB + 1];
static int sine_ready;

static inline float sine(float ph) {   /* ph in cycles, any value */
    ph -= floorf(ph);
    float x = ph * TAB;
    int i = (int)x;
    float f = x - i;
    return SINE[i] + (SINE[i + 1] - SINE[i]) * f;
}

static inline float polyblep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
static inline float saw(float *ph, float inc) {
    float t = *ph;
    float y = 2.0f * t - 1.0f - polyblep(t, inc);
    t += inc;
    if (t >= 1.0f) t -= 1.0f;
    *ph = t;
    return y;
}

void synth_init(synth_t *s, float sr) {
    if (!sine_ready) {
        for (int i = 0; i <= TAB; i++) SINE[i] = sinf(TWO_PI * i / TAB);
        sine_ready = 1;
    }
    memset(s, 0, sizeof *s);
    s->sr = sr > 1000 ? sr : 44100;
    s->level = 0.7f; s->tone = 0.5f; s->release = 0.35f; s->space = 0.3f;
    static const int CL[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
    static const int AL[4] = {556, 441, 579, 464};
    double k = s->sr / 44100.0;   /* desktop: the same room at any rate */
    for (int i = 0; i < 8; i++) { int n = (int)((CL[i] + (i & 1) * 23) * k); s->comb_len[i] = n < SYN_REV_MAX ? n : SYN_REV_MAX; }
    for (int i = 0; i < 4; i++) { int n = (int)(AL[i] * k); s->ap_len[i] = n < SYN_AP_MAX ? n : SYN_AP_MAX; }
}

void synth_note_on(synth_t *s, int note, int vel) {
    if (note < 0 || note > 127) return;
    int best = -1, oldest = 0x7fffffff;
    for (int i = 0; i < SYN_VOICES; i++) if (s->v[i].active && s->v[i].note == note) { best = i; break; }   /* retrigger */
    if (best < 0) for (int i = 0; i < SYN_VOICES; i++) if (!s->v[i].active) { best = i; break; }
    if (best < 0) {   /* steal: a released voice first, else the oldest */
        for (int i = 0; i < SYN_VOICES; i++) if (s->v[i].stage == 2 && s->v[i].age < oldest) { oldest = s->v[i].age; best = i; }
        if (best < 0) for (int i = 0; i < SYN_VOICES; i++) if (s->v[i].age < oldest) { oldest = s->v[i].age; best = i; }
    }
    voice_t *v = &s->v[best];
    float keep_env = v->active ? v->env : 0;
    memset(v, 0, sizeof *v);
    v->active = 1; v->held = 1; v->note = note; v->age = ++s->age;
    v->vel = vel / 127.0f;
    v->freq = 440.0f * powf(2.0f, (note - 69) / 12.0f);
    v->env = keep_env;
    v->menv = 1.0f; v->fenv = 1.0f;
    v->detune = 0.0025f + 0.002f * ((note * 7919) % 13) / 13.0f;
    v->pan = 0.5f + 0.35f * (((note % 12) - 5.5f) / 6.0f);
    v->ph1 = ((note * 31) % 17) / 17.0f; v->ph2 = ((note * 13) % 11) / 11.0f; v->ph3 = 0.33f; v->ph4 = 0.71f;
    v->stage = 0;
}

/* REVERSE: the note sounds like a piano note played backwards: it swells from silence to full over its length (the
 * tine / filter brightness grow with it) and is cut short at its end */
void synth_note_on_rev(synth_t *s, int note, int vel, int len) {
    synth_note_on(s, note, vel);
    for (int i = 0; i < SYN_VOICES; i++)
        if (s->v[i].active && s->v[i].note == note && s->v[i].age == s->age) {
            voice_t *v = &s->v[i];
            v->stage = 3;
            v->env = 0;
            v->rev_len = len > 64 ? len : 64;
            v->rev_t = 0;
            v->fast_rel = 1;
            v->menv = 0.05f;
            v->fenv = 0.05f;
        }
}

void synth_note_off(synth_t *s, int note) {
    for (int i = 0; i < SYN_VOICES; i++)
        if (s->v[i].active && s->v[i].note == note && s->v[i].held) { s->v[i].held = 0; s->v[i].stage = 2; }
}

void synth_all_off(synth_t *s, int hard) {
    for (int i = 0; i < SYN_VOICES; i++) {
        if (hard) s->v[i].active = 0;
        else if (s->v[i].active) { s->v[i].held = 0; s->v[i].stage = 2; }
    }
}

/* per-sound envelope times (seconds) */
typedef struct { float att, dec, sus, rel, ca, cd, cr; } adsr_t;
static adsr_t adsr_of(int sound, float release) {
    float r = 0.05f + release * 2.5f;
    switch (sound) {
    case S_EPIANO: return (adsr_t){0.002f, 2.8f, 0.0f, r * 0.6f, 0, 0, 0};
    case S_PAD: return (adsr_t){0.35f, 1.0f, 0.85f, r * 1.6f, 0, 0, 0};
    case S_ORGAN: return (adsr_t){0.004f, 0.1f, 1.0f, 0.04f + release * 0.3f, 0, 0, 0};
    case S_KEYS: return (adsr_t){0.003f, 0.9f, 0.35f, r * 0.7f, 0, 0, 0};
    case S_STRINGS: return (adsr_t){0.18f, 0.8f, 0.9f, r * 1.3f, 0, 0, 0};
    default: return (adsr_t){0.001f, 3.5f, 0.0f, r * 0.8f, 0, 0, 0};
    }
}

static float render_voice(synth_t *s, voice_t *v, const adsr_t *a, float *pl, float *pr) {
    float sr = s->sr, inc = v->freq / sr, y = 0;
    /* envelope (exponential segments) */
    if (v->stage == 0) {
        v->env += (1.05f - v->env) * a->ca;
        if (v->env >= 1.0f) { v->env = 1.0f; v->stage = 1; }
    } else if (v->stage == 1) {
        float rate = a->cd;
        if (a->sus <= 0.001f) rate *= 1.0f + (v->note - 60) * 0.025f;   /* higher notes decay faster */
        v->env += (a->sus - v->env) * rate;
    } else if (v->stage == 3) {   /* reverse swell: quiet for long, then rising fast (a decay turned around) */
        float x = (float)(++v->rev_t) / (float)v->rev_len;
        if (x > 1) x = 1;
        v->env = x * x * x;
        v->menv = 0.05f + 0.95f * x * x;
        v->fenv = v->menv;
    } else {
        v->env -= v->env * (v->fast_rel ? 0.004f : a->cr);   /* a reversed note stops short (about 6 ms) */
        if (v->env < 0.0004f) { v->active = 0; return 0; }
    }
    float tone = s->tone;
    switch (s->sound) {
    case S_EPIANO: {
        /* 2-op FM tine with a short metallic attack; index follows velocity and decays */
        v->menv *= 1.0f - 1.0f / (0.35f * sr);
        float idx = (0.4f + 1.6f * v->vel * (0.4f + tone)) * (0.25f + 0.75f * v->menv);
        float mod = sine(v->ph2) * idx;
        float tine = sine(v->ph3) * 0.12f * v->menv * v->menv * v->vel;
        y = sine(v->ph1 + mod * 0.16f) * 0.8f + tine;
        y += sine(v->ph1 * 2.0f) * 0.06f;   /* a little second harmonic body */
        v->ph1 += inc; v->ph2 += inc; v->ph3 += inc * 14.0f;
        if (v->ph1 > 64) v->ph1 -= 64;
        if (v->ph2 > 64) v->ph2 -= 64;
        if (v->ph3 > 64) v->ph3 -= 64;
        y *= 0.55f + 0.45f * v->vel;
        break;
    }
    case S_BELL: {
        v->menv *= 1.0f - 1.0f / (0.9f * sr);
        float idx = (1.0f + 2.0f * v->vel * (0.3f + tone)) * v->menv;
        y = sine(v->ph1 + sine(v->ph2) * idx * 0.2f) * 0.7f + sine(v->ph3 + sine(v->ph2) * 0.1f) * 0.2f * v->menv;
        v->ph1 += inc; v->ph2 += inc * 3.5f; v->ph3 += inc * 2.0f;
        if (v->ph1 > 64) v->ph1 -= 64;
        if (v->ph2 > 64) v->ph2 -= 64;
        if (v->ph3 > 64) v->ph3 -= 64;
        y *= 0.5f + 0.5f * v->vel;
        break;
    }
    case S_ORGAN: {
        float vib = 1.0f + 0.0025f * sine(v->ph4);
        v->ph4 += 6.1f / sr;
        if (v->ph4 > 64) v->ph4 -= 64;
        float d = 0.5f + tone;
        y = sine(v->ph1 * 0.5f) * 0.32f + sine(v->ph1) * 0.42f + sine(v->ph1 * 2.0f) * 0.30f * d +
            sine(v->ph1 * 3.0f) * 0.16f * d + sine(v->ph1 * 4.0f) * 0.12f * d * d;
        v->ph1 += inc * vib;
        if (v->ph1 > 128) v->ph1 -= 128;
        v->menv *= 1.0f - 1.0f / (0.012f * sr);
        y += sine(v->ph3) * 0.08f * v->menv;   /* key click */
        v->ph3 += inc * 9.0f;
        if (v->ph3 > 64) v->ph3 -= 64;
        y *= 0.55f;
        break;
    }
    default: {   /* subtractive: pad, keys, strings */
        int nosc = s->sound == S_STRINGS ? 3 : 2;
        float dt = v->detune * (s->sound == S_PAD ? 1.4f : 1.0f);
        float vib = 1.0f;
        if (s->sound == S_STRINGS) { vib += 0.003f * sine(v->ph4); v->ph4 += 5.2f / sr; if (v->ph4 > 64) v->ph4 -= 64; }
        float o = saw(&v->ph1, inc * (1.0f - dt) * vib) + saw(&v->ph2, inc * (1.0f + dt) * vib);
        if (nosc == 3) o += saw(&v->ph3, inc * vib * 0.5f) * 0.6f;
        o *= 0.5f;
        if (s->sound == S_KEYS) v->fenv *= 1.0f - 1.0f / (0.25f * sr);
        else v->fenv *= 1.0f - 1.0f / (2.5f * sr);
        float base = s->sound == S_KEYS ? 300.0f : s->sound == S_PAD ? 500.0f : 900.0f;
        float cut = base * (1.0f + 6.0f * tone) * (1.0f + (s->sound == S_KEYS ? 6.0f : 1.5f) * v->fenv * (0.4f + 0.6f * v->vel));
        cut += v->freq * 1.5f;
        if (cut > sr * 0.2f) cut = sr * 0.2f;
        float x = 3.14159f * cut / sr, f = 2.0f * x * (1.0f - x * x * (1.0f / 6.0f)), q = 0.9f;
        v->lp1 += f * v->bp;
        float hp = o - v->lp1 - q * v->bp;
        v->bp += f * hp;
        y = v->lp1 * 0.9f;
        y *= 0.55f + 0.45f * v->vel;
        break;
    }
    }
    y *= v->env;
    *pl = y * (1.0f - v->pan * 0.6f);
    *pr = y * (0.4f + v->pan * 0.6f);
    return y;
}

void synth_render(synth_t *s, float *l, float *r, int n) {
    adsr_t a = adsr_of(s->sound, s->release);
    a.ca = 1.0f - expf(-1.0f / (a.att * s->sr + 1));
    a.cd = a.dec > 0 ? 1.0f - expf(-1.0f / (a.dec * 0.35f * s->sr)) : 1;
    a.cr = 1.0f - expf(-1.0f / (a.rel * 0.3f * s->sr + 1));
    for (int i = 0; i < n; i++) { l[i] = 0; r[i] = 0; }
    for (int k = 0; k < SYN_VOICES; k++) {
        voice_t *v = &s->v[k];
        if (!v->active) continue;
        for (int i = 0; i < n && v->active; i++) {
            float pl = 0, pr = 0;
            render_voice(s, v, &a, &pl, &pr);
            l[i] += pl; r[i] += pr;
        }
    }
    float gain = 0.32f * s->level * s->level * 1.6f;
    int chorus = s->sound == S_EPIANO || s->sound == S_PAD || s->sound == S_STRINGS || s->sound == S_KEYS;
    float wet = s->space * 0.45f;
    for (int i = 0; i < n; i++) {
        float xl = l[i] * gain, xr = r[i] * gain;
        if (chorus) {   /* light stereo chorus */
            s->ch_buf[0][s->ch_pos] = xl; s->ch_buf[1][s->ch_pos] = xr;
            float lfo = sine(s->ch_ph), lfo2 = sine(s->ch_ph + 0.25f);
            s->ch_ph += 0.6f / s->sr;
            if (s->ch_ph > 64) s->ch_ph -= 64;
            float dl = (7.0f + 3.0f * lfo) * s->sr / 1000.0f, dr = (7.0f + 3.0f * lfo2) * s->sr / 1000.0f;
            float pos_l = s->ch_pos - dl, pos_r = s->ch_pos - dr;
            while (pos_l < 0) pos_l += SYN_CH_N;
            while (pos_r < 0) pos_r += SYN_CH_N;
            int il = (int)pos_l, ir = (int)pos_r;
            float fl = pos_l - il, fr = pos_r - ir;
            float cl = s->ch_buf[0][il & (SYN_CH_N - 1)] * (1 - fl) + s->ch_buf[0][(il + 1) & (SYN_CH_N - 1)] * fl;
            float cr = s->ch_buf[1][ir & (SYN_CH_N - 1)] * (1 - fr) + s->ch_buf[1][(ir + 1) & (SYN_CH_N - 1)] * fr;
            s->ch_pos = (s->ch_pos + 1) & (SYN_CH_N - 1);
            xl = xl * 0.8f + cr * 0.35f;
            xr = xr * 0.8f + cl * 0.35f;
        }
        if (wet > 0.001f) {   /* Freeverb-style: 4 combs + 2 allpasses per side */
            float in = (xl + xr) * 0.5f * 0.3f, ol = 0, or_ = 0;
            for (int c = 0; c < 8; c++) {
                float *b = s->comb[c];
                int p = s->comb_pos[c];
                float y = b[p];
                s->comb_lp[c] = y * 0.6f + s->comb_lp[c] * 0.4f;
                b[p] = in + s->comb_lp[c] * 0.82f;
                if (++p >= s->comb_len[c]) p = 0;
                s->comb_pos[c] = p;
                if (c & 1) or_ += y; else ol += y;
            }
            for (int k = 0; k < 4; k++) {
                float *b = s->ap[k];
                int p = s->ap_pos[k];
                float *x = (k & 1) ? &or_ : &ol;
                float bo = b[p];
                b[p] = *x + bo * 0.5f;
                *x = bo - *x;
                if (++p >= s->ap_len[k]) p = 0;
                s->ap_pos[k] = p;
            }
            xl += ol * wet;
            xr += or_ * wet;
        }
        /* DC block + soft clip */
        float yl = xl - s->dcx_l + 0.995f * s->dc_l; s->dcx_l = xl; s->dc_l = yl;
        float yr = xr - s->dcx_r + 0.995f * s->dc_r; s->dcx_r = xr; s->dc_r = yr;
        l[i] = tanhf(yl);
        r[i] = tanhf(yr);
    }
}
