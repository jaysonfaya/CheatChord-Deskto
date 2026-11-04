/* Chord Pilot progression library: well-known progression formulas, written as scale degrees so they work in
 * every key. (Chord progressions are common musical building blocks; these are the generic patterns, not songs.) */
#pragma once
#include "theory.h"

typedef struct { signed char deg, q, bass; } tchord_t;   /* bass: semitones above the tonic, -1 = root */
typedef struct {
    const char *name;
    int minor;          /* 1: written for a minor key */
    int style;          /* the style it fits best (for the hint text) */
    int n;
    tchord_t c[8];
} template_t;

extern const template_t TEMPLATES[];
extern const int NUM_TEMPLATES;
