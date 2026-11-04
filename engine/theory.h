/* Chord Pilot: music theory basics (pitch classes, chord qualities, scales, names). */
#pragma once
#include <stdint.h>

enum { F_MAJ, F_MIN, F_DOM, F_DIM, F_HDIM, F_SUS, F_AUG, NUM_FAM };

typedef struct {
    const char *sym;     /* appended to the root name: "", "m", "maj7", "m9" ... */
    int n;               /* number of intervals */
    int iv[7];           /* intervals above the root (may exceed 12 for tensions) */
    int fam;             /* F_* */
    int cx;              /* complexity 0 (triad) .. 3 (altered / 13ths) */
    int sus;             /* 1: suspended (no third) */
} quality_t;

extern const quality_t QUALITIES[];
extern const int NUM_QUALITIES;

/* quality indices used by name in the code */
enum {
    Q_MAJ, Q_MIN, Q_DIM, Q_AUG, Q_SUS2, Q_SUS4, Q_6, Q_M6, Q_MAJ7, Q_7, Q_M7, Q_M7B5, Q_DIM7, Q_MMAJ7,
    Q_7SUS4, Q_ADD9, Q_MADD9, Q_69, Q_MAJ9, Q_9, Q_M9, Q_M11, Q_9SUS4, Q_13, Q_MAJ7S11, Q_7B9, Q_7S9,
    Q_7S5, Q_MAJ13, Q_13SUS4
};

typedef struct { int root, q, bass; } chord_t;   /* bass: -1 = root, else a pitch class (slash chord) */

#define NUM_SCALES 8
extern const char *const SCALE_NAMES[NUM_SCALES];
extern const int SCALES[NUM_SCALES][7];
int scale_is_minor(int scale);               /* minor-ish tonality (minor, dorian, phrygian, harm., mel.) */
int scale_mask(int key, int scale);          /* bit pc set */

int key_uses_flats(int key, int scale);
const char *pc_name(int pc, int flats);
void chord_name(const chord_t *c, int flats, char *buf, int len);
int chord_pcs(const chord_t *c);             /* bit mask of pitch classes (incl. bass) */
int chord_equal(const chord_t *a, const chord_t *b);
int popcount12(int m);
const char *pc_name_key(int pc, int key, int scale, int dim);
void chord_name_key(const chord_t *c, int key, int scale, char *buf, int len);
