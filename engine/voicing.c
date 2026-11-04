#include "voicing.h"
#include <stdlib.h>
#include <string.h>

const char *const VOICING_NAMES[NUM_VOICINGS] = {"Close", "Open", "Drop 2", "Spread", "Neo Soul", "Shell"};

/* the upper-structure pitch classes, in "stacking order" (root first), for a voicing style */
static int upper_tones(const chord_t *c, int style, int *iv) {
    const quality_t *q = &QUALITIES[c->q];
    int n = 0, has_ext = q->n >= 5, alt5 = 0;
    for (int i = 0; i < q->n; i++) if (q->iv[i] % 12 == 6 || q->iv[i] % 12 == 8) alt5 = 1;
    int third = -1, seventh = -1;
    for (int i = 1; i < q->n; i++) {
        int x = q->iv[i] % 12;
        if (x == 3 || x == 4 || (q->sus && (x == 2 || x == 5) && third < 0)) third = q->iv[i];
        if (x == 10 || x == 11 || (x == 9 && q->fam == F_DIM)) seventh = q->iv[i];
    }
    if (style == V_SHELL) {
        iv[n++] = 0;
        if (third >= 0) iv[n++] = third % 12;
        if (seventh >= 0) iv[n++] = seventh % 12;
        else if (q->n > 3 && q->iv[3] % 12 == 9) iv[n++] = 9;                 /* 6 chords */
        else iv[n++] = 7;
        return n;
    }
    if (style == V_NEO) {   /* rootless: 3 7 9 (+13/11), triads get a 9 for colour */
        if (third >= 0) iv[n++] = third % 12;
        if (seventh >= 0) iv[n++] = seventh % 12;
        else if (q->n > 3 && q->iv[3] % 12 == 9) iv[n++] = 9;
        int has9 = 0;
        for (int i = 1; i < q->n; i++) {
            int x = q->iv[i];
            if (x > 12 && n < 5) { iv[n++] = x % 12; if (x % 12 == 2 || x % 12 == 1 || x % 12 == 3) has9 = 1; }
        }
        if (!has9 && q->fam != F_DIM && q->fam != F_HDIM && n < 4) iv[n++] = 2;
        if (n < 3) iv[n++] = alt5 ? q->iv[2] % 12 : 7;
        return n;
    }
    for (int i = 0; i < q->n && n < 6; i++) {
        int x = q->iv[i] % 12;
        if (x == 7 && has_ext && !alt5 && q->n >= 5) continue;            /* drop the 5th under tensions */
        if (q->n >= 6 && i > 0 && x == 5 && q->fam != F_MIN && !q->sus) continue;   /* 11 clashes on major */
        iv[n++] = x;
    }
    return n;
}

static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

/* stack pcs (root-relative intervals iv[0..n-1]) as a close voicing starting with element r, lowest note at >= base */
static int stack_close(int root, const int *iv, int n, int r, int base, int *out) {
    int pcs[MAX_VOICE] = {0};
    if (n < 1) { out[0] = base; return 1; }
    for (int i = 0; i < n; i++) pcs[i] = (root + iv[i]) % 12;
    /* order pcs ascending from the starting pc */
    int ord[MAX_VOICE] = {0}, k = 0;
    int start = pcs[r % n];
    int sorted[MAX_VOICE];
    memcpy(sorted, pcs, n * sizeof(int));
    for (int i = 0; i < n; i++) sorted[i] = (sorted[i] - start + 12) % 12;
    qsort(sorted, n, sizeof(int), cmp_int);
    for (int i = 0; i < n; i++) ord[k++] = (sorted[i] + start) % 12;
    int note = base + ((ord[0] - base) % 12 + 12) % 12;
    out[0] = note;
    for (int i = 1; i < k; i++) {
        int x = out[i - 1] + 1;
        while (x % 12 != ord[i]) x++;
        out[i] = x;
    }
    return k;
}

static void build(const chord_t *c, int style, int r, int base, voiced_t *v) {
    int iv[MAX_VOICE], n = upper_tones(c, style, iv);
    int notes[MAX_VOICE];
    int k;
    if (style == V_SPREAD) {
        /* 1-5-3-7-9 style: stack in fifths-ish order with wide gaps, start rotates */
        int ordr[MAX_VOICE] = {0}, m = 0;
        static const int PREF[] = {0, 7, 6, 8, 4, 3, 5, 2, 10, 11, 9, 1};
        for (int p = 0; p < 12; p++)
            for (int i = 0; i < n; i++) if (iv[i] == PREF[p]) ordr[m++] = iv[i];
        int rot[MAX_VOICE] = {0};
        if (m < 1) { ordr[m++] = 0; }
        for (int i = 0; i < m; i++) rot[i] = ordr[(i + r) % m];
        k = 0;
        int x = base + (((c->root + rot[0]) % 12 - base) % 12 + 12) % 12;
        notes[k++] = x;
        for (int i = 1; i < m; i++) {
            int y = x + 3;
            while (y % 12 != (c->root + rot[i]) % 12) y++;
            notes[k++] = y;
            x = y;
        }
    } else {
        k = stack_close(c->root, iv, n, r, base, notes);
        if (style == V_DROP2 && k >= 4) notes[k - 2] -= 12;
        if (style == V_OPEN && k >= 3) for (int i = 1; i < k; i += 2) notes[i] += 12;
        qsort(notes, k, sizeof(int), cmp_int);
    }
    v->n = k;
    memcpy(v->note, notes, k * sizeof(int));
    v->bass = -1;
}

static float mean_of(const voiced_t *v) {
    float s = 0;
    for (int i = 0; i < v->n; i++) s += v->note[i];
    return v->n ? s / v->n : 60;
}

static float lead_cost(const voiced_t *a, const voiced_t *b) {
    /* each note of b to its nearest in a, plus the outer voices weighted (top voice matters most) */
    float c = 0;
    for (int i = 0; i < b->n; i++) {
        int best = 99;
        for (int j = 0; j < a->n; j++) { int d = abs(b->note[i] - a->note[j]); if (d < best) best = d; }
        c += best;
    }
    c += 1.5f * abs(b->note[b->n - 1] - a->note[a->n - 1]);
    c += 0.5f * abs(b->note[0] - a->note[0]);
    return c;
}

void voice_chord(const chord_t *c, int style, int octave, int lead, int variant, int want_bass,
                 const voiced_t *prev, voiced_t *out) {
    int iv[MAX_VOICE], n = upper_tones(c, style, iv);
    float center = 64.0f + 12.0f * octave;
    if (style == V_SPREAD) center += 3;
    enum { MAXC = 3 * MAX_VOICE };
    voiced_t cand[MAXC];
    float cost[MAXC];
    int nc = 0;
    for (int r = 0; r < n; r++) {
        voiced_t v;
        build(c, style, r, (int)center - 12, &v);
        /* centre it */
        float m = mean_of(&v);
        int shift = 0;
        while (m + shift > center + 6) shift -= 12;
        while (m + shift < center - 6) shift += 12;
        for (int o = -12; o <= 12; o += 12) {
            voiced_t w = v;
            for (int i = 0; i < w.n; i++) w.note[i] += shift + o;
            if (w.note[0] < 40 || w.note[w.n - 1] > 98) continue;
            float d = mean_of(&w) - center;
            float cst = 0.35f * (d < 0 ? -d : d);
            if (lead && prev && prev->n) cst += lead_cost(prev, &w);
            else cst += (o != 0) * 50.0f + r * 2.0f;   /* no lead: plain inversion order */
            if (nc < MAXC) { cand[nc] = w; cost[nc] = cst; nc++; }
        }
    }
    if (!nc) { build(c, style, 0, 52, out); }
    else {
        /* sort by cost, drop duplicates */
        for (int i = 0; i < nc; i++)
            for (int j = i + 1; j < nc; j++)
                if (cost[j] < cost[i]) { float t = cost[i]; cost[i] = cost[j]; cost[j] = t; voiced_t x = cand[i]; cand[i] = cand[j]; cand[j] = x; }
        int uniq[MAXC], nu = 0;
        for (int i = 0; i < nc; i++) {
            int dup = 0;
            for (int j = 0; j < nu && !dup; j++)
                if (cand[uniq[j]].n == cand[i].n && !memcmp(cand[uniq[j]].note, cand[i].note, cand[i].n * sizeof(int))) dup = 1;
            if (!dup) uniq[nu++] = i;
        }
        *out = cand[uniq[variant % nu]];
    }
    out->bass = -1;
    if (want_bass || style == V_NEO) {
        int bpc = c->bass >= 0 ? c->bass : c->root;
        int b = 36 + bpc % 12;                           /* C1..B1 in MPC terms (MIDI 36-47) */
        if (b > 43) b -= 12;                             /* keep it low: E0..G1 range */
        if (lead && prev && prev->bass >= 0) {
            int best = b;
            for (int o = -12; o <= 12; o += 12)
                if (b + o >= 28 && b + o <= 50 && abs(b + o - prev->bass) < abs(best - prev->bass)) best = b + o;
            b = best;
        }
        while (out->n && b > out->note[0] - 3) b -= 12;
        if (b < 24) b += 12;
        out->bass = b;
    }
    /* a slash chord without a bass voice: put the slash note at the bottom of the upper structure */
    if (out->bass < 0 && c->bass >= 0 && out->n < MAX_VOICE) {
        int b = out->note[0] - 1;
        while (b % 12 != c->bass % 12) b--;
        memmove(out->note + 1, out->note, out->n * sizeof(int));
        out->note[0] = b;
        out->n++;
    }
}
