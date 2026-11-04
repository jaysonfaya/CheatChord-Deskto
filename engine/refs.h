/* CheatChord reference tracks: one real recording per artist / producer profile, with the key and chords as
 * documented in public sources (named in each entry). The documented chord moves are fed to the harmony model as
 * extra idioms of that profile, weighted by its influence control, so a profile measurably leans towards the moves
 * of its reference. New progressions are generated from these moves; songs are never copied.
 *
 * verified: 2 = key and chord sequence from at least one source (plus key from a second where possible)
 *           1 = key (and chord set or production facts) only: the sequence was not reliably documented, so no moves
 *               are taken from it (n = 0) or only moves derived from the documented chord set (marked in note). */
#pragma once
#include "style_types.h"

#define REF_MAXN 8
typedef struct {
    int profile;            /* index into ARTISTS / ARTIST_INFO */
    const char *track, *artist, *credit;
    const char *key;        /* as documented */
    const char *chords;     /* as documented ("" if not reliably documented) */
    const char *roman;      /* the same in Roman numerals ("" if none) */
    const char *feature;    /* what the profile takes from it */
    const char *source;     /* where key and chords come from */
    int verified;
    signed char frame;      /* 0 major / mixolydian frame, 1 minor / dorian frame */
    signed char n;          /* chord moves used: deg[0] -> deg[1] -> ... -> deg[n-1] -> deg[0] */
    signed char deg[REF_MAXN];
    short fam[REF_MAXN];
} ref_t;

extern const ref_t REFS[];
extern const int NUM_REFS;
/* the reference of a profile (NULL: none) */
const ref_t *ref_of(int profile);
