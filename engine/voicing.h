/* Chord Pilot voicings: turn a chord symbol into MIDI notes, with smooth voice leading. */
#pragma once
#include "theory.h"

enum { V_CLOSE, V_OPEN, V_DROP2, V_SPREAD, V_NEO, V_SHELL, NUM_VOICINGS };
extern const char *const VOICING_NAMES[NUM_VOICINGS];

#define MAX_VOICE 8

typedef struct {
    int n;                 /* upper-structure notes */
    int note[MAX_VOICE];   /* sorted ascending */
    int bass;              /* bass note (-1: none) */
} voiced_t;

/* style: V_*; octave: -2..2; lead: smooth voice leading from *prev (may be NULL or empty);
 * variant: 0 = best, 1..3 = next-best inversions (or plain inversion count without lead);
 * want_bass: add the root (or slash bass) an octave or two below */
void voice_chord(const chord_t *c, int style, int octave, int lead, int variant, int want_bass,
                 const voiced_t *prev, voiced_t *out);
