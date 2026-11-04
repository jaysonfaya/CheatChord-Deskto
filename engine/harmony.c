/* Chord Pilot harmony model.
 *
 * Every candidate chord (12 roots x 30 qualities, plus slash-bass variants of the best) gets a score made of:
 *   - what kind of chord it is in the key: diatonic, borrowed from the parallel mode, a secondary dominant,
 *     a tritone substitute or freely chromatic (how much each costs depends on the style and on SPICE);
 *   - the style's own vocabulary: which degrees it likes (bVII in 60s brit pop, the dorian IV in 80s funk,
 *     IVmaj9 and III7 in neo soul, #iv dim7 in gospel ...) and which chord colours (triads, add9, maj9, 13 ...);
 *   - how it follows the previous chord: root motion (falling fifths, steps, thirds, mediants), dominant and
 *     diminished resolution, sus resolution, tonal function (tonic -> predominant -> dominant), common tones,
 *     and the style's idioms (ii-V, IV-iv-I, bVI-bVII-I, IVmaj7-III7-vi ...);
 *   - when known, how well it leads into the next chord (inserting) or back to the first one (closing the loop);
 *   - complexity against the target set by style + SPICE.
 * The pool then samples distinct chord "groups" (root + family + bass) from the best with a temperature, so the
 * safest choice is usually there, next to some braver ones, and REROLL walks further down the list. */
#include "harmony.h"
#include "style_types.h"
#include "artists.h"
#include "refs.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* styles 12.. (4.0) are generated from tools/cc_data.py */
#include "gen/styles_gen.inc"
const char *const STYLE_NAMES[NUM_STYLES] = {
    "Pop", "60s Brit Pop", "70s Soul", "80s Pop Funk", "Neo Soul", "Gospel",
    "Jazz", "90s R&B", "House", "Lo-Fi", "Cinematic", "Rock", GEN_STYLE_NAMES};

/* family masks */
#define M  (1 << F_MAJ)
#define m  (1 << F_MIN)
#define D  (1 << F_DOM)
#define o  (1 << F_DIM)
#define h  (1 << F_HDIM)
#define s  (1 << F_SUS)
#define A  (1 << F_AUG)
#define MX (M | D | s)
#define ANY 0x7f



/* ---------------------------------------------------------------- 0 POP */
static const tok_t POP_T[] = {
    {0, 0, MX, 1.0f}, {0, 7, MX, .9f}, {0, 9, m, .9f}, {0, 5, MX, 1.0f}, {0, 2, m, .25f}, {0, 4, m, -.1f},
    {0, 10, M, .2f}, {0, 5, m, .1f},
    {1, 0, m, 1.0f}, {1, 8, M, 1.0f}, {1, 3, M, .9f}, {1, 10, MX, 1.0f}, {1, 5, m, .4f}, {1, 7, m, .2f},
    {1, 7, MX, .2f}, END_T};
static const idiom_t POP_I[] = {
    {0, 0, 7, MX, MX, .6f}, {0, 7, 9, MX, m, .8f}, {0, 9, 5, m, MX, .9f}, {0, 5, 0, MX, MX, .6f},
    {0, 5, 7, MX, MX, .5f}, {0, 0, 9, MX, m, .4f}, {0, 9, 7, m, MX, .3f}, {0, 0, 5, MX, MX, .4f},
    {1, 0, 8, m, M, .6f}, {1, 8, 3, M, M, .6f}, {1, 3, 10, M, MX, .7f}, {1, 10, 0, MX, m, .6f},
    {1, 0, 10, m, MX, .4f}, {1, 10, 8, MX, M, .5f}, END_I};
static const qpref_t POP_Q[] = {
    {Q_MAJ, 1.0f}, {Q_MIN, .9f}, {Q_ADD9, .5f}, {Q_SUS2, .5f}, {Q_SUS4, .35f}, {Q_M7, .4f}, {Q_MADD9, .3f},
    {Q_MAJ7, .3f}, {Q_7, .2f}, {Q_7SUS4, .2f}, END_Q};
static const tok_t POP_S[] = {{0, 0, MX, 2.0f}, {0, 9, m, 1.2f}, {0, 5, MX, 1.0f}, {1, 0, m, 2.0f}, {1, 8, M, 1.0f}, END_T};

/* ---------------------------------------------------------------- 1 60s BRIT POP */
static const tok_t BRIT_T[] = {
    {0, 0, M, 1.0f}, {0, 5, M, 1.0f}, {0, 7, MX, .8f}, {0, 9, m, .6f}, {0, 2, m, .4f}, {0, 4, m, .1f},
    {0, 10, M, .9f}, {0, 2, M | D, .6f}, {0, 4, M | D, .5f}, {0, 5, m, .7f}, {0, 8, M, .6f}, {0, 3, M, .4f},
    {0, 0, D, .4f}, {0, 9, M | D, .3f}, {0, 0, A, .3f},
    {1, 0, m, 1.0f}, {1, 10, M, .9f}, {1, 8, M, .8f}, {1, 3, M, .7f}, {1, 5, m, .6f}, {1, 7, MX, .6f},
    {1, 5, M, .4f}, END_T};
static const idiom_t BRIT_I[] = {
    {0, 0, 10, M, M, .8f}, {0, 10, 5, M, M, .9f}, {0, 5, 5, M, m, .9f}, {0, 5, 0, m, M, .9f},
    {0, 0, 2, M, M | D, .6f}, {0, 2, 5, M | D, M, .5f}, {0, 2, 7, M | D, MX, .5f}, {0, 8, 10, M, M, .7f},
    {0, 10, 0, M, M, .7f}, {0, 0, 4, M, M | D, .5f}, {0, 4, 9, M | D, m, .6f}, {0, 4, 5, M | D, M, .5f},
    {0, 9, 2, m, m, .4f}, {0, 0, 0, M, A, .4f}, {0, 0, 9, A, m, .5f}, {0, 7, 5, MX, M, .4f}, {0, 0, 9, M, m, .4f},
    {0, 9, 5, m, M, .4f}, {0, 5, 7, M, MX, .4f},
    {1, 0, 10, m, M, .6f}, {1, 10, 8, M, M, .6f}, {1, 8, 7, M, MX, .6f}, {1, 0, 5, m, M, .5f}, END_I};
static const qpref_t BRIT_Q[] = {
    {Q_MAJ, 1.0f}, {Q_MIN, 1.0f}, {Q_7, .7f}, {Q_M7, .5f}, {Q_MAJ7, .3f}, {Q_6, .4f}, {Q_AUG, .3f},
    {Q_M6, .45f}, {Q_SUS4, .2f}, {Q_9, .1f}, {Q_DIM, .1f}, END_Q};
static const tok_t BRIT_S[] = {{0, 0, M, 2.0f}, {0, 9, m, .6f}, {0, 5, M, .5f}, {0, 2, m, .3f}, {1, 0, m, 2.0f}, END_T};

/* ---------------------------------------------------------------- 2 70s SOUL */
static const tok_t SOUL_T[] = {
    {0, 0, M, 1.0f}, {0, 2, m, 1.0f}, {0, 7, D, .7f}, {0, 9, m, .7f}, {0, 4, m, .5f}, {0, 5, M, .9f},
    {0, 3, M, .5f}, {0, 8, M, .6f}, {0, 10, D | M, .6f}, {0, 2, D, .6f}, {0, 4, D, .6f}, {0, 9, D, .5f},
    {0, 1, D | M, .5f}, {0, 5, m, .5f}, {0, 6, h, .4f}, {0, 7, s, .5f},
    {1, 0, m, 1.0f}, {1, 5, m, .8f}, {1, 8, M, .7f}, {1, 10, D, .6f}, {1, 7, m, .4f}, {1, 7, D, .6f},
    {1, 3, M, .6f}, {1, 2, h, .6f}, {1, 1, M, .4f}, {1, 5, D, .5f}, END_T};
static const idiom_t SOUL_I[] = {
    {2, 2, 7, m | h, D | s, .8f}, {0, 7, 0, D | s, M, .6f}, {0, 4, 9, m, D, .6f}, {0, 9, 2, D, m, .7f},
    {0, 2, 7, D, D | s, .5f}, {0, 8, 10, M, D | M, .7f}, {0, 10, 0, D | M, M, .7f}, {0, 1, 0, D, M, .8f},
    {0, 0, 3, M, M, .5f}, {0, 0, 5, M, M, .4f}, {0, 5, 10, m, D, .6f}, {0, 0, 10, M, D, .4f},
    {0, 5, 4, M, m, .5f}, {0, 3, 2, M, m, .4f}, {0, 9, 2, m, m, .4f}, {0, 0, 4, M, m, .4f},
    {1, 0, 5, m, m, .6f}, {1, 5, 10, m, D, .6f}, {1, 10, 3, D, M, .6f}, {1, 8, 7, M, D, .5f},
    {1, 0, 5, m, D, .6f}, {1, 1, 0, M, m, .4f}, END_I};
static const qpref_t SOUL_Q[] = {
    {Q_MAJ7, 1.0f}, {Q_MAJ9, .9f}, {Q_69, .6f}, {Q_M7, .9f}, {Q_M9, .9f}, {Q_M11, .6f}, {Q_7, .5f},
    {Q_9, .7f}, {Q_13, .8f}, {Q_9SUS4, .8f}, {Q_7SUS4, .6f}, {Q_13SUS4, .5f}, {Q_7S9, .3f}, {Q_7B9, .4f},
    {Q_MAJ13, .4f}, {Q_MAJ7S11, .4f}, {Q_M7B5, .6f}, {Q_DIM7, .3f}, {Q_MAJ, .2f}, {Q_MIN, .2f}, {Q_M6, .4f},
    {Q_6, .3f}, {Q_ADD9, .3f}, END_Q};
static const tok_t SOUL_S[] = {
    {0, 0, M, 1.5f}, {0, 2, m, 1.2f}, {0, 5, M, 1.0f}, {0, 9, m, .8f}, {0, 8, M, .4f}, {1, 0, m, 1.8f},
    {1, 5, m, .6f}, END_T};

/* ---------------------------------------------------------------- 3 80s POP FUNK */
static const tok_t FUNK_T[] = {
    {1, 0, m, 1.2f}, {1, 5, M | D, .9f}, {1, 5, m, .6f}, {1, 10, M | D, .9f}, {1, 8, M, .8f}, {1, 3, M, .6f},
    {1, 7, m, .6f}, {1, 2, m, .4f}, {1, 7, D, .3f},
    {0, 0, M, 1.0f}, {0, 5, M, .8f}, {0, 2, m, .9f}, {0, 9, m, .9f}, {0, 4, m, .6f}, {0, 7, s, .6f},
    {0, 7, MX, .3f}, {0, 10, M, .6f}, END_T};
static const idiom_t FUNK_I[] = {
    {1, 0, 5, m, M | D, 1.0f}, {1, 5, 0, M | D, m, .9f}, {1, 0, 10, m, MX, .7f}, {1, 10, 0, MX, m, .7f},
    {1, 8, 10, M, MX, .8f}, {1, 0, 5, m, m, .5f}, {1, 5, 7, m, m, .5f}, {1, 7, 0, m, m, .5f},
    {1, 0, 3, m, M, .4f}, {1, 3, 5, M, M | D | m, .4f},
    {0, 2, 4, m, m, .6f}, {0, 4, 5, m, M, .5f}, {0, 5, 7, M, s | MX, .6f}, {0, 9, 2, m, m, .4f},
    {0, 2, 0, m, M, .4f}, {0, 9, 5, m, M, .4f}, END_I};
static const qpref_t FUNK_Q[] = {
    {Q_M7, 1.0f}, {Q_M9, .9f}, {Q_M11, .9f}, {Q_7, .5f}, {Q_9, .8f}, {Q_9SUS4, .7f}, {Q_MAJ7, .7f},
    {Q_MAJ9, .6f}, {Q_ADD9, .5f}, {Q_MIN, .4f}, {Q_MAJ, .4f}, {Q_13, .4f}, {Q_7SUS4, .5f}, {Q_SUS2, .4f},
    {Q_13SUS4, .3f}, END_Q};
static const tok_t FUNK_S[] = {{1, 0, m, 2.0f}, {0, 0, M, 1.2f}, {0, 9, m, .9f}, {0, 2, m, .8f}, {0, 5, M, .8f}, END_T};

/* ---------------------------------------------------------------- 4 NEO SOUL */
static const tok_t NEO_T[] = {
    {0, 5, M, 1.1f}, {0, 4, m, .9f}, {0, 9, m, .9f}, {0, 2, m, .9f}, {0, 0, M, .6f}, {0, 4, D, .9f},
    {0, 7, m, .6f}, {0, 0, D, .6f}, {0, 10, D | M, .5f}, {0, 3, M, .4f}, {0, 5, m, .5f}, {0, 8, M, .4f},
    {1, 0, m, 1.0f}, {1, 5, m, .9f}, {1, 8, M, .8f}, {1, 3, M, .6f}, {1, 2, h, .6f}, {1, 7, D, .6f},
    {1, 10, D, .6f}, {1, 7, m, .5f}, {1, 1, M, .4f}, END_T};
static const idiom_t NEO_I[] = {
    {0, 5, 4, M, m, .9f}, {0, 4, 9, m, m, .8f}, {0, 5, 4, M, D, .9f}, {0, 4, 9, D, m, .9f}, {0, 9, 7, m, m, .7f},
    {0, 7, 0, m, D, .9f}, {0, 0, 5, D, M, .8f}, {0, 2, 4, m, m, .5f}, {0, 9, 2, m, m, .6f}, {0, 2, 7, m, D, .6f},
    {0, 5, 5, M, m, .7f}, {0, 5, 0, m, M, .5f},
    {1, 0, 5, m, m, .7f}, {1, 5, 10, m, D, .6f}, {1, 8, 7, M, D, .6f}, {1, 2, 7, h, D, .6f}, END_I};
static const qpref_t NEO_Q[] = {
    {Q_MAJ9, 1.0f}, {Q_MAJ7, .8f}, {Q_M9, 1.0f}, {Q_M11, .9f}, {Q_M7, .7f}, {Q_9, .6f}, {Q_13, .7f},
    {Q_7S9, .7f}, {Q_69, .6f}, {Q_MAJ13, .5f}, {Q_MAJ7S11, .6f}, {Q_9SUS4, .6f}, {Q_M7B5, .5f}, {Q_7B9, .4f},
    {Q_ADD9, .3f}, {Q_7, .3f}, END_Q};
static const tok_t NEO_S[] = {
    {0, 5, M, 1.4f}, {0, 2, m, 1.0f}, {0, 0, M, 1.0f}, {0, 9, m, .9f}, {0, 4, m, .6f}, {1, 0, m, 1.6f},
    {1, 5, m, .8f}, END_T};

/* ---------------------------------------------------------------- 5 GOSPEL */
static const tok_t GOSP_T[] = {
    {0, 0, M, 1.0f}, {0, 5, M, 1.0f}, {0, 7, D, .7f}, {0, 2, m, .7f}, {0, 9, m, .7f}, {0, 4, m, .6f},
    {0, 4, D, .7f}, {0, 9, D, .8f}, {0, 2, D, .6f}, {0, 6, o | h, .8f}, {0, 5, m, .6f}, {0, 0, D, .6f},
    {0, 10, D, .4f}, {0, 7, s, .7f},
    {1, 0, m, 1.0f}, {1, 5, m, .9f}, {1, 7, D, .8f}, {1, 2, h, .7f}, {1, 8, M, .6f}, {1, 3, M, .5f},
    {1, 11, o, .5f}, END_T};
static const idiom_t GOSP_I[] = {
    {0, 0, 0, M, D, .5f}, {0, 0, 5, D, M, 1.0f}, {0, 5, 6, M, o | h, 1.0f}, {0, 6, 0, o | h, M, .9f},
    {0, 4, 9, m | D, D, .8f}, {0, 9, 2, D, m | D, .9f}, {0, 2, 7, m | D, D | s, .9f}, {0, 7, 0, D | s, M, .8f},
    {0, 5, 5, M, m, .6f}, {0, 5, 0, m, M, .6f}, {0, 5, 7, M, D | s, .5f}, {0, 9, 2, m, D, .5f},
    {1, 2, 7, h, D, .9f}, {1, 7, 0, D, m, .8f}, {1, 0, 5, m, m, .6f}, END_I};
static const qpref_t GOSP_Q[] = {
    {Q_MAJ7, .6f}, {Q_69, .7f}, {Q_7, .8f}, {Q_9, .8f}, {Q_13, 1.0f}, {Q_7S9, .6f}, {Q_7B9, .7f}, {Q_M7, .8f},
    {Q_M9, .7f}, {Q_DIM7, .9f}, {Q_9SUS4, .8f}, {Q_13SUS4, .6f}, {Q_MAJ9, .6f}, {Q_MAJ, .4f}, {Q_M11, .5f},
    {Q_6, .5f}, {Q_M7B5, .6f}, {Q_ADD9, .3f}, END_Q};
static const tok_t GOSP_S[] = {{0, 0, M, 1.5f}, {0, 5, M, 1.0f}, {0, 9, m, .6f}, {1, 0, m, 1.6f}, END_T};

/* ---------------------------------------------------------------- 6 JAZZ */
static const tok_t JAZZ_T[] = {
    {0, 0, M, .8f}, {0, 2, m, 1.1f}, {0, 7, D, 1.0f}, {0, 9, m, .7f}, {0, 4, m, .6f}, {0, 5, M, .6f},
    {0, 9, D, .8f}, {0, 2, D, .7f}, {0, 4, D, .6f}, {0, 1, D, .7f}, {0, 5, m, .4f}, {0, 10, D, .5f},
    {0, 6, h, .5f},
    {1, 0, m, .8f}, {1, 2, h, 1.1f}, {1, 7, D, 1.0f}, {1, 5, m, .6f}, {1, 8, M, .6f}, {1, 1, D, .5f},
    {1, 3, M, .5f}, END_T};
static const idiom_t JAZZ_I[] = {
    {2, 2, 7, m | h, D, 1.2f}, {0, 7, 0, D, M, 1.0f}, {0, 4, 9, m, D, .7f}, {0, 9, 2, D, m, .8f},
    {0, 2, 1, m, D, .8f}, {0, 1, 0, D, M, 1.0f}, {0, 5, 5, M, m, .5f}, {0, 5, 10, m, D, .7f},
    {0, 10, 0, D, M, .6f}, {0, 0, 9, M, D, .6f}, {1, 7, 0, D, m, 1.0f}, END_I};
static const qpref_t JAZZ_Q[] = {
    {Q_MAJ7, 1.0f}, {Q_69, .8f}, {Q_MAJ9, .7f}, {Q_M7, 1.0f}, {Q_M9, .7f}, {Q_7, .8f}, {Q_9, .7f}, {Q_13, .8f},
    {Q_7B9, .7f}, {Q_7S9, .5f}, {Q_7S5, .4f}, {Q_M7B5, .9f}, {Q_DIM7, .5f}, {Q_MAJ, -.4f}, {Q_MIN, -.4f},
    {Q_MMAJ7, .4f}, {Q_M6, .5f}, {Q_MAJ7S11, .4f}, {Q_6, .4f}, END_Q};
static const tok_t JAZZ_S[] = {{0, 0, M, 1.2f}, {0, 2, m, 1.3f}, {0, 4, m, .4f}, {0, 9, m, .5f}, {1, 0, m, 1.2f}, {1, 2, h, 1.0f}, END_T};

/* ---------------------------------------------------------------- 7 90s R&B */
static const tok_t RNB_T[] = {
    {0, 0, M, .7f}, {0, 5, M, 1.0f}, {0, 9, m, .9f}, {0, 2, m, .9f}, {0, 4, m, .8f}, {0, 7, s, .8f},
    {0, 7, D, .4f}, {0, 4, D, .5f}, {0, 10, M, .4f}, {0, 5, m, .4f},
    {1, 0, m, 1.0f}, {1, 5, m, .8f}, {1, 8, M, .9f}, {1, 10, MX, .7f}, {1, 7, m, .5f}, {1, 3, M, .6f},
    {1, 2, h, .5f}, {1, 7, D, .5f}, END_T};
static const idiom_t RNB_I[] = {
    {0, 5, 4, M, m, .7f}, {0, 4, 9, m, m, .7f}, {0, 9, 2, m, m, .6f}, {0, 2, 7, m, s | D, .6f},
    {0, 5, 7, M, MX, .5f}, {0, 2, 4, m, m, .6f}, {0, 4, 5, m, M, .5f}, {0, 0, 4, M, m, .4f},
    {1, 0, 5, m, m, .6f}, {1, 5, 10, m, MX, .5f}, {1, 8, 10, M, MX, .6f}, {1, 8, 7, M, m, .5f}, END_I};
static const qpref_t RNB_Q[] = {
    {Q_MAJ7, .9f}, {Q_MAJ9, .9f}, {Q_M7, .9f}, {Q_M9, .9f}, {Q_M11, .6f}, {Q_ADD9, .6f}, {Q_9SUS4, .8f},
    {Q_7SUS4, .6f}, {Q_13SUS4, .5f}, {Q_9, .5f}, {Q_69, .4f}, {Q_MADD9, .5f}, {Q_13, .4f}, {Q_SUS2, .3f}, END_Q};
static const tok_t RNB_S[] = {{0, 5, M, 1.0f}, {0, 0, M, 1.0f}, {0, 9, m, .9f}, {0, 2, m, .8f}, {1, 0, m, 1.8f}, {1, 8, M, .6f}, END_T};

/* ---------------------------------------------------------------- 8 HOUSE */
static const tok_t HOUSE_T[] = {
    {1, 0, m, 1.2f}, {1, 8, M, 1.0f}, {1, 3, M, .8f}, {1, 10, MX, .9f}, {1, 5, m, .9f}, {1, 7, m, .6f},
    {1, 2, h, .2f},
    {0, 9, m, 1.0f}, {0, 5, M, 1.0f}, {0, 0, M, .8f}, {0, 7, MX, .6f}, {0, 2, m, .7f}, {0, 4, m, .5f}, END_T};
static const idiom_t HOUSE_I[] = {
    {1, 0, 8, m, M, .7f}, {1, 8, 3, M, M, .5f}, {1, 3, 10, M, MX, .6f}, {1, 10, 0, MX, m, .6f},
    {1, 0, 5, m, m, .7f}, {1, 5, 0, m, m, .5f}, {1, 0, 7, m, m, .4f}, {1, 0, 10, m, MX, .5f},
    {0, 9, 5, m, M, .7f}, {0, 5, 0, M, M, .5f}, {0, 0, 7, M, MX, .5f}, {0, 2, 9, m, m, .5f}, END_I};
static const qpref_t HOUSE_Q[] = {
    {Q_M7, .9f}, {Q_M9, 1.0f}, {Q_M11, .8f}, {Q_MADD9, .5f}, {Q_MAJ7, .7f}, {Q_MAJ9, .6f}, {Q_69, .5f},
    {Q_ADD9, .5f}, {Q_9, .3f}, {Q_7SUS4, .4f}, {Q_MIN, .5f}, {Q_MAJ, .4f}, {Q_SUS2, .4f}, END_Q};
static const tok_t HOUSE_S[] = {{1, 0, m, 2.0f}, {1, 5, m, .5f}, {0, 9, m, .9f}, {0, 5, M, .6f}, {0, 0, M, .6f}, END_T};

/* ---------------------------------------------------------------- 9 LO-FI */
static const tok_t LOFI_T[] = {
    {0, 5, M, 1.0f}, {0, 4, m, .8f}, {0, 2, m, 1.0f}, {0, 0, M, .8f}, {0, 9, m, .8f}, {0, 4, D, .6f},
    {0, 7, D, .6f}, {0, 5, m, .5f}, {0, 10, D, .5f},
    {1, 0, m, .9f}, {1, 5, m, .9f}, {1, 8, M, .8f}, {1, 2, h, .7f}, {1, 7, D, .7f}, {1, 3, M, .6f},
    {1, 10, MX, .6f}, END_T};
static const idiom_t LOFI_I[] = {
    {0, 5, 4, M, m, .8f}, {0, 4, 2, m, m, .6f}, {0, 2, 0, m, M, .5f}, {2, 2, 7, m | h, D, .7f},
    {0, 7, 0, D, M, .6f}, {0, 5, 5, M, m, .6f}, {0, 4, 9, m, D, .5f}, {0, 9, 2, m, m, .5f}, {0, 0, 9, M, m, .4f},
    {1, 7, 0, D, m, .6f}, {1, 0, 5, m, m, .5f}, END_I};
static const qpref_t LOFI_Q[] = {
    {Q_MAJ7, 1.0f}, {Q_MAJ9, 1.0f}, {Q_M7, 1.0f}, {Q_M9, .9f}, {Q_M11, .5f}, {Q_9, .6f}, {Q_13, .5f},
    {Q_7S9, .5f}, {Q_7B9, .4f}, {Q_69, .5f}, {Q_MAJ7S11, .3f}, {Q_M7B5, .4f}, {Q_MAJ, -.3f}, {Q_MIN, -.3f}, END_Q};
static const tok_t LOFI_S[] = {{0, 5, M, 1.2f}, {0, 2, m, 1.0f}, {0, 0, M, 1.0f}, {0, 9, m, .7f}, {1, 0, m, 1.5f}, {1, 5, m, .8f}, END_T};

/* ---------------------------------------------------------------- 10 CINEMATIC */
static const tok_t CINE_T[] = {
    {1, 0, m, 1.2f}, {1, 8, M, 1.1f}, {1, 3, M, .9f}, {1, 10, M, .9f}, {1, 5, m, .8f}, {1, 7, m, .3f},
    {1, 7, M, .5f}, {1, 1, M, .4f}, {1, 5, M, .3f},
    {0, 0, M, 1.0f}, {0, 5, M, .8f}, {0, 9, m, .8f}, {0, 8, M, .7f}, {0, 10, M, .6f}, {0, 4, m, .4f},
    {0, 7, M, .6f}, {0, 5, m, .6f}, {0, 3, M, .4f}, END_T};
static const idiom_t CINE_I[] = {
    {1, 0, 8, m, M, .8f}, {1, 8, 3, M, M, .7f}, {1, 3, 10, M, M, .8f}, {1, 10, 0, M, m, .6f}, {1, 0, 10, m, M, .5f},
    {1, 8, 5, M, m, .5f}, {1, 5, 0, m, m, .5f}, {1, 0, 7, m, M, .3f}, {1, 8, 10, M, M, .7f},
    {0, 0, 8, M, M, .6f}, {0, 0, 4, M, M, .4f}, {0, 8, 0, M, M, .5f}, {0, 5, 5, M, m, .5f}, END_I};
static const qpref_t CINE_Q[] = {
    {Q_MAJ, .9f}, {Q_MIN, .9f}, {Q_SUS2, .8f}, {Q_SUS4, .6f}, {Q_ADD9, .7f}, {Q_MADD9, .7f}, {Q_MAJ7, .3f},
    {Q_M7, .3f}, {Q_69, .3f}, {Q_MMAJ7, .3f}, END_Q};
static const tok_t CINE_S[] = {{1, 0, m, 2.0f}, {1, 8, M, .7f}, {0, 0, M, 1.5f}, {0, 9, m, .9f}, END_T};

/* ---------------------------------------------------------------- 11 ROCK */
static const tok_t ROCK_T[] = {
    {0, 0, M, 1.1f}, {0, 5, M, 1.1f}, {0, 7, M, .9f}, {0, 10, M, 1.0f}, {0, 9, m, .6f}, {0, 8, M, .6f},
    {0, 3, M, .5f}, {0, 2, m, .2f},
    {1, 0, m, 1.0f}, {1, 10, M, 1.0f}, {1, 8, M, .9f}, {1, 5, m, .7f}, {1, 3, M, .8f}, {1, 5, M, .4f},
    {1, 7, m, .4f}, END_T};
static const idiom_t ROCK_I[] = {
    {0, 0, 10, M, M, .8f}, {0, 10, 5, M, M, .9f}, {0, 5, 0, M, M, .7f}, {0, 0, 5, M, M, .5f}, {0, 7, 5, M, M, .5f},
    {0, 8, 10, M, M, .8f}, {0, 10, 0, M, M, .8f}, {0, 3, 5, M, M, .5f},
    {1, 0, 10, m, M, .7f}, {1, 10, 8, M, M, .7f}, {1, 8, 10, M, M, .5f}, {1, 5, 0, m, m, .4f}, END_I};
static const qpref_t ROCK_Q[] = {
    {Q_MAJ, 1.0f}, {Q_MIN, .9f}, {Q_SUS4, .5f}, {Q_SUS2, .5f}, {Q_ADD9, .4f}, {Q_7, .3f}, {Q_M7, .3f},
    {Q_7SUS4, .2f}, END_Q};
static const tok_t ROCK_S[] = {{0, 0, M, 2.0f}, {1, 0, m, 2.0f}, {0, 9, m, .5f}, END_T};

static const style_t STYLES[NUM_STYLES] = {
    /* base_cx borrow secdom chroma vamp slash sus fifths steps thirds mediant retro */
    {0.6f, .3f, .2f, .05f, .5f, .25f, .6f, .8f, 1.0f, 1.0f, .2f, .6f, POP_T, POP_I, POP_Q, POP_S},
    {0.5f, .9f, .7f, .3f, .3f, .35f, .2f, 1.0f, 1.0f, .9f, .7f, .5f, BRIT_T, BRIT_I, BRIT_Q, BRIT_S},
    {2.0f, .8f, .9f, .9f, .4f, .35f, .7f, 1.3f, .9f, .7f, .8f, .3f, SOUL_T, SOUL_I, SOUL_Q, SOUL_S},
    {1.4f, .6f, .4f, .4f, .9f, .5f, .6f, .8f, 1.2f, .6f, .4f, .6f, FUNK_T, FUNK_I, FUNK_Q, FUNK_S},
    {2.5f, .8f, .8f, 1.0f, .5f, .3f, .5f, 1.2f, .8f, .9f, .6f, .4f, NEO_T, NEO_I, NEO_Q, NEO_S},
    {2.0f, .7f, 1.0f, .7f, .3f, .7f, .6f, 1.6f, .9f, .6f, .3f, .3f, GOSP_T, GOSP_I, GOSP_Q, GOSP_S},
    {2.2f, .7f, 1.0f, 1.0f, .2f, .1f, .3f, 2.0f, .6f, .5f, .4f, .1f, JAZZ_T, JAZZ_I, JAZZ_Q, JAZZ_S},
    {1.7f, .6f, .6f, .5f, .7f, .5f, .7f, 1.0f, 1.0f, .9f, .4f, .5f, RNB_T, RNB_I, RNB_Q, RNB_S},
    {1.6f, .5f, .2f, .3f, 1.0f, .3f, .5f, .7f, 1.1f, .9f, .3f, .7f, HOUSE_T, HOUSE_I, HOUSE_Q, HOUSE_S},
    {2.3f, .6f, .7f, .7f, .6f, .2f, .4f, 1.4f, .9f, .8f, .4f, .3f, LOFI_T, LOFI_I, LOFI_Q, LOFI_S},
    {0.8f, .9f, .3f, .5f, .4f, .3f, .8f, .6f, 1.0f, 1.0f, 1.2f, .6f, CINE_T, CINE_I, CINE_Q, CINE_S},
    {0.2f, .9f, .3f, .1f, .6f, .2f, .6f, .8f, 1.2f, .5f, .4f, .9f, ROCK_T, ROCK_I, ROCK_Q, ROCK_S},
    GEN_STYLE_ROWS
};

#undef M
#undef m
#undef D
#undef o
#undef h
#undef s
#undef A

/* ------------------------------------------------------------------------------------------------ helpers */
uint32_t rng_next(uint32_t *st) {   /* xorshift32 */
    uint32_t x = *st ? *st : 0x9e3779b9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return *st = x;
}
float rng_unit(uint32_t *st) { return (rng_next(st) >> 8) * (1.0f / 16777216.0f); }

static int fam_of(const chord_t *c) { return QUALITIES[c->q].fam; }
static int fbit(const chord_t *c) { return 1 << fam_of(c); }
static int frame_of(int scale) { return scale_is_minor(scale) ? 1 : 0; }

/* the diatonic triad family on a scale degree root (F_MAJ, F_MIN, F_DIM, F_AUG), -1 if pc not a scale root */
static int diatonic_triad_fam(int key, int scale, int pc) {
    int mask = scale_mask(key, scale);
    if (!((mask >> pc) & 1)) return -1;
    int third = -1, fifth = -1;
    for (int i = 3; i <= 4; i++) if ((mask >> ((pc + i) % 12)) & 1) { third = i; break; }
    for (int i = 6; i <= 8; i++) if ((mask >> ((pc + i) % 12)) & 1) { if (fifth < 0 || i == 7) fifth = i; }
    if (third == 4 && fifth == 7) return F_MAJ;
    if (third == 3 && fifth == 7) return F_MIN;
    if (third == 3 && fifth == 6) return F_DIM;
    if (third == 4 && fifth == 8) return F_AUG;
    return F_MIN;
}

static int parallel_masks(int key, int scale, int *out) {
    int n = 0;
    if (frame_of(scale) == 0) {
        static const int P[] = {1, 2, 3, 6, 4};             /* aeolian, dorian, mixolydian, harmonic minor, lydian */
        for (int i = 0; i < 5; i++) out[n++] = scale_mask(key, P[i]);
    } else {
        static const int P[] = {0, 6, 7, 2, 5, 3};          /* ionian, harmonic, melodic, dorian, phrygian, mixo */
        for (int i = 0; i < 6; i++) out[n++] = scale_mask(key, P[i]);
    }
    return n;
}

int harmony_class(int key, int scale, const chord_t *c) {
    int pcs = chord_pcs(c), mask = scale_mask(key, scale);
    if ((pcs & ~mask) == 0) return CLS_DIATONIC;
    int f = fam_of(c);
    int domish = f == F_DOM || (f == F_MAJ && c->q != Q_MAJ7 && c->q != Q_MAJ9 && c->q != Q_MAJ13 && c->q != Q_MAJ7S11);
    if (domish) {
        int t = (c->root + 5) % 12, tf = diatonic_triad_fam(key, scale, t);
        if (tf >= 0 && tf != F_DIM && t != key) return CLS_SECONDARY;
    }
    if (f == F_DIM) {   /* leading-tone diminished of a diatonic chord */
        int t = (c->root + 1) % 12, tf = diatonic_triad_fam(key, scale, t);
        if (tf >= 0 && tf != F_DIM) return CLS_SECONDARY;
    }
    int pm[8], np = parallel_masks(key, scale, pm);
    for (int i = 0; i < np; i++) if ((pcs & ~pm[i]) == 0) return CLS_BORROWED;
    if (f == F_DOM) {
        int t = (c->root + 11) % 12, tf = diatonic_triad_fam(key, scale, t);
        if (tf >= 0 && tf != F_DIM) return CLS_SUBV;
    }
    return CLS_CHROMATIC;
}

static void roman_of(int key, int pc, int lower, char *buf, int len) {
    static const char *const R[12] = {"I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII"};
    const char *r = R[((pc - key) % 12 + 12) % 12];
    int n = 0;
    for (const char *p = r; *p && n < len - 1; p++) {
        char ch = *p;
        if (lower && ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        buf[n++] = ch;
    }
    buf[n] = 0;
}

void harmony_roman(int key, int scale, const chord_t *c, char *buf, int len) {
    int f = fam_of(c), cls = harmony_class(key, scale, c);
    char r[16], t[16];
    if (cls == CLS_SECONDARY) {
        int target = f == F_DIM ? (c->root + 1) % 12 : (c->root + 5) % 12;
        int tf = diatonic_triad_fam(key, scale, target);
        roman_of(key, target, tf == F_MIN || tf == F_DIM, t, sizeof t);
        snprintf(buf, len, "%s/%s", f == F_DIM ? "vii" : "V", t);
        return;
    }
    if (cls == CLS_SUBV) {
        int target = (c->root + 11) % 12;
        int tf = diatonic_triad_fam(key, scale, target);
        roman_of(key, target, tf == F_MIN || tf == F_DIM, t, sizeof t);
        snprintf(buf, len, "subV/%s", t);
        return;
    }
    roman_of(key, c->root, f == F_MIN || f == F_DIM || f == F_HDIM, r, sizeof r);
    const char *suf = f == F_DIM ? "o" : f == F_HDIM ? "o7" : f == F_AUG ? "+" : f == F_SUS ? "sus" : "";
    if (f == F_HDIM) suf = "m7b5";
    snprintf(buf, len, "%s%s", r, suf);
}

/* tonal function of a diatonic chord: 0 tonic, 1 predominant, 2 dominant, -1 other */
static int tonal_function(int key, int scale, const chord_t *c) {
    if (harmony_class(key, scale, c) != CLS_DIATONIC && !(fam_of(c) == F_DOM && (c->root - key + 12) % 12 == 7)) return -1;
    int deg = (c->root - key + 12) % 12;
    if (frame_of(scale) == 0) {
        switch (deg) { case 0: case 9: case 4: return 0; case 2: case 5: return 1; case 7: case 11: return 2; }
    } else {
        switch (deg) { case 0: case 3: case 8: return 0; case 2: case 5: return 1; case 7: case 10: case 11: return 2; }
    }
    return -1;
}

/* the chosen style with the artist profile laid over it */
#define NLAYER 3   /* the style, an artist, a producer */
typedef struct { style_t num; const style_t *layer[NLAYER]; float w[NLAYER]; const ref_t *ref[NLAYER]; } eff_t;

static float clamp01f(float x) { return x < 0 ? 0 : x > 1.5f ? 1.5f : x; }
static float clampm(float x) { return x < 0.1f ? 0.1f : x > 2.5f ? 2.5f : x; }   /* root-motion weights */

static void add_overlay(style_t *n, const style_t *ar) {
    n->base_cx += ar->base_cx;
    if (n->base_cx < 0) n->base_cx = 0;
    if (n->base_cx > 3) n->base_cx = 3;
    n->borrow = clamp01f(n->borrow + ar->borrow);
    n->secdom = clamp01f(n->secdom + ar->secdom);
    n->chroma = clamp01f(n->chroma + ar->chroma);
    n->vamp = clamp01f(n->vamp + ar->vamp);
    n->slash = clamp01f(n->slash + ar->slash);
    n->sus = clamp01f(n->sus + ar->sus);
    n->fifths = clampm(n->fifths + ar->fifths);
    n->steps = clampm(n->steps + ar->steps);
    n->thirds = clampm(n->thirds + ar->thirds);
    n->mediant = clamp01f(n->mediant + ar->mediant);
    n->retro = clamp01f(n->retro + ar->retro);
}

/* the neutral middle the STYLE influence moves away from: at 0 % a style keeps only its key frame */
static const style_t NEUTRAL = {1.2f, .35f, .35f, .25f, .3f, .3f, .35f, 1.0f, 1.0f, 1.0f, .3f, .3f, NULL, NULL, NULL, NULL};

/* artist and producer are profile indices (artists.h, 0 = none); w: STYLE / ARTIST / PRODUCER weights (1 = 50 %).
 * With both profiles, each pulls a little less on the numbers. */
static void eff_style_w(int style, int artist, int producer, const float *w, eff_t *E) {
    const style_t *st = &STYLES[style < 0 || style >= NUM_STYLES ? 0 : style];
    float ws = w ? w[0] : 1.0f;
    E->num = *st;
    if (ws != 1.0f) {   /* scale the style's numbers around the neutral middle */
        style_t *n = &E->num;
        n->base_cx = NEUTRAL.base_cx + ws * (st->base_cx - NEUTRAL.base_cx);
        if (n->base_cx < 0) n->base_cx = 0;
        if (n->base_cx > 3) n->base_cx = 3;
        n->borrow = clamp01f(NEUTRAL.borrow + ws * (st->borrow - NEUTRAL.borrow));
        n->secdom = clamp01f(NEUTRAL.secdom + ws * (st->secdom - NEUTRAL.secdom));
        n->chroma = clamp01f(NEUTRAL.chroma + ws * (st->chroma - NEUTRAL.chroma));
        n->vamp = clamp01f(NEUTRAL.vamp + ws * (st->vamp - NEUTRAL.vamp));
        n->slash = clamp01f(NEUTRAL.slash + ws * (st->slash - NEUTRAL.slash));
        n->sus = clamp01f(NEUTRAL.sus + ws * (st->sus - NEUTRAL.sus));
        n->fifths = clampm(NEUTRAL.fifths + ws * (st->fifths - NEUTRAL.fifths));
        n->steps = clampm(NEUTRAL.steps + ws * (st->steps - NEUTRAL.steps));
        n->thirds = clampm(NEUTRAL.thirds + ws * (st->thirds - NEUTRAL.thirds));
        n->mediant = clamp01f(NEUTRAL.mediant + ws * (st->mediant - NEUTRAL.mediant));
        n->retro = clamp01f(NEUTRAL.retro + ws * (st->retro - NEUTRAL.retro));
    }
    E->layer[0] = st;
    E->layer[1] = artist > 0 && artist < NUM_ARTISTS ? &ARTISTS[artist] : NULL;
    E->layer[2] = producer > 0 && producer < NUM_ARTISTS && producer != artist ? &ARTISTS[producer] : NULL;
    E->w[0] = ws;
    E->w[1] = w ? w[1] : 1.0f;
    E->w[2] = w ? w[2] : 1.0f;
    E->ref[0] = NULL;
    E->ref[1] = E->layer[1] ? ref_of(artist) : NULL;
    E->ref[2] = E->layer[2] ? ref_of(producer) : NULL;
    int both = E->layer[1] && E->layer[2];
    for (int l = 1; l < NLAYER; l++) {
        if (!E->layer[l]) continue;
        style_t d = *E->layer[l];
        const float k = (both ? 0.6f : 1.0f) * E->w[l];
        d.base_cx *= k; d.borrow *= k; d.secdom *= k; d.chroma *= k; d.vamp *= k; d.slash *= k;
        d.sus *= k; d.fifths *= k; d.steps *= k; d.thirds *= k; d.mediant *= k; d.retro *= k;
        add_overlay(&E->num, &d);
    }
}

static void eff_of(const hctx_t *hc, eff_t *E) {
    float w[3] = {hc->wstyle, hc->wartist, hc->wprod};
    eff_style_w(hc->style, hc->artist, hc->producer, hc->mixed ? w : NULL, E);
}

static float tok_weight(const tok_t *t, int frame, int deg, int fb) {
    float w = 0;
    for (; t && t->frame >= 0; t++)
        if ((t->frame == frame || t->frame == 2) && t->deg == deg && (t->fam & fb)) w += t->w;
    return w;
}

static float idiom_weight(const eff_t *E, int frame, int key, const chord_t *p, const chord_t *c) {
    int fd = (p->root - key + 12) % 12, td = (c->root - key + 12) % 12, fb = fbit(p), tb = fbit(c);
    float w = 0;
    for (int l = 0; l < NLAYER; l++) {
        if (!E->layer[l]) continue;
        float lw = (l ? 1.2f : 1.0f) * E->w[l];   /* the profiles' own moves weigh a little more */
        for (const idiom_t *i = E->layer[l]->idioms; i && i->frame >= 0; i++)
            if ((i->frame == frame || i->frame == 2) && i->from == fd && i->to == td && (i->ffam & fb) && (i->tfam & tb))
                w += lw * i->w;
        /* the documented moves of the profile's reference track (refs.c) */
        const ref_t *r = E->ref[l];
        if (r && r->n > 1 && r->frame == frame)
            for (int k = 0; k < r->n; k++) {
                int k2 = (k + 1) % r->n;
                if (r->deg[k] == fd && r->deg[k2] == td && (r->fam[k] & fb) && (r->fam[k2] & tb) && r->deg[k] != r->deg[k2])
                    w += lw * 1.4f;
            }
    }
    return w;
}

static float qpref(const eff_t *E, int q) {
    float w = -0.35f;   /* a colour the style doesn't use */
    for (const qpref_t *p = E->layer[0]->qp; p && p->q >= 0; p++) if (p->q == q) { w = p->w; break; }
    w *= E->w[0];
    /* the profiles pull the colour towards their palette: their average opinion, at their strongest weight */
    float S = 0, K = 0, kmax = 0;
    for (int l = 1; l < NLAYER; l++) {
        if (!E->layer[l] || !E->layer[l]->qp) continue;
        float k = E->w[l], v = -0.25f / 1.1f;
        for (const qpref_t *p = E->layer[l]->qp; p->q >= 0; p++) if (p->q == q) { v = p->w; break; }
        S += k * 1.1f * v;
        K += k;
        if (k > kmax) kmax = k;
    }
    if (K > 0) {
        float a = K < 1 ? K : 1;
        w = w * (1.0f - 0.6f * a) + (S / K) * kmax;
    }
    return w;
}

static float tok_weight2(const eff_t *E, int starts, int frame, int deg, int fb) {
    float w = 0;
    for (int l = 0; l < NLAYER; l++) {
        if (!E->layer[l]) continue;
        w += (l ? 1.2f : 1.0f) * E->w[l] * tok_weight(starts ? E->layer[l]->starts : E->layer[l]->toks, frame, deg, fb);
        const ref_t *r = E->ref[l];   /* the reference track's chords are part of the profile's vocabulary */
        if (!starts && r && r->frame == frame)
            for (int k = 0; k < r->n; k++)
                if (r->deg[k] == deg && (r->fam[k] & fb)) { w += 1.2f * E->w[l] * 0.4f; break; }
    }
    return w;
}

static int bass_pc(const chord_t *c) { return c->bass >= 0 ? c->bass : c->root; }

/* how well c follows p, key- and style-independent part plus the style's root-motion taste */
static float transition(const style_t *st, const chord_t *p, const chord_t *c, float spice) {
    if (chord_equal(p, c)) return -8.0f;
    int d = (c->root - p->root + 12) % 12, pf = fam_of(p), cf = fam_of(c);
    const quality_t *pq = &QUALITIES[p->q], *cq = &QUALITIES[c->q];
    float t = 0;
    switch (d) {
    case 5:
        t += 1.4f * st->fifths;
        if (pf == F_DOM) t += 2.2f;
        if (pf == F_HDIM && cf == F_DOM) t += 1.8f;
        if (pf == F_MIN && cf == F_DOM) t += 1.0f;
        break;
    case 7: t += 0.8f; if (pf == F_DOM) t -= 0.8f; break;
    case 2: t += 1.0f * st->steps; if (pf == F_DOM) t -= 0.4f; break;
    case 10: t += 0.9f * st->steps; break;
    case 9: t += 1.1f * st->thirds; break;
    case 3: t += 0.7f * st->thirds; break;
    case 4: t += 0.3f + 0.8f * st->mediant; break;
    case 8: t += 0.5f + 0.8f * st->mediant; break;
    case 1:
        t += 0.1f;
        if (pf == F_DIM) t += 2.5f;
        if (pf == F_DOM) t -= 0.4f;
        break;
    case 11:
        t += 0.2f;
        if (pf == F_DOM) t += 0.5f + 2.0f * st->chroma;   /* subV resolving down a semitone */
        break;
    case 6: t -= 0.6f; break;
    case 0:
        if (pq->sus && !cq->sus && (cf == F_DOM || cf == F_MAJ)) t += 2.2f;
        else if (pf == cf || (pf == F_MAJ && cf == F_DOM) || (pf == F_MIN && cf == F_MIN)) t += 0.2f;
        else t -= 0.8f;
        if (pf == F_MAJ && cf == F_DOM) t += 0.4f;    /* I -> I7 (on to IV) */
        break;
    }
    /* chromatic planing: the same chord type sliding by a step or semitone (soul, neo soul) */
    if (p->q == c->q && (d == 1 || d == 11 || d == 2 || d == 10) && QUALITIES[c->q].cx >= 1) t += 0.8f * st->chroma;
    /* sus chord wants resolution, not another sus */
    if (pq->sus && cq->sus) t -= 0.6f;
    /* shared tones keep the line smooth */
    int ct = popcount12(chord_pcs(p) & chord_pcs(c));
    t += 0.16f * (ct > 4 ? 4 : ct);
    /* bass line: stepwise bass motion with slash chords */
    if (c->bass >= 0 || p->bass >= 0) {
        int bd = (bass_pc(c) - bass_pc(p) + 12) % 12;
        if (bd == 1 || bd == 2 || bd == 10 || bd == 11) t += 0.9f * (0.4f + st->slash);
        else if (bd == 0) t += 0.3f;
    }
    (void)spice;
    return t;
}

static float func_bonus(const style_t *st, int key, int scale, const chord_t *p, const chord_t *c) {
    int a = tonal_function(key, scale, p), b = tonal_function(key, scale, c);
    if (a < 0 || b < 0) return 0;
    static const float FM[3][3] = {{-0.1f, 0.5f, 0.3f}, {0.3f, 0.1f, 0.7f}, {0.9f, -0.3f, 0.0f}};
    float f = FM[a][b];
    if (a == 2 && b == 1) f += 0.4f * st->retro;   /* V -> IV is fine in rock/pop */
    return f;
}

float harmony_score(const hctx_t *hc, const chord_t *c) {
    eff_t E;
    eff_of(hc, &E);
    const style_t *st = &E.num;
    int frame = frame_of(hc->scale), deg = (c->root - hc->key + 12) % 12, fb = fbit(c);
    const quality_t *q = &QUALITIES[c->q];
    float sp = hc->spice, sc = 0;

    /* 1. what it is in the key */
    switch (harmony_class(hc->key, hc->scale, c)) {
    case CLS_DIATONIC: break;
    case CLS_BORROWED: sc -= 1.7f - 1.3f * st->borrow - 0.7f * sp; break;
    case CLS_SECONDARY: sc -= 1.7f - 1.3f * st->secdom - 0.7f * sp; break;
    case CLS_SUBV: sc -= 2.8f - 1.6f * st->chroma - 1.2f * sp; break;
    default: sc -= 4.2f - 1.6f * st->chroma - 2.0f * sp; break;
    }
    /* 2. the style's vocabulary */
    sc += 0.9f * tok_weight2(&E, 0, frame, deg, fb);
    sc += 0.9f * qpref(&E, c->q);
    if (q->sus) sc += 0.5f * (st->sus - 0.5f);
    float target = st->base_cx + (sp - 0.5f) * 2.6f;
    if (target < 0) target = 0;
    if (target > 3) target = 3;
    float dcx = (float)q->cx - target;
    sc -= 0.75f * (dcx < 0 ? -dcx : dcx);
    if (c->bass >= 0) {
        sc -= 1.6f - 1.4f * st->slash;
        int bi = (c->bass - c->root + 12) % 12;
        if (bi == 7) sc -= 0.5f;                /* 6/4 positions are mostly passing */
    }
    /* sus chords live on I, ii, IV, V, bVII; diminished triads other than as leading-tone chords are rare */
    if (q->sus && deg != 0 && deg != 2 && deg != 5 && deg != 7 && deg != 10) sc -= 0.9f;
    int cls = harmony_class(hc->key, hc->scale, c);
    if (q->fam == F_DIM && cls == CLS_DIATONIC && deg != 11 && deg != 2) sc -= 0.4f;
    if (q->fam == F_DIM && cls != CLS_DIATONIC && cls != CLS_SECONDARY) sc -= 1.2f;
    if (q->fam == F_AUG && deg != 0 && deg != 7) sc -= 1.0f;
    /* 3. context */
    if (hc->prev) {
        sc += transition(st, hc->prev, c, sp);
        sc += 0.8f * idiom_weight(&E, frame, hc->key, hc->prev, c);
        sc += func_bonus(st, hc->key, hc->scale, hc->prev, c);
        if (hc->prev2 && hc->prev2->root == c->root && fam_of(hc->prev2) == fam_of(c)) sc -= 0.9f * (1.0f - st->vamp);
        /* gravity: after an outside chord, coming home is attractive and wandering further costs */
        if (harmony_class(hc->key, hc->scale, hc->prev) != CLS_DIATONIC) {
            if (cls == CLS_DIATONIC || cls == CLS_SECONDARY) sc += 0.6f;
            else sc -= 0.7f * (1.0f - sp);
            if (hc->prev2 && harmony_class(hc->key, hc->scale, hc->prev2) != CLS_DIATONIC && cls != CLS_DIATONIC) sc -= 1.0f;
        }
    } else {
        sc += 0.9f * tok_weight2(&E, 1, frame, deg, fb);
        if (harmony_class(hc->key, hc->scale, c) != CLS_DIATONIC) sc -= 2.0f;
        if (c->bass >= 0) sc -= 1.5f;
        if (q->sus || q->fam == F_DIM || q->fam == F_AUG) sc -= 1.0f;
    }
    if (hc->next) {
        sc += 0.6f * transition(st, c, hc->next, sp);
        sc += 0.5f * idiom_weight(&E, frame, hc->key, c, hc->next);
    }
    if (hc->loop_to) {
        sc += 0.45f * transition(st, c, hc->loop_to, sp);
        sc += 0.4f * idiom_weight(&E, frame, hc->key, c, hc->loop_to);
        if (chord_equal(c, hc->loop_to)) sc -= 3.0f;
    }
    return sc;
}

static int cand_cmp(const void *a, const void *b) {
    float x = ((const cand_t *)a)->score, y = ((const cand_t *)b)->score;
    return x < y ? 1 : x > y ? -1 : 0;
}

void harmony_score_all(const hctx_t *hc, cand_list_t *out) {
    out->n = 0;
    for (int r = 0; r < 12; r++)
        for (int q = 0; q < NUM_QUALITIES; q++) {
            chord_t c = {r, q, -1};
            out->all[out->n].c = c;
            out->all[out->n].score = harmony_score(hc, &c);
            out->n++;
        }
    qsort(out->all, out->n, sizeof out->all[0], cand_cmp);
    /* slash-bass variants of the best 48 */
    int base = out->n < 48 ? out->n : 48;
    for (int i = 0; i < base && out->n < MAX_CANDS - 4; i++) {
        chord_t c = out->all[i].c;
        const quality_t *q = &QUALITIES[c.q];
        int basses[4], nb = 0;
        for (int k = 1; k < q->n && nb < 3; k++) {
            int iv = q->iv[k] % 12;
            if (iv == 3 || iv == 4 || iv == 7 || iv == 10) basses[nb++] = (c.root + iv) % 12;
        }
        if (q->fam == F_MAJ && !q->sus && (c.root - hc->key + 12) % 12 == 5) basses[nb++] = (c.root + 2) % 12;   /* IV/V */
        for (int k = 0; k < nb; k++) {
            chord_t v = c;
            v.bass = basses[k];
            out->all[out->n].c = v;
            out->all[out->n].score = harmony_score(hc, &v);
            out->n++;
        }
    }
    qsort(out->all, out->n, sizeof out->all[0], cand_cmp);
}

static int group_of(const chord_t *c) {
    int f = fam_of(c);
    if (f == F_SUS) f = F_MAJ;   /* sus2/sus4 triads are colours of the same chord, not a different idea */
    return (c->root * NUM_FAM + f) * 13 + (c->bass >= 0 ? c->bass + 1 : 0);
}

int harmony_pool(const hctx_t *hc, chord_t *pool, int n, unsigned char *seen, uint32_t *rng, const chord_t *avoid, int navoid) {
    cand_list_t L;   /* 16 KB on the caller's stack (the worker thread has 256 KB) */
    harmony_score_all(hc, &L);
    /* best candidate per group, in order */
    enum { MAXG = 96 };
    int gidx[MAXG], gfirst[MAXG], ng = 0;
    for (int i = 0; i < L.n && ng < MAXG; i++) {
        int g = group_of(&L.all[i].c), dup = 0;
        for (int k = 0; k < ng; k++) if (gidx[k] == g) { dup = 1; break; }
        if (!dup) { gidx[ng] = g; gfirst[ng] = i; ng++; }
    }
    if (!ng) return 0;
    float best = L.all[gfirst[0]].score;
    eff_t E;
    eff_of(hc, &E);
    const style_t *st = &E.num;
    float temp = 0.45f + 0.55f * hc->spice, win = 2.5f + 4.5f * hc->spice;
    int max_slash = st->slash >= 0.5f ? 2 : 1;
    /* candidates allowed: unseen, within a window below the best */
    int avail = 0;
    for (int k = 0; k < ng; k++) if (!seen[gidx[k]] && L.all[gfirst[k]].score > best - win) avail++;
    int restarted = 0;
    if (avail < n) {   /* everything good was shown: start over, but not with the same first idea */
        memset(seen, 0, HARMONY_GROUPS);
        restarted = 1;
        if (avail + navoid >= n + navoid / 2)   /* enough left without the set just shown */
            for (int i = 0; i < navoid; i++) seen[group_of(&avoid[i])] = 1;
    }
    int picked = 0, nslash = 0, per_root[12] = {0};
    unsigned char used[MAXG] = {0};
    /* the best unseen group always gets in first (the "safe" pick), the rest are sampled */
    for (int round = 0; picked < n && round < 400; round++) {
        float tot = 0, w[MAXG];
        for (int k = 0; k < ng; k++) {
            w[k] = 0;
            if (used[k] || seen[gidx[k]]) continue;
            const chord_t *c = &L.all[gfirst[k]].c;
            float sc = L.all[gfirst[k]].score;
            if (sc < best - win) continue;
            if (per_root[c->root] >= 2) continue;
            if (c->bass >= 0 && nslash >= max_slash) continue;
            w[k] = expf((sc - best) / temp);
            if (per_root[c->root]) w[k] *= 0.35f;
            tot += w[k];
        }
        if (tot <= 0) {
            if (win < 12.0f) { win += 2.0f; continue; }
            break;
        }
        int k = 0;
        if (picked == 0 && !restarted) {
            for (k = 0; k < ng && w[k] <= 0; k++) {}
        } else {
            float r = rng_unit(rng) * tot;
            for (k = 0; k < ng; k++) { r -= w[k]; if (r <= 0 && w[k] > 0) break; }
            if (k >= ng) for (k = ng - 1; k > 0 && w[k] <= 0; k--) {}
        }
        used[k] = 1;
        /* within the group: a colour sampled from its own candidates */
        int g = gidx[k];
        float gw[64], gt = 0;
        int gi[64], gn = 0;
        for (int i = 0; i < L.n && gn < 64; i++)
            if (group_of(&L.all[i].c) == g && L.all[i].score > L.all[gfirst[k]].score - 2.5f) {
                gi[gn] = i;
                gw[gn] = expf((L.all[i].score - L.all[gfirst[k]].score) / 0.6f);
                gt += gw[gn++];
            }
        int pick = gfirst[k];
        if (gn > 1 && picked > 0) {
            float r = rng_unit(rng) * gt;
            for (int j = 0; j < gn; j++) { r -= gw[j]; if (r <= 0) { pick = gi[j]; break; } }
        }
        /* 4.0: never the same notes twice in a pool (C6 and Am7/C are one sound): another colour of the group, or skip it */
        int dupn = 0;
        for (int j = 0; j < picked; j++) if (chord_pcs(&pool[j]) == chord_pcs(&L.all[pick].c)) dupn = 1;
        if (dupn) {
            int alt = -1;
            for (int j = 0; j < gn && alt < 0; j++) {
                int ok = 1;
                for (int q = 0; q < picked; q++) if (chord_pcs(&pool[q]) == chord_pcs(&L.all[gi[j]].c)) ok = 0;
                if (ok) alt = gi[j];
            }
            if (alt < 0) { seen[g] = 1; continue; }
            pick = alt;
        }
        pool[picked++] = L.all[pick].c;
        per_root[L.all[pick].c.root]++;
        if (L.all[pick].c.bass >= 0) nslash++;
        seen[g] = 1;
    }
    return picked;
}

void harmony_generate(int key, int scale, int style, int artist, int producer, const float *mix, float spice, int len,
                      chord_t *out, uint32_t *rng) {
    cand_list_t L;
    if (len > 16) len = 16;
    for (int i = 0; i < len; i++) {
        hctx_t hc = {key, scale, style, spice, i ? &out[i - 1] : NULL, i > 1 ? &out[i - 2] : NULL, NULL,
                     (i == len - 1 && len > 1) ? &out[0] : NULL, artist, producer, mix != NULL,
                     mix ? mix[0] : 1.0f, mix ? mix[1] : 1.0f, mix ? mix[2] : 1.0f};
        /* 8 chords: the second half often restates the first with a new ending, as songs do */
        if (len == 8 && i >= 4 && i < 6 && (rng_next(rng) % 100) < 40 && !chord_equal(&out[i - 4], &out[i - 1])) {
            out[i] = out[i - 4];
            continue;
        }
        harmony_score_all(&hc, &L);
        /* no slash chord as the first chord; at most a few */
        float temp = 0.45f + 0.45f * spice, best = L.all[0].score, tot = 0;
        int lim = L.n < 30 ? L.n : 30, nsl = 0;
        for (int j = 0; j < i; j++) nsl += out[j].bass >= 0;
        float w[30];
        for (int k = 0; k < lim; k++) {
            w[k] = expf((L.all[k].score - best) / temp);
            if (L.all[k].c.bass >= 0 && (i == 0 || nsl >= len / 4)) w[k] = 0;
            for (int j = 0; j < i; j++) {
                const chord_t *c = &L.all[k].c;
                if (chord_equal(&out[j], c)) w[k] *= j == i - 1 ? 0.0f : 0.4f;
                else if (out[j].root == c->root) w[k] *= 0.7f;
                if (j == i - 1 && out[j].root == c->root && !(QUALITIES[out[j].q].sus && !QUALITIES[c->q].sus)) w[k] *= 0.1f;
            }
            tot += w[k];
        }
        float r = rng_unit(rng) * tot;
        int k = 0;
        for (; k < lim - 1; k++) { r -= w[k]; if (r <= 0 && w[k] > 0) break; }
        out[i] = L.all[k].c;
    }
}

/* ------------------------------------------------------------------------------------------------ refit */
int harmony_map_pc(int pc, int okey, int oscale, int key, int scale) {
    int rel = ((pc - okey) % 12 + 12) % 12;
    if (oscale != scale)
        for (int d = 0; d < 7; d++)
            if (SCALES[oscale][d] == rel) { rel = SCALES[scale][d]; break; }
    return (key + rel) % 12;
}

static float colour_likeness(const chord_t *a, const chord_t *b) {
    const quality_t *qa = &QUALITIES[a->q], *qb = &QUALITIES[b->q];
    float l = 0;
    if (a->q == b->q) l += 0.5f;
    if (qa->fam == qb->fam) l += 1.0f;
    else if ((qa->fam == F_MAJ && (qb->fam == F_DOM || qb->fam == F_SUS)) || (qb->fam == F_MAJ && (qa->fam == F_DOM || qa->fam == F_SUS)))
        l += 0.4f;
    else if ((qa->fam == F_MIN && qb->fam == F_HDIM) || (qa->fam == F_HDIM && qb->fam == F_MIN)) l += 0.3f;
    l -= 0.3f * (float)abs(qa->cx - qb->cx);
    return l;
}

chord_t harmony_refit(const hctx_t *h, int okey, int oscale, const chord_t *c, int recolour) {
    chord_t base = *c;
    base.root = harmony_map_pc(c->root, okey, oscale, h->key, h->scale);
    if (c->bass >= 0) base.bass = harmony_map_pc(c->bass, okey, oscale, h->key, h->scale);
    if (!recolour) return base;
    /* keep the root (its degree), let the colour follow the new setting, staying close to the old colour; a new
     * scale is asked for its own colours, so the old colour counts for less then */
    float lw = oscale != h->scale ? 0.5f : 0.9f;
    chord_t best = base;
    float bs = -1e9f;
    for (int q = 0; q < NUM_QUALITIES; q++)
        for (int b = 0; b < 2; b++) {
            chord_t t = {base.root, q, -1};
            if (b) {
                if (base.bass < 0) continue;
                t.bass = base.bass;
                if (!((chord_pcs(&t) >> base.bass) & 1) && !(QUALITIES[q].fam == F_MAJ && (base.bass - base.root + 12) % 12 == 2))
                    continue;   /* a slash bass that is no longer a chord tone */
                if (t.bass == t.root) continue;
            }
            float sc = harmony_score(h, &t) + lw * colour_likeness(c, &t) + (b ? 0.8f : 0.0f);
            if (sc > bs + 1e-4f) { bs = sc; best = t; }
        }
    return best;
}

/* ------------------------------------------------------------------------------------------------ cruise */
chord_t harmony_next(const hctx_t *h, const chord_t *used, int nused) {
    cand_list_t L;   /* on the caller's stack (worker thread) */
    harmony_score_all(h, &L);
    int has_dom = 0;
    for (int i = 0; i < nused; i++) if (fam_of(&used[i]) == F_DOM) has_dom = 1;
    int best = 0;
    float bs = -1e9f;
    for (int i = 0; i < L.n && i < 200; i++) {
        const chord_t *c = &L.all[i].c;
        float sc = L.all[i].score;
        /* progression context: a chord already used twice is tired, the same root again a little less fresh */
        int same = 0, root = 0;
        for (int k = 0; k < nused; k++) { if (chord_equal(&used[k], c)) same++; else if (used[k].root == c->root) root++; }
        sc -= 1.4f * same * same + 0.35f * root;
        /* tension and release: before the loop closes, a progression without any pull gets one */
        if (h->loop_to && !has_dom && fam_of(c) == F_DOM) sc += 0.6f;
        if (c->bass >= 0 && !h->prev) sc -= 1.0f;
        if (sc > bs) { bs = sc; best = i; }
    }
    return L.all[best].c;
}
