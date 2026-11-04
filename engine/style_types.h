/* Chord Pilot: how a style (and an artist profile on top of it) is described to the harmony model. */
#pragma once
#include "theory.h"

typedef struct { signed char frame, deg; short fam; float w; } tok_t;   /* frame 0 major key, 1 minor key, 2 both */
typedef struct { signed char frame, from, to; short ffam, tfam; float w; } idiom_t;
typedef struct { signed char q; float w; } qpref_t;

/* A style. For an artist profile the numbers are ADDED to the chosen style's (deltas; base_cx too) and the lists
 * are added on top of the style's lists. */
typedef struct {
    float base_cx, borrow, secdom, chroma, vamp, slash, sus, fifths, steps, thirds, mediant, retro;
    const tok_t *toks;
    const idiom_t *idioms;
    const qpref_t *qp;
    const tok_t *starts;
} style_t;

#define END_T {-1, 0, 0, 0}
#define END_I {-1, 0, 0, 0, 0, 0}
#define END_Q {-1, 0}

/* family masks for the tables */
#define FM_M  (1 << F_MAJ)
#define FM_m  (1 << F_MIN)
#define FM_D  (1 << F_DOM)
#define FM_o  (1 << F_DIM)
#define FM_h  (1 << F_HDIM)
#define FM_s  (1 << F_SUS)
#define FM_A  (1 << F_AUG)
#define FM_MX (FM_M | FM_D | FM_s)
