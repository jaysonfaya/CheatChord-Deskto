#include "theory.h"
#include <stdio.h>
#include <string.h>

/* order must match the Q_* enum in theory.h */
const quality_t QUALITIES[] = {
    {"",        3, {0, 4, 7},             F_MAJ,  0, 0},
    {"m",       3, {0, 3, 7},             F_MIN,  0, 0},
    {"dim",     3, {0, 3, 6},             F_DIM,  1, 0},
    {"aug",     3, {0, 4, 8},             F_AUG,  1, 0},
    {"sus2",    3, {0, 2, 7},             F_SUS,  0, 1},
    {"sus4",    3, {0, 5, 7},             F_SUS,  0, 1},
    {"6",       4, {0, 4, 7, 9},          F_MAJ,  1, 0},
    {"m6",      4, {0, 3, 7, 9},          F_MIN,  1, 0},
    {"maj7",    4, {0, 4, 7, 11},         F_MAJ,  1, 0},
    {"7",       4, {0, 4, 7, 10},         F_DOM,  1, 0},
    {"m7",      4, {0, 3, 7, 10},         F_MIN,  1, 0},
    {"m7b5",    4, {0, 3, 6, 10},         F_HDIM, 1, 0},
    {"dim7",    4, {0, 3, 6, 9},          F_DIM,  2, 0},
    {"m(maj7)", 4, {0, 3, 7, 11},         F_MIN,  2, 0},
    {"7sus4",   4, {0, 5, 7, 10},         F_DOM,  1, 1},
    {"add9",    4, {0, 4, 7, 14},         F_MAJ,  1, 0},
    {"madd9",   4, {0, 3, 7, 14},         F_MIN,  1, 0},
    {"6/9",     5, {0, 4, 7, 9, 14},      F_MAJ,  2, 0},
    {"maj9",    5, {0, 4, 7, 11, 14},     F_MAJ,  2, 0},
    {"9",       5, {0, 4, 7, 10, 14},     F_DOM,  2, 0},
    {"m9",      5, {0, 3, 7, 10, 14},     F_MIN,  2, 0},
    {"m11",     6, {0, 3, 7, 10, 14, 17}, F_MIN,  3, 0},
    {"9sus4",   5, {0, 5, 7, 10, 14},     F_DOM,  2, 1},
    {"13",      6, {0, 4, 7, 10, 14, 21}, F_DOM,  3, 0},
    {"maj7#11", 5, {0, 4, 7, 11, 18},     F_MAJ,  3, 0},
    {"7b9",     5, {0, 4, 7, 10, 13},     F_DOM,  3, 0},
    {"7#9",     5, {0, 4, 7, 10, 15},     F_DOM,  3, 0},
    {"7#5",     4, {0, 4, 8, 10},         F_DOM,  2, 0},
    {"maj13",   6, {0, 4, 7, 11, 14, 21}, F_MAJ,  3, 0},
    {"13sus4",  6, {0, 5, 7, 10, 14, 21}, F_DOM,  3, 1},
};
const int NUM_QUALITIES = (int)(sizeof QUALITIES / sizeof QUALITIES[0]);

const char *const SCALE_NAMES[NUM_SCALES] = {
    "Major", "Minor", "Dorian", "Mixolydian", "Lydian", "Phrygian", "Harm. Minor", "Mel. Minor"};

const int SCALES[NUM_SCALES][7] = {
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 2, 4, 5, 7, 9, 10},
    {0, 2, 4, 6, 7, 9, 11}, {0, 1, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 8, 11}, {0, 2, 3, 5, 7, 9, 11}};

int scale_is_minor(int s) { return s == 1 || s == 2 || s == 5 || s == 6 || s == 7; }

int scale_mask(int key, int scale) {
    int m = 0;
    for (int i = 0; i < 7; i++) m |= 1 << ((key + SCALES[scale][i]) % 12);
    return m;
}

int popcount12(int m) {
    int n = 0;
    for (int i = 0; i < 12; i++) n += (m >> i) & 1;
    return n;
}

/* Spelling: the key signature of the relative major decides. Minor-ish modes map to their relative major. */
int key_uses_flats(int key, int scale) {
    static const int REL[NUM_SCALES] = {0, 3, 10, 5, 7, 8, 3, 3};   /* semitones from tonic to the major parent */
    int major = (key + REL[scale]) % 12;
    /* F Bb Eb Ab Db Gb use flats */
    return major == 5 || major == 10 || major == 3 || major == 8 || major == 1 || major == 6;
}

const char *pc_name(int pc, int flats) {
    static const char *const SH[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char *const FL[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    pc = ((pc % 12) + 12) % 12;
    return flats ? FL[pc] : SH[pc];
}

void chord_name(const chord_t *c, int flats, char *buf, int len) {
    if (c->q < 0) { snprintf(buf, len, "-"); return; }
    if (c->bass >= 0 && c->bass != c->root)
        snprintf(buf, len, "%s%s/%s", pc_name(c->root, flats), QUALITIES[c->q].sym, pc_name(c->bass, flats));
    else
        snprintf(buf, len, "%s%s", pc_name(c->root, flats), QUALITIES[c->q].sym);
}

int chord_pcs(const chord_t *c) {
    int m = 0;
    const quality_t *q = &QUALITIES[c->q];
    for (int i = 0; i < q->n; i++) m |= 1 << ((c->root + q->iv[i]) % 12);
    if (c->bass >= 0) m |= 1 << (c->bass % 12);
    return m;
}

int chord_equal(const chord_t *a, const chord_t *b) {
    return a->root == b->root && a->q == b->q && a->bass == b->bass;
}

/* Spell a pitch class in a key: diatonic notes follow the key signature; chromatic ones are flat when they sit
 * on a flat degree (bII bIII bVI bVII) or the key uses flats, sharp otherwise (#IV, leading-tone diminished). */
const char *pc_name_key(int pc, int key, int scale, int dim) {
    pc = ((pc % 12) + 12) % 12;
    static const int NAT = (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11);
    int flats = key_uses_flats(key, scale);
    if ((NAT >> pc) & 1) return pc_name(pc, 0);
    if ((scale_mask(key, scale) >> pc) & 1) return pc_name(pc, flats);
    if (flats) return pc_name(pc, 1);
    if (dim) return pc_name(pc, 0);
    int rel = (pc - key + 12) % 12;
    return pc_name(pc, rel == 1 || rel == 3 || rel == 8 || rel == 10);
}

void chord_name_key(const chord_t *c, int key, int scale, char *buf, int len) {
    if (c->q < 0) { snprintf(buf, len, "-"); return; }
    int dim = QUALITIES[c->q].fam == F_DIM;
    if (c->bass >= 0 && c->bass != c->root)
        snprintf(buf, len, "%s%s/%s", pc_name_key(c->root, key, scale, dim), QUALITIES[c->q].sym,
                 pc_name_key(c->bass, key, scale, 0));
    else
        snprintf(buf, len, "%s%s", pc_name_key(c->root, key, scale, dim), QUALITIES[c->q].sym);
}
