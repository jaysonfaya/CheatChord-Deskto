/* CheatChord desktop synth (see synth.h). Plain C11, no allocation, every state bounded. */
#include "synth.h"
#include <math.h>
#include <string.h>

const char *const SOUND_NAMES[NUM_SOUNDS] = {
    "Electric Piano", "Warm Pad", "Organ", "Synth Keys", "Strings", "Bell Keys",
    "Grand Piano", "Wurli", "Lo-Fi Keys", "Nylon Guitar", "Harp", "Vibraphone", "Clav", "Choir", "Brass", "Glass Pad",
};

#define TWO_PI 6.28318530718f
#define PI_F 3.14159265359f
#define TAB 4096
static float SINE[TAB + 1];
static int sine_ready;

static inline float sine (float ph) {   /* ph in cycles, any value */
    ph -= floorf (ph);
    float x = ph * TAB;
    int i = (int)x;
    float f = x - i;
    return SINE[i] + (SINE[i + 1] - SINE[i]) * f;
}
static inline float wrap (float p) { return p - floorf (p); }
static inline float polyblep (float t, float dt) {
    if (dt <= 0) return 0;
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
static inline float saw (float *ph, float inc) {   /* band-limited saw, phase kept in [0, 1) */
    float t = *ph;
    float y = 2.0f * t - 1.0f - polyblep (t, inc);
    *ph = wrap (t + inc);
    return y;
}
static inline float square (float *ph, float inc, float pw) {
    float t = *ph;
    float y = (t < pw ? 1.0f : -1.0f) + polyblep (t, inc) - polyblep (wrap (t - pw + 1.0f), inc);
    *ph = wrap (t + inc);
    return y;
}
static inline float noise (uint32_t *r) {
    *r ^= *r << 13; *r ^= *r >> 17; *r ^= *r << 5;
    return (float)(int32_t)*r * (1.0f / 2147483648.0f);
}
/* tan() for the filter coefficient, accurate enough below 0.4 * sr */
static inline float tanx (float x) {
    if (x > 1.35f) x = 1.35f;
    float x2 = x * x;
    return x * (1.0f + x2 * (0.333333f + x2 * (0.133333f + x2 * 0.053968f)));
}
/* TPT state-variable filter: z[0], z[1] state; returns lp, writes bp and hp */
static inline float svf (float *z, float in, float fc, float q, float sr, float *bp, float *hp) {
    float g = tanx (PI_F * fc / sr), k = 1.0f / q;
    float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    float v3 = in - z[1];
    float v1 = a1 * z[0] + a2 * v3;
    float v2 = z[1] + a2 * z[0] + a3 * v3;
    z[0] = 2.0f * v1 - z[0];
    z[1] = 2.0f * v2 - z[1];
    if (bp) *bp = v1;
    if (hp) *hp = in - k * v1 - v2;
    return v2;
}
static inline float clampf (float x, float a, float b) { return x < a ? a : x > b ? b : x; }
static inline float cents (float c) { return powf (2.0f, c / 1200.0f); }
/* a multiplier that decays to 1/e in tau seconds */
static inline float decay_mul (float tau, float sr) { return tau <= 0 ? 0.0f : expf (-1.0f / (tau * sr)); }
static inline float keyscale (int note, float per_oct) { return powf (2.0f, -(note - 60) / 12.0f * per_oct); }

/* ------------------------------------------------------------------------------------------------ per-sound shape */
typedef struct { float att, dec, sus, rel, gain; int chorus; } shape_t;
static shape_t shape_of (int snd, float release) {
    const float r = 0.08f + release * 2.4f;   /* the RELEASE knob */
    switch (snd) {
    case S_EPIANO: return (shape_t){0.001f, 0.0f, 0.0f, 0.10f + r * 0.35f, 0.36f, 1};
    case S_PAD:    return (shape_t){0.45f, 1.5f, 0.85f, 0.4f + r * 1.4f, 0.43f, 1};
    case S_ORGAN:  return (shape_t){0.004f, 0.1f, 1.0f, 0.03f + release * 0.25f, 0.65f, 0};
    case S_KEYS:   return (shape_t){0.002f, 1.2f, 0.45f, 0.08f + r * 0.5f, 0.57f, 1};
    case S_STRINGS:return (shape_t){0.28f, 1.0f, 0.9f, 0.3f + r * 1.1f, 0.45f, 1};
    case S_BELL:   return (shape_t){0.001f, 0.0f, 0.0f, 0.3f + r * 0.8f, 0.41f, 0};
    case S_PIANO:  return (shape_t){0.001f, 0.0f, 0.0f, 0.10f + r * 0.30f, 0.53f, 0};
    case S_WURLI:  return (shape_t){0.001f, 0.0f, 0.0f, 0.08f + r * 0.30f, 0.40f, 1};
    case S_LOFI:   return (shape_t){0.002f, 0.0f, 0.0f, 0.12f + r * 0.4f, 0.34f, 1};
    case S_GUITAR: return (shape_t){0.001f, 0.0f, 0.0f, 0.06f + r * 0.25f, 1.40f, 0};
    case S_HARP:   return (shape_t){0.001f, 0.0f, 0.0f, 0.3f + r * 0.8f, 1.04f, 0};
    case S_VIBES:  return (shape_t){0.001f, 0.0f, 0.0f, 0.15f + r * 0.6f, 0.42f, 0};
    case S_CLAV:   return (shape_t){0.001f, 0.0f, 0.0f, 0.03f + r * 0.1f, 0.77f, 0};
    case S_CHOIR:  return (shape_t){0.35f, 1.0f, 0.9f, 0.35f + r * 1.0f, 0.16f, 1};
    case S_BRASS:  return (shape_t){0.035f, 0.6f, 0.75f, 0.12f + r * 0.5f, 0.43f, 0};
    default:       return (shape_t){0.6f, 2.0f, 0.8f, 0.5f + r * 1.5f, 0.43f, 1};   /* glass pad */
    }
}

/* ------------------------------------------------------------------------------------------------ setup */
static void reverb_setup (synth_t *s) {
    static const int L[SYN_FDN] = {1123, 1289, 1471, 1621, 1873, 2029, 2269, 2459};
    static const int A[4] = {225, 341, 441, 556};
    const float k = s->sr / 44100.0f;
    for (int i = 0; i < SYN_FDN; i++) { int n = (int)(L[i] * k); s->fdn_len[i] = n < SYN_FDN_MAX ? n : SYN_FDN_MAX; }
    for (int i = 0; i < 4; i++) { int n = (int)(A[i] * k); s->ap_len[i] = n < SYN_AP_MAX ? n : SYN_AP_MAX; }
}

void synth_init (synth_t *s, float sr) {
    if (!sine_ready) {
        for (int i = 0; i <= TAB; i++) SINE[i] = sinf (TWO_PI * i / TAB);
        sine_ready = 1;
    }
    memset (s, 0, sizeof *s);
    s->sr = sr > 1000 ? sr : 44100;
    s->level = 0.7f; s->tone = 0.5f; s->release = 0.35f; s->space = 0.3f;
    s->rng = 0x9e3779b9u;
    s->lim = 1.0f;
    reverb_setup (s);
}

static voice_t *grab (synth_t *s, int note) {
    int best = -1, oldest = 0x7fffffff;
    for (int i = 0; i < SYN_VOICES; i++) if (s->v[i].active && s->v[i].note == note && s->v[i].stage != 2) { best = i; break; }
    if (best < 0) for (int i = 0; i < SYN_VOICES; i++) if (!s->v[i].active) { best = i; break; }
    if (best < 0) {   /* steal: the quietest released voice, else the oldest */
        float q = 1e9f;
        for (int i = 0; i < SYN_VOICES; i++) if (s->v[i].stage == 2 && s->v[i].env < q) { q = s->v[i].env; best = i; }
        if (best < 0) for (int i = 0; i < SYN_VOICES; i++) if (s->v[i].age < oldest) { oldest = s->v[i].age; best = i; }
    }
    return &s->v[best];
}

static void start_ks (synth_t *s, voice_t *v, int idx, float bright, float t60, float pluck_pos) {
    float period = s->sr / v->freq;
    if (period > SYN_KS - 4) period = SYN_KS - 4;
    if (period < 4) period = 4;
    int n = (int)floorf (period - 0.5f);   /* the loop filter adds half a sample */
    float frac = period - 0.5f - n;
    v->ks_len = n;
    v->ks_pos = 0;
    v->ks_frac = (1.0f - frac) / (1.0f + frac);   /* allpass coefficient for the fractional part */
    v->ks_ap = 0;
    v->ks_last = 0;
    v->ks_g = powf (10.0f, -3.0f * period / (t60 * s->sr));
    v->ks_bright = bright;
    /* excitation: noise, low-passed by velocity, with a comb for the pluck position */
    float *b = s->ks[idx];
    float lp = 0, a = 0.25f + 0.7f * bright;
    for (int i = 0; i < n; i++) {
        lp += a * (noise (&v->rng) - lp);
        b[i] = lp;
    }
    int off = (int)(n * pluck_pos);
    if (off > 0) for (int i = n - 1; i >= off; i--) b[i] -= b[i - off] * 0.9f;
    float mean = 0, pk = 1e-6f;
    for (int i = 0; i < n; i++) mean += b[i];
    mean /= (float)(n > 0 ? n : 1);
    for (int i = 0; i < n; i++) { b[i] -= mean; if (fabsf (b[i]) > pk) pk = fabsf (b[i]); }
    for (int i = 0; i < n; i++) b[i] *= 1.0f / pk;
}

void synth_note_on (synth_t *s, int note, int vel) {
    if (note < 0 || note > 127) return;
    voice_t *v = grab (s, note);
    const int idx = (int)(v - s->v);
    float keep = v->active ? v->env : 0;
    memset (v, 0, sizeof *v);
    v->active = 1; v->held = 1; v->note = note; v->age = ++s->age;
    v->snd = s->sound;
    v->vel = clampf (vel / 127.0f, 0.02f, 1.0f);
    v->freq = 440.0f * powf (2.0f, (note - 69) / 12.0f);
    v->rng = 0x12345u + (uint32_t)note * 2654435761u + (uint32_t)s->age * 40503u;
    v->pan = clampf (0.5f + 0.32f * (((note % 12) - 5.5f) / 6.0f) + 0.08f * ((note - 60) / 24.0f), 0.1f, 0.9f);
    for (int i = 0; i < 8; i++) v->ph[i] = wrap ((float)((note * (i + 3) * 37) % 101) / 101.0f);
    v->menv = 1.0f; v->fenv = 1.0f;
    v->lfo = wrap (note * 0.137f);
    const float tone = s->tone, sr = s->sr;
    /* a retriggered note starts from where it was (no click); the struck ones restart */
    v->env = (v->snd == S_PAD || v->snd == S_STRINGS || v->snd == S_CHOIR || v->snd == S_GLASS || v->snd == S_ORGAN) ? keep : 0.0f;
    if (v->snd == S_PIANO) {
        /* additive piano: inharmonic partials (stiff strings), strike position, two-stage decay, a detuned
         * second string for the beating, brighter with velocity */
        float B = clampf (0.00025f * expf ((note - 60) * 0.045f), 0.00004f, 0.02f);
        float tau0 = clampf (9.0f * keyscale (note, 0.42f), 0.6f, 16.0f);
        float bright = 0.35f + 0.65f * v->vel;
        float slope = 2.7f - 0.9f * bright - 0.6f * tone;
        int np = 0;
        for (int n = 1; n <= 32 && np < SYN_PART - 2; n++) {
            float f = v->freq * n * sqrtf (1.0f + B * n * n);
            if (f > 0.45f * sr || f > 14000.0f) break;
            float a = 1.0f / powf ((float)n, slope);
            a *= fabsf (sinf (PI_F * n * 0.118f)) * 0.85f + 0.15f;   /* hammer at about 1/8 of the string */
            float tau = tau0 / (1.0f + 0.35f * (n - 1) + f * 0.0004f);
            v->pph[np] = wrap (n * 0.31f);
            v->pinc[np] = f / sr;
            v->pa1[np] = a * 0.62f;
            v->pa2[np] = a * 0.38f;
            v->pd1[np] = decay_mul (tau * 0.22f, sr);
            v->pd2[np] = decay_mul (tau * 1.4f, sr);
            np++;
            if (n <= 2) {   /* the second string, a little sharp */
                v->pph[np] = 0.5f;
                v->pinc[np] = f * cents (n == 1 ? 1.1f : 0.8f) / sr;
                v->pa1[np] = a * 0.30f;
                v->pa2[np] = a * 0.28f;
                v->pd1[np] = decay_mul (tau * 0.22f, sr);
                v->pd2[np] = decay_mul (tau * 1.6f, sr);
                np++;
            }
        }
        v->np = np;
        v->aux = v->vel;   /* hammer thump level */
        v->env = 1.0f;
    } else if (v->snd == S_GUITAR) {
        start_ks (s, v, idx, 0.25f + 0.5f * v->vel * (0.5f + tone), clampf (3.2f * keyscale (note, 0.5f), 0.6f, 6.0f), 0.18f);
        v->env = 1.0f;
    } else if (v->snd == S_HARP) {
        start_ks (s, v, idx, 0.2f + 0.4f * v->vel * (0.5f + tone), clampf (5.5f * keyscale (note, 0.45f), 0.8f, 9.0f), 0.30f);
        v->env = 1.0f;
    } else if (v->snd == S_CLAV) {
        start_ks (s, v, idx, 0.85f + 0.15f * v->vel, clampf (1.3f * keyscale (note, 0.4f), 0.3f, 2.5f), 0.08f);
        v->env = 1.0f;
    } else if (v->snd == S_EPIANO || v->snd == S_WURLI || v->snd == S_LOFI || v->snd == S_BELL || v->snd == S_VIBES) {
        v->env = 1.0f;
    }
    v->gain = 1.0f;
}

void synth_note_on_rev (synth_t *s, int note, int vel, int len) {
    synth_note_on (s, note, vel);
    for (int i = 0; i < SYN_VOICES; i++)
        if (s->v[i].active && s->v[i].note == note && s->v[i].age == s->age) {
            voice_t *v = &s->v[i];
            v->stage = 3;
            v->env = 0;
            v->rev_len = len > 64 ? len : 64;
            v->rev_t = 0;
            v->fast_rel = 1;
        }
}

void synth_note_off (synth_t *s, int note) {
    for (int i = 0; i < SYN_VOICES; i++)
        if (s->v[i].active && s->v[i].note == note && s->v[i].held) { s->v[i].held = 0; if (s->v[i].stage != 3) s->v[i].stage = 2; }
}

void synth_all_off (synth_t *s, int hard) {
    for (int i = 0; i < SYN_VOICES; i++) {
        voice_t *v = &s->v[i];
        if (!v->active) continue;
        v->held = 0;
        v->stage = 2;
        if (hard) v->fast_rel = 1;   /* a 3 ms fade, never a click */
    }
}

/* ------------------------------------------------------------------------------------------------ one sample of a voice */
static float voice_sample (synth_t *s, voice_t *v, int idx, const shape_t *sh, float sr) {
    const float tone = s->tone;
    float inc = v->freq / sr, y = 0;
    v->t++;
    /* amplitude envelope */
    const int struck = sh->dec <= 0.0f;
    if (v->stage == 3) {
        float x = (float)(++v->rev_t) / (float)v->rev_len;
        if (x > 1) x = 1;
        v->env = x * x * x;
    } else if (v->stage == 2) {
        float tau = v->fast_rel ? 0.003f : sh->rel;
        v->env *= decay_mul (tau, sr);
        if (v->env < 0.00005f) { v->active = 0; return 0; }
    } else if (!struck) {
        if (v->stage == 0) {
            v->env += (1.08f - v->env) * (1.0f - decay_mul (sh->att * 0.5f, sr));
            if (v->env >= 1.0f) { v->env = 1.0f; v->stage = 1; }
        } else v->env += (sh->sus - v->env) * (1.0f - decay_mul (sh->dec, sr));
    }
    const float vel = v->vel;
    switch (v->snd) {
    case S_EPIANO: case S_LOFI: {
        /* tine piano: an FM pair for the body (index falls with time), a second pair for the bell-like tine, and the
         * pickup's asymmetric drive ("bark") with velocity */
        float wob = 1.0f;
        if (v->snd == S_LOFI) {   /* tape: slow wow and a little flutter */
            v->lfo = wrap (v->lfo + 0.45f / sr);
            v->lfo2 = wrap (v->lfo2 + 6.3f / sr);
            wob = 1.0f + 0.0045f * sine (v->lfo) + 0.0009f * sine (v->lfo2);
        }
        float tau = clampf (3.6f * keyscale (v->note, 0.55f), 0.5f, 7.0f);
        if (v->stage < 2) v->env *= decay_mul (tau, sr);
        v->menv *= decay_mul (0.55f * keyscale (v->note, 0.3f), sr);
        v->fenv *= decay_mul (0.045f, sr);
        float idx1 = (0.25f + 1.9f * vel * vel * (0.45f + tone)) * (0.2f + 0.8f * v->menv);
        float body = sine (v->ph[0] + sine (v->ph[1]) * idx1 * 0.159f);
        float ting = sine (v->ph[0] + sine (v->ph[2]) * (1.6f * vel * v->fenv) * 0.159f) - sine (v->ph[0]);
        y = body + 0.35f * ting;
        float drive = 1.0f + 1.1f * vel * vel;   /* the bark: gentle, only when played hard */
        y = (y + 0.12f * vel * y * y) * drive;
        y = y / (1.0f + 0.35f * fabsf (y)) * (1.0f + 0.35f) / drive * (0.7f + 0.3f * drive);
        if (v->snd == S_EPIANO) y = svf (v->z + 2, y, 2600.0f + 5000.0f * tone * (0.6f + 0.4f * vel), 0.6f, sr, 0, 0);
        v->ph[0] = wrap (v->ph[0] + inc * wob);
        v->ph[1] = wrap (v->ph[1] + inc * wob);
        v->ph[2] = wrap (v->ph[2] + inc * 14.0f * wob);
        if (v->snd == S_LOFI) {
            float fc = 900.0f + 2600.0f * tone + 800.0f * vel;
            y = svf (v->z, y, fc, 0.6f, sr, 0, 0);
            y += noise (&v->rng) * 0.004f * v->env;   /* a little hiss with the note */
            y *= 1.15f;
        }
        y *= 0.55f + 0.45f * vel;
        break;
    }
    case S_WURLI: {
        float tau = clampf (2.4f * keyscale (v->note, 0.6f), 0.4f, 5.0f);
        if (v->stage < 2) v->env *= decay_mul (tau, sr);
        v->menv *= decay_mul (0.35f, sr);
        float idx1 = (0.6f + 2.4f * vel * (0.4f + tone)) * (0.3f + 0.7f * v->menv);
        y = sine (v->ph[0] + sine (v->ph[0] * 1.0f) * idx1 * 0.12f + sine (v->ph[1]) * 0.25f * v->menv);
        float drive = 1.2f + 2.2f * vel;   /* reedy: odd harmonics from symmetric clipping */
        y = tanhf (y * drive) / tanhf (drive);
        y = svf (v->z, y, 3000.0f + 4000.0f * tone, 0.6f, sr, 0, 0);
        v->lfo = wrap (v->lfo + 5.6f / sr);
        y *= 0.88f + 0.12f * sine (v->lfo);   /* the built-in tremolo, gentle */
        v->ph[0] = wrap (v->ph[0] + inc);
        v->ph[1] = wrap (v->ph[1] + inc * 3.0f);
        y *= 0.5f + 0.5f * vel;
        break;
    }
    case S_PIANO: {
        float sum = 0;
        for (int k = 0; k < v->np; k++) {
            sum += sine (v->pph[k]) * (v->pa1[k] + v->pa2[k]);
            v->pph[k] = wrap (v->pph[k] + v->pinc[k]);
            v->pa1[k] *= v->pd1[k];
            v->pa2[k] *= v->pd2[k];
        }
        /* hammer thump: low-passed noise, a few milliseconds */
        if (v->aux > 0.0005f) {
            float th = svf (v->z, noise (&v->rng) * v->aux, clampf (v->freq * 3.0f, 200.0f, 4000.0f), 0.8f, sr, 0, 0);
            sum += th * 0.6f;
            v->aux *= decay_mul (0.006f, sr);
        }
        y = sum * (0.5f + 0.5f * vel);
        break;
    }
    case S_GUITAR: case S_HARP: case S_CLAV: {
        float *b = s->ks[idx];
        int n = v->ks_len, p = v->ks_pos;
        float x = b[p];
        /* loop: two-point average blended by brightness, gain from the decay time, allpass for exact tuning */
        float lpd = v->ks_bright * x + (1.0f - v->ks_bright) * 0.5f * (x + v->ks_last);
        v->ks_last = x;
        float ap = v->ks_frac * (lpd - v->ks_ap) + v->z[4];
        v->z[4] = lpd;
        v->ks_ap = ap;
        float fb = ap * v->ks_g;
        if (v->stage == 2) fb *= 0.995f;   /* damped by the hand */
        b[p] = fb;
        v->ks_pos = p + 1 >= n ? 0 : p + 1;
        y = x;
        if (v->snd == S_GUITAR) {   /* body: a soft low-mid lift, the top rolled off */
            float bp;
            svf (v->z, y, 210.0f, 1.2f, sr, &bp, 0);
            y = svf (v->z + 2, y + bp * 0.5f, 3500.0f + 3000.0f * tone, 0.7f, sr, 0, 0);
            y *= 0.8f;
        } else if (v->snd == S_CLAV) {   /* pickup: a resonant band and no lows */
            float bp, hp;
            svf (v->z, y, 1300.0f + 1500.0f * tone, 2.2f, sr, &bp, &hp);
            y = hp * 0.6f + bp * 0.9f;
        } else y *= 0.75f;
        y *= 0.55f + 0.45f * vel;
        break;
    }
    case S_VIBES: {
        /* a tuned bar: modes 1 : 4 : 10, each with its own decay, a mallet click, the motor tremolo */
        if (v->stage < 2) v->env = 1.0f;
        v->menv *= decay_mul (clampf (4.5f * keyscale (v->note, 0.5f), 0.8f, 8.0f), sr);
        v->fenv *= decay_mul (0.9f, sr);
        v->aux *= decay_mul (0.18f, sr);
        if (v->t == 1) v->aux = 1.0f;
        y = sine (v->ph[0]) * v->menv + sine (v->ph[1]) * 0.22f * v->fenv * (0.5f + vel) + sine (v->ph[2]) * 0.06f * v->aux * vel;
        v->ph[0] = wrap (v->ph[0] + inc);
        v->ph[1] = wrap (v->ph[1] + inc * 3.98f);
        v->ph[2] = wrap (v->ph[2] + inc * 10.2f);
        v->lfo = wrap (v->lfo + (3.5f + 3.0f * tone) / sr);
        y *= 0.75f + 0.25f * sine (v->lfo);
        if (v->menv < 0.0004f) v->active = 0;
        y *= 0.5f + 0.5f * vel;
        break;
    }
    case S_BELL: {
        if (v->stage < 2) v->env *= decay_mul (clampf (5.0f * keyscale (v->note, 0.4f), 1.0f, 9.0f), sr);
        v->menv *= decay_mul (1.2f, sr);
        float idx1 = (0.8f + 2.2f * vel * (0.3f + tone)) * (0.25f + 0.75f * v->menv);
        y = sine (v->ph[0] + sine (v->ph[1]) * idx1 * 0.159f) * 0.75f + sine (v->ph[2] + sine (v->ph[1]) * 0.1f) * 0.25f * v->menv;
        v->ph[0] = wrap (v->ph[0] + inc);
        v->ph[1] = wrap (v->ph[1] + inc * 3.5f);
        v->ph[2] = wrap (v->ph[2] + inc * 2.756f);
        y *= 0.5f + 0.5f * vel;
        break;
    }
    case S_ORGAN: {
        /* tonewheels: drawbars 16' 8' 5 1/3' 4' + upper ones with TONE, 3rd-harmonic percussion, key click, rotor */
        static const float R[9] = {0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f};
        float d[9] = {0.8f, 1.0f, 0.75f, 0.55f, 0.2f * tone, 0.3f * tone, 0.1f * tone, 0.15f * tone, 0.2f * tone};
        v->lfo = wrap (v->lfo + 6.4f / sr);
        float rot = sine (v->lfo);
        float vib = 1.0f + 0.0016f * rot;
        float sum = 0;
        float base = v->ph[0];
        for (int k = 0; k < 9; k++) if (d[k] > 0.001f && v->freq * R[k] < 0.45f * sr) sum += sine (base * R[k]) * d[k];
        v->ph[0] = base + inc * vib;
        if (v->ph[0] > 48.0f) v->ph[0] -= 48.0f;   /* 48 is a common multiple of every drawbar ratio x 2 */
        v->menv *= decay_mul (0.22f, sr);
        sum += sine (base * 3.0f) * 0.55f * v->menv * vel;
        v->fenv *= decay_mul (0.0025f, sr);
        sum += noise (&v->rng) * 0.08f * v->fenv;
        y = sum * 0.32f * (0.86f + 0.14f * rot);
        v->pan = 0.5f + 0.25f * rot;
        break;
    }
    case S_PAD: case S_STRINGS: case S_CHOIR: case S_BRASS: case S_KEYS: {
        float vib = 1.0f;
        if (v->snd == S_STRINGS || v->snd == S_CHOIR) {   /* vibrato after the note settles */
            v->lfo = wrap (v->lfo + 5.3f / sr);
            float onset = clampf ((float)v->t / (0.45f * sr), 0.0f, 1.0f);
            vib = 1.0f + (v->snd == S_CHOIR ? 0.0035f : 0.0028f) * onset * sine (v->lfo);
        }
        if (v->snd == S_BRASS) {   /* a scoop into the note */
            float sc = clampf ((float)v->t / (0.07f * sr), 0.0f, 1.0f);
            vib = cents (-28.0f * (1.0f - sc) * (1.0f - sc));
        }
        static const float DET[6] = {-11.0f, -4.5f, 0.0f, 5.0f, 10.5f, -18.0f};
        int nosc = v->snd == S_PAD ? 5 : v->snd == S_STRINGS ? 5 : v->snd == S_CHOIR ? 4 : v->snd == S_BRASS ? 3 : 2;
        float spread = v->snd == S_PAD ? 1.0f : v->snd == S_STRINGS ? 0.8f : v->snd == S_CHOIR ? 0.7f : 0.45f;
        float o = 0;
        for (int k = 0; k < nosc; k++) o += saw (&v->ph[k], inc * vib * cents (DET[k] * spread));
        o /= sqrtf ((float)nosc);
        if (v->snd == S_KEYS) o = o * 0.6f + square (&v->ph[6], inc * 0.5f, 0.35f) * 0.5f;
        if (v->snd == S_PAD) o += sine (v->ph[7]) * 0.35f, v->ph[7] = wrap (v->ph[7] + inc * 0.5f);
        if (v->snd == S_CHOIR) {
            /* "aah" formants, moving toward "ooh" with low TONE */
            float f1 = 520.0f + 260.0f * tone, f2 = 880.0f + 360.0f * tone, f3 = 2550.0f;
            float b1, b2, b3;
            svf (v->z, o, f1, 6.0f, sr, &b1, 0);
            svf (v->z + 2, o, f2, 7.0f, sr, &b2, 0);
            svf (v->z + 4, o, f3, 9.0f, sr, &b3, 0);
            y = b1 * 1.0f + b2 * 0.7f + b3 * 0.35f;
            y = svf (v->z + 6, y, 3400.0f, 0.6f, sr, 0, 0) * 1.6f;
        } else {
            float fc, q = 0.75f;
            if (v->snd == S_PAD) {
                float sw = clampf ((float)v->t / (1.2f * sr), 0.0f, 1.0f);
                fc = 350.0f + 1800.0f * tone * (0.5f + 0.5f * sw) + v->freq * 1.2f;
            } else if (v->snd == S_STRINGS) {
                fc = 1300.0f + 3500.0f * tone + v->freq * 1.5f;
            } else if (v->snd == S_BRASS) {
                float a = clampf ((float)v->t / (0.06f * sr), 0.0f, 1.0f);
                v->fenv *= decay_mul (0.45f, sr);
                fc = (500.0f + 3500.0f * tone) * (0.25f + a * (0.45f + 0.55f * v->fenv)) * (0.6f + 0.6f * vel) + v->freq;
            } else {   /* synth keys: a plucky 4-pole */
                v->fenv *= decay_mul (0.22f, sr);
                fc = (250.0f + 1400.0f * tone) * (1.0f + 7.0f * v->fenv * (0.4f + 0.6f * vel)) + v->freq;
                q = 1.1f;
            }
            fc = clampf (fc, 40.0f, 0.42f * sr);
            y = svf (v->z, o, fc, q, sr, 0, 0);
            y = svf (v->z + 2, y, fc, 0.7f, sr, 0, 0);   /* 4-pole */
            if (v->snd == S_STRINGS) { float hp; svf (v->z + 4, y, 160.0f, 0.7f, sr, 0, &hp); y = hp; }
        }
        y *= 0.6f + 0.4f * vel;
        break;
    }
    default: {   /* glass pad: an FM pair with a slowly breathing index */
        v->lfo = wrap (v->lfo + 0.23f / sr);
        float idx1 = (0.5f + 1.2f * tone) * (0.7f + 0.3f * sine (v->lfo));
        y = sine (v->ph[0] + sine (v->ph[1]) * idx1 * 0.159f) * 0.7f + sine (v->ph[2]) * 0.25f;
        v->ph[0] = wrap (v->ph[0] + inc);
        v->ph[1] = wrap (v->ph[1] + inc * 2.0f);
        v->ph[2] = wrap (v->ph[2] + inc * 2.003f);
        y *= 0.6f + 0.4f * vel;
        break;
    }
    }
    if (struck && v->stage < 2 && v->env < 0.00005f) { v->active = 0; return 0; }
    y *= v->env;
    /* a struck or released note ends once it has faded below -86 dB for a while */
    v->z[11] = fabsf (y) > v->z[11] ? fabsf (y) : v->z[11] * 0.9997f;
    if ((struck || v->stage == 2) && v->t > (int)(0.05f * sr) && v->z[11] < 0.00005f) v->active = 0;
    return y;
}

/* ------------------------------------------------------------------------------------------------ the block */
static void reset_all (synth_t *s) {   /* something went non-finite: silence, fresh state, no noise */
    float sr = s->sr;
    int snd = s->sound;
    float lv = s->level, tn = s->tone, rl = s->release, sp = s->space;
    synth_init (s, sr);
    s->sound = snd; s->level = lv; s->tone = tn; s->release = rl; s->space = sp;
}

void synth_render (synth_t *s, float *l, float *r, int n) {
    const float sr = s->sr;
    shape_t shapes[NUM_SOUNDS];
    for (int k = 0; k < NUM_SOUNDS; k++) shapes[k] = shape_of (k, s->release);
    for (int i = 0; i < n; i++) { l[i] = 0; r[i] = 0; }
    for (int k = 0; k < SYN_VOICES; k++) {
        voice_t *v = &s->v[k];
        if (!v->active) continue;
        const shape_t *sh = &shapes[v->snd < NUM_SOUNDS ? v->snd : 0];
        const float g = sh->gain;
        for (int i = 0; i < n && v->active; i++) {
            float y = voice_sample (s, v, k, sh, sr) * g;
            l[i] += y * (1.0f - v->pan) * 1.4f;
            r[i] += y * v->pan * 1.4f;
        }
    }
    const float target_gain = 0.5f * s->level * s->level * 1.6f;
    const float target_wet = s->space * 0.5f;
    const float target_ch = shapes[s->sound < NUM_SOUNDS ? s->sound : 0].chorus ? 1.0f : 0.0f;
    const float sm = 1.0f - expf (-1.0f / (0.02f * sr));   /* every change glides over 20 ms */
    const float t60 = 1.0f + 3.0f * s->space;
    const float att = 1.0f - expf (-1.0f / (0.0008f * sr)), rel = 1.0f - expf (-1.0f / (0.25f * sr));
    float fb[SYN_FDN];
    for (int k = 0; k < SYN_FDN; k++) fb[k] = powf (10.0f, -3.0f * s->fdn_len[k] / (t60 * sr));
    const float damp = 0.35f;
    int bad = 0;
    for (int i = 0; i < n; i++) {
        s->gain_s += (target_gain - s->gain_s) * sm;
        s->wet_s += (target_wet - s->wet_s) * sm;
        s->ch_mix += (target_ch - s->ch_mix) * sm;
        float xl = l[i] * s->gain_s, xr = r[i] * s->gain_s;
        /* stereo chorus (faded in and out with the sound) */
        s->ch_buf[0][s->ch_pos] = xl;
        s->ch_buf[1][s->ch_pos] = xr;
        if (s->ch_mix > 0.001f) {
            float lfo = sine (s->ch_ph), lfo2 = sine (s->ch_ph + 0.25f);
            float dl = (8.0f + 3.0f * lfo) * sr / 1000.0f, dr = (8.0f + 3.0f * lfo2) * sr / 1000.0f;
            float pl = (float)s->ch_pos - dl, pr = (float)s->ch_pos - dr;
            while (pl < 0) pl += SYN_CH_N;
            while (pr < 0) pr += SYN_CH_N;
            int il = (int)pl, ir = (int)pr;
            float fl = pl - il, fr = pr - ir;
            float cl = s->ch_buf[0][il & (SYN_CH_N - 1)] * (1 - fl) + s->ch_buf[0][(il + 1) & (SYN_CH_N - 1)] * fl;
            float cr = s->ch_buf[1][ir & (SYN_CH_N - 1)] * (1 - fr) + s->ch_buf[1][(ir + 1) & (SYN_CH_N - 1)] * fr;
            xl = xl * (1.0f - 0.2f * s->ch_mix) + cr * 0.35f * s->ch_mix;
            xr = xr * (1.0f - 0.2f * s->ch_mix) + cl * 0.35f * s->ch_mix;
        }
        s->ch_ph = wrap (s->ch_ph + 0.5f / sr);
        s->ch_pos = (s->ch_pos + 1) & (SYN_CH_N - 1);
        /* reverb: two diffusers, then an 8-line FDN (Householder mix, damped lines) */
        if (s->wet_s > 0.0005f) {
            float in = (xl + xr) * 0.5f;
            for (int k = 0; k < 2; k++) {
                float *b = s->ap[k];
                int p = s->ap_pos[k];
                float bo = b[p];
                float w = in + bo * 0.6f;
                b[p] = w;
                in = bo - w * 0.6f;
                s->ap_pos[k] = p + 1 >= s->ap_len[k] ? 0 : p + 1;
            }
            float o[SYN_FDN], sum = 0;
            for (int k = 0; k < SYN_FDN; k++) {
                o[k] = s->fdn[k][s->fdn_p[k]];
                s->fdn_lp[k] += (o[k] - s->fdn_lp[k]) * (1.0f - damp);
                o[k] = s->fdn_lp[k];
                sum += o[k];
            }
            sum *= 2.0f / SYN_FDN;
            float ol = 0, orr = 0;
            for (int k = 0; k < SYN_FDN; k++) {
                int p = s->fdn_p[k];
                s->fdn[k][p] = (o[k] - sum) * fb[k] + in * (k & 1 ? 0.35f : -0.35f);
                s->fdn_p[k] = p + 1 >= s->fdn_len[k] ? 0 : p + 1;
                if (k & 1) orr += o[k]; else ol += o[k];
            }
            xl += ol * 0.45f * s->wet_s;
            xr += orr * 0.45f * s->wet_s;
        }
        /* DC block, then a peak limiter at -1 dB and a soft clip that never reaches full scale */
        float yl = xl - s->dcx_l + 0.9995f * s->dc_l; s->dcx_l = xl; s->dc_l = yl;
        float yr = xr - s->dcx_r + 0.9995f * s->dc_r; s->dcx_r = xr; s->dc_r = yr;
        float pk = fabsf (yl) > fabsf (yr) ? fabsf (yl) : fabsf (yr);
        float want = pk > 0.85f ? 0.85f / pk : 1.0f;
        s->lim += (want - s->lim) * (want < s->lim ? att : rel);
        yl *= s->lim;
        yr *= s->lim;
        yl = yl / (1.0f + 0.15f * yl * yl);
        yr = yr / (1.0f + 0.15f * yr * yr);
        if (!(yl == yl) || !(yr == yr) || fabsf (yl) > 4.0f || fabsf (yr) > 4.0f) { bad = 1; yl = yr = 0; }
        l[i] = clampf (yl, -0.98f, 0.98f);
        r[i] = clampf (yr, -0.98f, 0.98f);
    }
    if (bad) {
        for (int i = 0; i < n; i++) l[i] = r[i] = 0;
        reset_all (s);
    }
}
