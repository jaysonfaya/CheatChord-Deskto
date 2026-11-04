/* Chord Pilot artist / producer profiles. These describe harmonic habits heard across each catalogue (which degrees,
 * which chord colours, which moves), as an influence on the model. They contain no songs.
 * Numbers in ARTISTS are DELTAS added to the chosen style; lists are added on top of the style's lists.
 * Degrees: semitones above the key's tonic (0 I, 2 II, 3 bIII, 4 III, 5 IV, 7 V, 8 bVI, 9 VI, 10 bVII, 1 bII, 6 #IV). */
#include <stddef.h>
#include "artists.h"

#define M FM_M
#define m FM_m
#define D FM_D
#define o FM_o
#define h FM_h
#define s FM_s
#define A FM_A
#define MX FM_MX

/* style indices (harmony.c STYLE_NAMES) */
enum { POP, BRIT, SOUL, FUNK, NEO, GOSPEL, JAZZ, RNB, HOUSE, LOFI, CINE, ROCK };

/* 1 Michael Jackson: minor funk vamps, the dorian IV7, sus dominants, i-iv-v grooves, IV/V lifts */
static const tok_t MJ_T[] = {{1, 0, m, 1.0f}, {1, 5, D | M, .9f}, {1, 5, m, .7f}, {1, 7, m, .6f}, {1, 10, MX, .5f},
                             {0, 2, m, .6f}, {0, 7, s, .6f}, {0, 9, m, .5f}, END_T};
static const idiom_t MJ_I[] = {{1, 0, 5, m, D | M, 1.0f}, {1, 5, 0, D | M, m, .8f}, {1, 0, 5, m, m, .7f}, {1, 5, 7, m, m, .6f},
                               {1, 7, 0, m, m, .6f}, {1, 8, 10, M, MX, .5f}, {0, 2, 7, m, s | D, .6f}, {0, 5, 7, M, s, .5f}, END_I};
static const qpref_t MJ_Q[] = {{Q_M7, 1.0f}, {Q_M9, .8f}, {Q_M11, .7f}, {Q_7SUS4, .7f}, {Q_9, .7f}, {Q_9SUS4, .6f}, {Q_ADD9, .5f},
                               {Q_MIN, .5f}, {Q_MAJ, .3f}, {Q_MAJ7, .4f}, END_Q};
static const tok_t MJ_S[] = {{1, 0, m, 1.5f}, {0, 2, m, .8f}, END_T};

/* 2 Quincy Jones: big-band sophistication on pop: ii-V chains, 13ths, altered dominants, #11, chromatic bass lines */
static const tok_t QJ_T[] = {{0, 2, m, .9f}, {0, 7, D, .9f}, {0, 9, D, .7f}, {0, 2, D, .6f}, {0, 1, D, .6f}, {0, 6, h, .5f},
                             {0, 5, M, .6f}, {0, 4, m, .5f}, {1, 2, h, .8f}, {1, 7, D, .8f}, {1, 8, M, .5f}, END_T};
static const idiom_t QJ_I[] = {{2, 2, 7, m | h, D, 1.1f}, {0, 7, 0, D, M, .8f}, {0, 9, 2, D, m, .9f}, {0, 4, 9, m, D, .7f},
                               {0, 2, 1, m, D, .7f}, {0, 1, 0, D, M, .9f}, {0, 5, 4, M, m, .5f}, {0, 6, 5, h, M | m, .5f}, END_I};
static const qpref_t QJ_Q[] = {{Q_MAJ7S11, .9f}, {Q_13, 1.0f}, {Q_7B9, .8f}, {Q_7S9, .6f}, {Q_M11, .7f}, {Q_M9, .8f},
                               {Q_MAJ9, .8f}, {Q_69, .7f}, {Q_M7B5, .7f}, {Q_13SUS4, .6f}, {Q_MAJ13, .6f}, END_Q};
static const tok_t QJ_S[] = {{0, 2, m, 1.0f}, {0, 0, M, .8f}, {0, 5, M, .6f}, END_T};

/* 3 Stevie Wonder: chromatic planing, bIII/bVI/bII major 7ths, 13sus, key lifts, gospel-inflected dominants */
static const tok_t SW_T[] = {{0, 3, M, .9f}, {0, 8, M, .8f}, {0, 1, M | D, .8f}, {0, 10, D | M, .6f}, {0, 7, s | D, .7f},
                             {0, 2, m, .6f}, {1, 1, M, .5f}, {1, 3, M, .6f}, END_T};
static const idiom_t SW_I[] = {{0, 0, 3, M, M, .9f}, {0, 3, 8, M, M, .7f}, {0, 8, 1, M, M | D, .7f}, {0, 1, 0, M | D, M, .9f},
                               {0, 8, 10, M, D | s, .8f}, {0, 10, 0, D | s, M, .8f}, {0, 2, 7, m, s, .6f}, {1, 0, 1, m, M, .5f}, END_I};
static const qpref_t SW_Q[] = {{Q_MAJ9, 1.0f}, {Q_MAJ7, .8f}, {Q_13SUS4, .9f}, {Q_9SUS4, .8f}, {Q_M11, .7f}, {Q_7S9, .6f},
                               {Q_69, .6f}, {Q_13, .7f}, {Q_M9, .7f}, END_Q};
static const tok_t SW_S[] = {{0, 0, M, 1.0f}, {0, 2, m, .7f}, {1, 0, m, 1.0f}, END_T};

/* 4 The Beatles: modal mixture (bVII, iv, bVI), II and III major, augmented passing chords, 6ths */
static const tok_t BE_T[] = {{0, 10, M, 1.0f}, {0, 5, m, .8f}, {0, 8, M, .7f}, {0, 2, M | D, .8f}, {0, 4, M | D, .6f},
                             {0, 0, A, .4f}, {0, 3, M, .4f}, END_T};
static const idiom_t BE_I[] = {{0, 0, 10, M, M, .9f}, {0, 5, 5, M, m, 1.0f}, {0, 5, 0, m, M, .9f}, {0, 0, 2, M, M | D, .7f},
                               {0, 2, 5, M | D, M, .6f}, {0, 8, 10, M, M, .7f}, {0, 0, 0, M, A, .5f}, {0, 4, 9, M | D, m, .6f}, END_I};
static const qpref_t BE_Q[] = {{Q_MAJ, 1.0f}, {Q_MIN, .9f}, {Q_7, .8f}, {Q_6, .6f}, {Q_M6, .6f}, {Q_AUG, .5f}, {Q_M7, .4f},
                               {Q_MAJ7, .3f}, END_Q};
static const tok_t BE_S[] = {{0, 0, M, 1.5f}, END_T};

/* 5 Prince: mixolydian rock-funk (I-bVII-IV), sus2/add9 shimmer, minor 7 funk, chromatic mediants */
static const tok_t PR_T[] = {{0, 10, M, .9f}, {0, 5, M, .7f}, {0, 3, M, .6f}, {0, 8, M, .6f}, {0, 0, D | s, .6f},
                             {1, 0, m, .7f}, {1, 5, D, .6f}, END_T};
static const idiom_t PR_I[] = {{0, 0, 10, M | D | s, M, .9f}, {0, 10, 5, M, M, .8f}, {0, 5, 0, M, M | D | s, .6f},
                               {0, 0, 3, M, M, .6f}, {0, 0, 8, M, M, .5f}, {1, 0, 5, m, D, .6f}, END_I};
static const qpref_t PR_Q[] = {{Q_ADD9, 1.0f}, {Q_SUS2, .8f}, {Q_7SUS4, .7f}, {Q_9SUS4, .7f}, {Q_M7, .8f}, {Q_9, .6f},
                               {Q_MAJ, .6f}, {Q_MAJ7, .4f}, END_Q};
static const tok_t PR_S[] = {{0, 0, M | s, 1.2f}, {1, 0, m, 1.0f}, END_T};

/* 6 Drake (with producers like 40): dark minor ambience, two-chord vamps, i-bVI / i-iv, lush 7ths and 9ths, little motion */
static const tok_t DR_T[] = {{1, 0, m, 1.1f}, {1, 8, M, 1.0f}, {1, 5, m, .8f}, {1, 3, M, .5f}, {1, 10, M, .4f},
                             {0, 9, m, .9f}, {0, 5, M, .9f}, {0, 4, m, .5f}, END_T};
static const idiom_t DR_I[] = {{1, 0, 8, m, M, 1.0f}, {1, 8, 0, M, m, 1.0f}, {1, 0, 5, m, m, .8f}, {1, 5, 0, m, m, .8f},
                               {0, 9, 5, m, M, .9f}, {0, 5, 9, M, m, .9f}, END_I};
static const qpref_t DR_Q[] = {{Q_M9, 1.0f}, {Q_M7, .9f}, {Q_MAJ7, .9f}, {Q_MAJ9, .8f}, {Q_MADD9, .7f}, {Q_M11, .5f}, END_Q};
static const tok_t DR_S[] = {{1, 0, m, 1.5f}, {0, 9, m, 1.0f}, END_T};

/* 7 Kanye West (soul-sample era): looped soul chords, bVI-bVII-I lifts, major 7 loops, dramatic minor turns */
static const tok_t KW_T[] = {{0, 8, M, .9f}, {0, 10, M, .9f}, {0, 5, M, .7f}, {0, 5, m, .6f}, {0, 9, m, .5f},
                             {1, 8, M, .8f}, {1, 10, M, .7f}, {1, 3, M, .6f}, END_T};
static const idiom_t KW_I[] = {{0, 8, 10, M, M, 1.0f}, {0, 10, 0, M, M, .9f}, {0, 0, 8, M, M, .5f}, {0, 5, 5, M, m, .6f},
                               {1, 0, 10, m, M, .6f}, {1, 10, 8, M, M, .7f}, {1, 8, 7, M, M | D, .5f}, END_I};
static const qpref_t KW_Q[] = {{Q_MAJ7, 1.0f}, {Q_M7, .8f}, {Q_MAJ, .6f}, {Q_ADD9, .6f}, {Q_MIN, .5f}, {Q_9, .4f}, END_Q};
static const tok_t KW_S[] = {{0, 0, M, 1.0f}, {0, 8, M, .7f}, {1, 0, m, 1.0f}, END_T};

/* 8 J Dilla: jazzy chopped soul, m9/maj9, half-step slides, ii-V fragments, unresolved loops */
static const tok_t JD_T[] = {{0, 2, m, .8f}, {0, 4, m, .7f}, {0, 5, M, .8f}, {0, 1, M, .5f}, {0, 3, M, .5f},
                             {1, 0, m, .8f}, {1, 5, m, .7f}, END_T};
static const idiom_t JD_I[] = {{0, 5, 4, M, m, .8f}, {0, 4, 3, m, M, .6f}, {0, 3, 2, M, m, .7f}, {0, 2, 1, m, M | D, .6f},
                               {0, 1, 0, M | D, M, .6f}, {2, 2, 7, m | h, D, .6f}, END_I};
static const qpref_t JD_Q[] = {{Q_M9, 1.0f}, {Q_MAJ9, .9f}, {Q_M11, .7f}, {Q_7S9, .6f}, {Q_13, .6f}, {Q_MAJ7, .8f}, {Q_M7, .7f}, END_Q};
static const tok_t JD_S[] = {{0, 5, M, 1.0f}, {0, 2, m, .9f}, {1, 0, m, 1.0f}, END_T};

/* 9 Pharrell / The Neptunes: bright major 7 and #11 colours, parallel planing by whole steps, quirky IV/V, 9sus */
static const tok_t PW_T[] = {{0, 0, M, .9f}, {0, 2, M, .6f}, {0, 10, M, .7f}, {0, 5, M, .8f}, {0, 3, M, .5f},
                             {0, 7, s, .6f}, END_T};
static const idiom_t PW_I[] = {{0, 0, 10, M, M, .8f}, {0, 10, 0, M, M, .7f}, {0, 0, 2, M, M, .7f}, {0, 2, 0, M, M, .6f},
                               {0, 5, 7, M, s, .7f}, {0, 0, 3, M, M, .5f}, END_I};
static const qpref_t PW_Q[] = {{Q_MAJ7S11, 1.0f}, {Q_MAJ9, .9f}, {Q_MAJ7, .8f}, {Q_9SUS4, .8f}, {Q_69, .7f}, {Q_ADD9, .6f},
                               {Q_M9, .5f}, END_Q};
static const tok_t PW_S[] = {{0, 0, M, 1.2f}, {0, 5, M, .7f}, END_T};

/* 10 Timbaland: dark minor, phrygian bII, stark triads, diminished tension, few chords */
static const tok_t TB_T[] = {{1, 0, m, 1.1f}, {1, 1, M, 1.0f}, {1, 8, M, .7f}, {1, 7, m, .6f}, {1, 11, o, .5f},
                             {1, 6, o, .4f}, END_T};
static const idiom_t TB_I[] = {{1, 0, 1, m, M, 1.0f}, {1, 1, 0, M, m, 1.0f}, {1, 0, 8, m, M, .6f}, {1, 8, 7, M, m, .6f},
                               {1, 11, 0, o, m, .6f}, END_I};
static const qpref_t TB_Q[] = {{Q_MIN, 1.0f}, {Q_MAJ, .8f}, {Q_MMAJ7, .6f}, {Q_DIM, .6f}, {Q_MADD9, .5f}, {Q_DIM7, .4f}, END_Q};
static const tok_t TB_S[] = {{1, 0, m, 1.6f}, END_T};

/* 11 Daft Punk: disco-funk loops, vi-I-iii-II and dorian m7 cycles, 9ths, steady four-chord rotations */
static const tok_t DP_T[] = {{0, 9, m, .9f}, {0, 0, M, .8f}, {0, 4, m, .8f}, {0, 2, M | D, .7f}, {0, 2, m, .6f},
                             {1, 0, m, .8f}, {1, 5, M | D, .8f}, END_T};
static const idiom_t DP_I[] = {{0, 9, 0, m, M, .8f}, {0, 0, 4, M, m, .8f}, {0, 4, 2, m, M | D, .9f}, {0, 2, 9, M | D, m, .7f},
                               {1, 0, 5, m, M | D, .8f}, {1, 5, 0, M | D, m, .7f}, END_I};
static const qpref_t DP_Q[] = {{Q_M7, 1.0f}, {Q_MAJ7, .8f}, {Q_9, .7f}, {Q_ADD9, .6f}, {Q_7, .6f}, {Q_M9, .6f}, {Q_MAJ, .5f}, END_Q};
static const tok_t DP_S[] = {{0, 9, m, 1.2f}, {1, 0, m, 1.2f}, END_T};

/* 12 Burt Bacharach: sophisticated pop: unexpected root moves, major 7 and 9sus4, II7 and bVII colours */
static const tok_t BB_T[] = {{0, 2, D | s, .7f}, {0, 10, M, .6f}, {0, 3, M, .5f}, {0, 4, m, .6f}, {0, 7, s, .8f},
                             {0, 9, m, .6f}, END_T};
static const idiom_t BB_I[] = {{0, 0, 3, M, M, .6f}, {0, 4, 10, m, M, .5f}, {0, 2, 7, D | s, s, .7f}, {0, 7, 0, s, M, .7f},
                               {0, 9, 2, m, D | s, .6f}, END_I};
static const qpref_t BB_Q[] = {{Q_MAJ7, 1.0f}, {Q_9SUS4, .9f}, {Q_M7, .8f}, {Q_MAJ9, .7f}, {Q_6, .6f}, {Q_7SUS4, .6f}, {Q_9, .5f}, END_Q};
static const tok_t BB_S[] = {{0, 0, M, 1.0f}, {0, 2, m, .6f}, END_T};

/* 13 Babyface: lush 90s ballads: IVmaj7-iii-vi, 9sus4 to V, passing diminished, maj9/m9 beds */
static const tok_t BF_T[] = {{0, 5, M, 1.0f}, {0, 4, m, .8f}, {0, 9, m, .8f}, {0, 7, s, .8f}, {0, 1, o, .4f}, {0, 6, o, .4f}, END_T};
static const idiom_t BF_I[] = {{0, 5, 4, M, m, .9f}, {0, 4, 9, m, m, .8f}, {0, 9, 2, m, m, .6f}, {0, 2, 7, m, s, .8f},
                               {0, 7, 7, s, D | M, .6f}, {0, 0, 1, M, o, .4f}, {0, 1, 2, o, m, .5f}, END_I};
static const qpref_t BF_Q[] = {{Q_MAJ9, 1.0f}, {Q_M9, .9f}, {Q_9SUS4, .9f}, {Q_13SUS4, .7f}, {Q_MAJ7, .8f}, {Q_DIM7, .5f},
                               {Q_M7, .7f}, END_Q};
static const tok_t BF_S[] = {{0, 5, M, 1.0f}, {0, 0, M, 1.0f}, END_T};

/* 14 Frank Ocean: drifting guitar-like major 7 and add9, borrowed iv, plagal moves, few chords, open endings */
static const tok_t FO_T[] = {{0, 0, M, .9f}, {0, 5, M, .9f}, {0, 5, m, .8f}, {0, 9, m, .6f}, {0, 4, m, .5f}, {0, 10, M, .4f}, END_T};
static const idiom_t FO_I[] = {{0, 0, 5, M, M, .8f}, {0, 5, 5, M, m, .9f}, {0, 5, 0, m, M, .9f}, {0, 5, 0, M, M, .6f},
                               {0, 9, 5, m, M, .5f}, END_I};
static const qpref_t FO_Q[] = {{Q_MAJ7, 1.0f}, {Q_ADD9, .9f}, {Q_SUS2, .7f}, {Q_M7, .7f}, {Q_MADD9, .7f}, {Q_MAJ9, .7f},
                               {Q_M6, .5f}, END_Q};
static const tok_t FO_S[] = {{0, 0, M, 1.2f}, {0, 5, M, .9f}, END_T};

/* 15 The Weeknd: 80s synth noir: i-bVI-bIII-bVII cycles, sus2/add9 triads, plain minor and major colours */
static const tok_t WK_T[] = {{1, 0, m, 1.0f}, {1, 8, M, .9f}, {1, 3, M, .8f}, {1, 10, M, .8f}, {1, 5, m, .5f}, END_T};
static const idiom_t WK_I[] = {{1, 0, 8, m, M, .9f}, {1, 8, 3, M, M, .8f}, {1, 3, 10, M, M, .9f}, {1, 10, 0, M, m, .8f},
                               {1, 8, 10, M, M, .6f}, END_I};
static const qpref_t WK_Q[] = {{Q_MIN, 1.0f}, {Q_MAJ, .9f}, {Q_SUS2, .8f}, {Q_MADD9, .7f}, {Q_ADD9, .7f}, {Q_M7, .4f}, END_Q};
static const tok_t WK_S[] = {{1, 0, m, 1.5f}, END_T};

/* 16 Max Martin: chart pop: strong cadences, I-V-vi-IV and vi-IV-I-V, sus4 before V, ii-IV-V build ups */
static const tok_t MM_T[] = {{0, 0, M, 1.0f}, {0, 7, M | s, .9f}, {0, 9, m, .9f}, {0, 5, M, 1.0f}, {0, 2, m, .5f}, END_T};
static const idiom_t MM_I[] = {{0, 0, 7, M, M, .8f}, {0, 7, 9, M, m, .9f}, {0, 9, 5, m, M, .9f}, {0, 5, 0, M, M, .7f},
                               {0, 5, 7, M, M | s, .8f}, {0, 2, 5, m, M, .6f}, {0, 7, 7, s, M, .7f}, END_I};
static const qpref_t MM_Q[] = {{Q_MAJ, 1.0f}, {Q_MIN, .9f}, {Q_SUS4, .7f}, {Q_ADD9, .6f}, {Q_SUS2, .5f}, END_Q};
static const tok_t MM_S[] = {{0, 0, M, 1.2f}, {0, 9, m, 1.0f}, {0, 5, M, .8f}, END_T};

/* 17 D'Angelo: neo-soul grit: m11 and 7#9, bluesy dominant IV, chromatic slides into ii-V */
static const tok_t DA_T[] = {{0, 2, m, .9f}, {0, 5, D, .8f}, {0, 4, D, .7f}, {0, 1, D, .6f}, {0, 9, m, .6f},
                             {1, 0, m, 1.0f}, {1, 5, D, .8f}, END_T};
static const idiom_t DA_I[] = {{0, 2, 1, m, D, .7f}, {0, 1, 0, D, M | D, .8f}, {0, 4, 9, D, m, .8f}, {2, 2, 7, m, D, .7f},
                               {1, 0, 5, m, D, .9f}, {1, 5, 0, D, m, .8f}, END_I};
static const qpref_t DA_Q[] = {{Q_M11, 1.0f}, {Q_7S9, .9f}, {Q_13, .8f}, {Q_M9, .8f}, {Q_9, .7f}, {Q_MAJ9, .5f}, END_Q};
static const tok_t DA_S[] = {{0, 2, m, 1.0f}, {1, 0, m, 1.2f}, END_T};

/* 18 Bruno Mars: retro soul-funk: doo-wop I-vi-ii-V, dominant 9 funk, IV-iii-ii stepping down, gospel V/V */
static const tok_t BM_T[] = {{0, 2, m, .9f}, {0, 4, m, .6f}, {0, 9, m, .7f}, {0, 5, M, .8f}, {0, 7, D, .7f}, {0, 2, D, .5f},
                             {1, 0, m, .8f}, {1, 5, D, .6f}, END_T};
static const idiom_t BM_I[] = {{0, 5, 4, M, m, .9f}, {0, 4, 2, m, m, .8f}, {0, 2, 0, m, M, .6f}, {0, 0, 9, M, m, .7f},
                               {0, 9, 2, m, m, .7f}, {0, 2, 7, m, D, .8f}, {1, 0, 5, m, D, .7f}, END_I};
static const qpref_t BM_Q[] = {{Q_M7, 1.0f}, {Q_MAJ7, .9f}, {Q_9, .8f}, {Q_M9, .7f}, {Q_7, .6f}, {Q_13, .5f}, {Q_MAJ, .5f},
                               {Q_MIN, .4f}, END_Q};
static const tok_t BM_S[] = {{0, 0, M, 1.0f}, {0, 5, M, .7f}, {1, 0, m, 1.0f}, END_T};

/* 19 SZA: floating alt-R&B: maj9 and m9 clouds, IV-iii, borrowed iv, few chords, sus2 */
static const tok_t SZ_T[] = {{0, 5, M, 1.0f}, {0, 4, m, .7f}, {0, 9, m, .6f}, {0, 5, m, .6f}, {0, 2, m, .5f}, {1, 0, m, .8f},
                             {1, 10, M, .7f}, {1, 8, M, .7f}, END_T};
static const idiom_t SZ_I[] = {{0, 5, 4, M, m, .9f}, {0, 4, 5, m, M, .7f}, {0, 5, 5, M, m, .8f}, {0, 5, 0, m, M, .6f},
                               {1, 0, 10, m, M, .7f}, {1, 10, 8, M, M, .7f}, {1, 8, 0, M, m, .6f}, END_I};
static const qpref_t SZ_Q[] = {{Q_MAJ9, 1.0f}, {Q_MAJ7, .9f}, {Q_M9, .9f}, {Q_ADD9, .7f}, {Q_SUS2, .6f}, {Q_M11, .6f},
                               {Q_MADD9, .5f}, END_Q};
static const tok_t SZ_S[] = {{0, 5, M, 1.2f}, {1, 0, m, 1.0f}, {0, 9, m, .8f}, END_T};

/* 20 Coldplay: anthemic piano/guitar triads with add9 and sus2, plagal IV-I, vi-IV-I-V, few sevenths */
static const tok_t CP_T[] = {{0, 0, M, .8f}, {0, 5, M, 1.0f}, {0, 9, m, .9f}, {0, 7, M, .7f}, {0, 4, m, .5f}, {0, 10, M, .4f},
                             {1, 0, m, .6f}, {1, 8, M, .8f}, {1, 3, M, .7f}, END_T};
static const idiom_t CP_I[] = {{0, 5, 0, M, M, .8f}, {0, 9, 5, m, M, .8f}, {0, 0, 7, M, M, .6f}, {0, 5, 9, M, m, .6f},
                               {0, 4, 5, m, M, .5f}, {1, 8, 3, M, M, .7f}, {1, 3, 10, M, M, .6f}, END_I};
static const qpref_t CP_Q[] = {{Q_ADD9, 1.0f}, {Q_SUS2, .9f}, {Q_MAJ, .9f}, {Q_MIN, .8f}, {Q_SUS4, .6f}, {Q_MADD9, .5f},
                               {Q_MAJ7, .1f}, END_Q};
static const tok_t CP_S[] = {{0, 5, M, 1.0f}, {0, 0, M, 1.0f}, {0, 9, m, .8f}, END_T};

/* 21 Adele: piano ballad: i-bVI-bIII-bVII, III7 to vi, sus4 before V, walking slash basses */
static const tok_t AD_T[] = {{0, 4, D, .7f}, {0, 9, m, .9f}, {0, 5, M, .8f}, {0, 7, s, .6f}, {0, 4, m, .5f}, {1, 0, m, 1.0f},
                             {1, 8, M, .9f}, {1, 3, M, .8f}, {1, 10, M, .7f}, {1, 7, D, .6f}, END_T};
static const idiom_t AD_I[] = {{0, 0, 4, M, D, .7f}, {0, 4, 9, D, m, 1.0f}, {0, 9, 5, m, M, .8f}, {0, 5, 7, M, s, .7f},
                               {1, 0, 8, m, M, .8f}, {1, 8, 3, M, M, .7f}, {1, 3, 10, M, M, .7f}, {1, 10, 7, M, D, .6f}, END_I};
static const qpref_t AD_Q[] = {{Q_MAJ, 1.0f}, {Q_MIN, 1.0f}, {Q_SUS4, .7f}, {Q_7, .5f}, {Q_ADD9, .4f}, {Q_M7, .4f},
                               {Q_MAJ7, .3f}, END_Q};
static const tok_t AD_S[] = {{1, 0, m, 1.2f}, {0, 0, M, 1.0f}, {0, 9, m, .8f}, END_T};

/* 22 Dr. Dre: G-funk minor: i-iv grooves, minor 9 / 11 synths, bVI to V, a little phrygian bII */
static const tok_t DD_T[] = {{1, 0, m, 1.1f}, {1, 5, m, .9f}, {1, 5, D, .6f}, {1, 8, M, .7f}, {1, 7, D, .6f}, {1, 1, M, .4f},
                             {0, 9, m, .6f}, {0, 2, m, .6f}, END_T};
static const idiom_t DD_I[] = {{1, 0, 5, m, m, 1.0f}, {1, 5, 0, m, m, .9f}, {1, 0, 8, m, M, .6f}, {1, 8, 7, M, D, .8f},
                               {1, 7, 0, D, m, .7f}, {1, 0, 1, m, M, .4f}, END_I};
static const qpref_t DD_Q[] = {{Q_M9, 1.0f}, {Q_M7, .9f}, {Q_M11, .8f}, {Q_MIN, .6f}, {Q_7B9, .5f}, {Q_9, .5f}, {Q_MAJ7, .4f}, END_Q};
static const tok_t DD_S[] = {{1, 0, m, 1.6f}, {0, 9, m, 1.0f}, END_T};

/* 23 Metro Boomin: dark trap minor: plain triads, i-bVI, phrygian bII, the harmonic-minor V, diminished colour */
static const tok_t MB_T[] = {{1, 0, m, 1.2f}, {1, 8, M, .9f}, {1, 1, M, .8f}, {1, 7, M, .6f}, {1, 7, D, .4f}, {1, 11, o, .5f},
                             {1, 3, M, .4f}, {1, 5, m, .6f}, END_T};
static const idiom_t MB_I[] = {{1, 0, 8, m, M, 1.0f}, {1, 8, 0, M, m, .9f}, {1, 0, 1, m, M, .8f}, {1, 1, 0, M, m, .8f},
                               {1, 8, 7, M, M | D, .8f}, {1, 7, 0, M | D, m, .7f}, {1, 11, 0, o, m, .6f}, END_I};
static const qpref_t MB_Q[] = {{Q_MIN, 1.0f}, {Q_MAJ, .8f}, {Q_DIM, .5f}, {Q_MADD9, .4f}, {Q_M7, .3f}, {Q_AUG, .2f}, END_Q};
static const tok_t MB_S[] = {{1, 0, m, 2.0f}, END_T};

/* 24 Nile Rodgers: disco-funk guitar: m7 and 9 vamps, ii-iii stepping, the dorian i-IV7 */
static const tok_t NR_T[] = {{0, 2, m, 1.0f}, {0, 4, m, .8f}, {0, 5, M, .7f}, {0, 7, D, .5f}, {1, 0, m, .9f}, {1, 5, D, .8f},
                             {1, 2, m, .6f}, {0, 9, m, .5f}, END_T};
static const idiom_t NR_I[] = {{0, 2, 4, m, m, 1.0f}, {0, 4, 2, m, m, .9f}, {0, 4, 5, m, M, .7f}, {0, 5, 4, M, m, .6f},
                               {1, 0, 5, m, D, 1.0f}, {1, 5, 0, D, m, .9f}, {1, 0, 2, m, m, .6f}, {1, 2, 0, m, m, .6f}, END_I};
static const qpref_t NR_Q[] = {{Q_M7, 1.0f}, {Q_9, .9f}, {Q_M9, .8f}, {Q_MAJ7, .7f}, {Q_7, .6f}, {Q_M11, .5f}, {Q_13, .5f},
                               {Q_7SUS4, .5f}, END_Q};
static const tok_t NR_S[] = {{0, 2, m, 1.2f}, {1, 0, m, 1.2f}, END_T};

/* 25 Jam & Lewis: 80s / 90s synth R&B: add9 and 9sus4 stabs, IV/V, bVII-IV, minor 11 */
static const tok_t JL_T[] = {{0, 5, M, .8f}, {0, 7, s, .7f}, {0, 10, M, .6f}, {0, 9, m, .6f}, {0, 2, m, .6f}, {1, 0, m, .8f},
                             {1, 10, M, .6f}, {1, 5, m, .6f}, END_T};
static const idiom_t JL_I[] = {{0, 5, 7, M, s | D, .8f}, {0, 7, 0, s | D, M, .7f}, {0, 10, 5, M, M, .7f}, {0, 0, 10, M, M, .5f},
                               {0, 2, 5, m, M, .6f}, {1, 0, 10, m, M, .6f}, {1, 10, 5, M, m, .5f}, END_I};
static const qpref_t JL_Q[] = {{Q_ADD9, 1.0f}, {Q_M11, .9f}, {Q_9SUS4, .8f}, {Q_MAJ9, .7f}, {Q_SUS2, .7f}, {Q_M9, .6f},
                               {Q_7SUS4, .6f}, END_Q};
static const tok_t JL_S[] = {{0, 5, M, .8f}, {0, 0, M, .8f}, {1, 0, m, 1.0f}, END_T};

/* 4.0: profiles 26.., ARTIST_INFO and the two option lists (tools/gen_data.py) */
#undef M
#undef m
#undef D
#undef o
#undef h
#undef s
#undef A
#undef MX
#include "gen/profiles_gen.inc"

/* base_cx borrow secdom chroma vamp slash sus fifths steps thirds mediant retro (all deltas) */
const style_t ARTISTS[NUM_ARTISTS] = {
    {0.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    {0.0f, .0f, -.1f, .0f, .3f, .2f, .3f, -.2f, .2f, .0f, .0f, .1f, MJ_T, MJ_I, MJ_Q, MJ_S},
    {0.6f, .1f, .3f, .3f, -.2f, .3f, .1f, .5f, .0f, .0f, .1f, -.2f, QJ_T, QJ_I, QJ_Q, QJ_S},
    {0.4f, .2f, .1f, .4f, .0f, .1f, .3f, .0f, .1f, .0f, .5f, .0f, SW_T, SW_I, SW_Q, SW_S},
    {-0.3f, .3f, .3f, .0f, -.1f, .1f, -.2f, .0f, .1f, .1f, .3f, .2f, BE_T, BE_I, BE_Q, BE_S},
    {-0.1f, .3f, -.1f, .1f, .2f, .1f, .4f, -.2f, .3f, .0f, .4f, .3f, PR_T, PR_I, PR_Q, PR_S},
    {-0.2f, .1f, -.3f, -.2f, .6f, -.2f, -.1f, -.3f, -.2f, .2f, .0f, .0f, DR_T, DR_I, DR_Q, DR_S},
    {-0.2f, .3f, .0f, .0f, .4f, .0f, .0f, -.2f, .3f, .0f, .2f, .2f, KW_T, KW_I, KW_Q, KW_S},
    {0.3f, .1f, .1f, .4f, .3f, -.1f, .0f, .1f, .2f, .0f, .1f, .0f, JD_T, JD_I, JD_Q, JD_S},
    {0.2f, .2f, .0f, .3f, .2f, .3f, .3f, -.3f, .4f, .0f, .4f, .1f, PW_T, PW_I, PW_Q, PW_S},
    {-0.6f, .2f, -.3f, .3f, .5f, -.2f, -.3f, -.3f, .2f, .0f, .2f, .0f, TB_T, TB_I, TB_Q, TB_S},
    {0.0f, .0f, .2f, -.1f, .4f, -.1f, -.1f, .1f, .2f, .2f, .0f, .1f, DP_T, DP_I, DP_Q, DP_S},
    {0.3f, .2f, .2f, .2f, -.2f, .2f, .4f, .0f, .1f, .2f, .4f, .0f, BB_T, BB_I, BB_Q, BB_S},
    {0.3f, .0f, .2f, .1f, -.1f, .3f, .4f, .1f, .0f, .2f, .0f, .0f, BF_T, BF_I, BF_Q, BF_S},
    {-0.1f, .4f, -.2f, .0f, .3f, .1f, .3f, -.3f, .1f, .1f, .1f, .2f, FO_T, FO_I, FO_Q, FO_S},
    {-0.6f, .2f, -.3f, -.1f, .3f, -.1f, .3f, -.3f, .2f, .2f, .2f, .2f, WK_T, WK_I, WK_Q, WK_S},
    {-0.6f, -.2f, -.1f, -.3f, .0f, .0f, .2f, .1f, .1f, .1f, -.2f, .3f, MM_T, MM_I, MM_Q, MM_S},
    {0.5f, .1f, .1f, .4f, .2f, .0f, .0f, .2f, .1f, .0f, .0f, .0f, DA_T, DA_I, DA_Q, DA_S},
    {0.2f, .1f, .3f, .1f, .1f, .1f, .0f, .3f, .3f, .1f, .0f, .1f, BM_T, BM_I, BM_Q, BM_S},
    {0.3f, .3f, -.3f, .1f, .4f, .1f, .3f, -.3f, .3f, .2f, .2f, .1f, SZ_T, SZ_I, SZ_Q, SZ_S},
    {-0.5f, .1f, -.4f, -.3f, .2f, .3f, .5f, -.2f, .3f, .2f, .0f, .3f, CP_T, CP_I, CP_Q, CP_S},
    {-0.3f, .1f, .3f, -.2f, -.1f, .5f, .2f, .1f, .3f, .2f, .0f, .1f, AD_T, AD_I, AD_Q, AD_S},
    {0.0f, .0f, -.2f, .1f, .6f, -.2f, -.2f, .1f, .0f, .0f, .0f, .0f, DD_T, DD_I, DD_Q, DD_S},
    {-0.8f, .2f, -.4f, .2f, .6f, -.3f, -.3f, -.2f, .2f, .0f, .3f, .0f, MB_T, MB_I, MB_Q, MB_S},
    {0.3f, .0f, -.1f, .0f, .5f, -.1f, .1f, -.3f, .5f, .0f, .0f, .0f, NR_T, NR_I, NR_Q, NR_S},
    {0.2f, .2f, -.1f, .1f, .2f, .6f, .5f, -.1f, .2f, .1f, .2f, .1f, JL_T, JL_I, JL_Q, JL_S},
    GEN_PROFILE_ROWS
};

/* the 3.x browser lists (index 0 = none): old projects are converted with these */
const int OLD_ARTIST_LIST[NUM_OLD_ARTIST_LIST] = {0, 1, 3, 4, 5, 6, 14, 15, 17, 12, 18, 19, 20, 21};
const int OLD_PRODUCER_LIST[NUM_OLD_PRODUCER_LIST] = {0, 2, 7, 8, 9, 10, 11, 13, 16, 22, 23, 24, 25};

int artist_list_index(int profile) {
    for (int i = 0; i < NUM_ARTIST_LIST; i++) if (ARTIST_LIST[i] == profile) return i;
    return 0;
}
int producer_list_index(int profile) {
    for (int i = 0; i < NUM_PRODUCER_LIST; i++) if (PRODUCER_LIST[i] == profile) return i;
    return 0;
}
