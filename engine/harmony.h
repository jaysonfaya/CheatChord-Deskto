/* Chord Pilot harmony model: scores every possible next chord against the key, the style, the chords around it
 * and the "spice" setting, and samples a varied pool of suggestions from the best of them. */
#pragma once
#include "theory.h"

#include "gen/counts.h"   /* NUM_STYLES (tools/gen_data.py) */
extern const char *const STYLE_NAMES[NUM_STYLES];
extern const char *const STYLE_DESC[NUM_STYLES];   /* one screen line per style */

enum { CLS_DIATONIC, CLS_BORROWED, CLS_SECONDARY, CLS_SUBV, CLS_CHROMATIC };

typedef struct {
    int key, scale, style;
    float spice;               /* 0..1 */
    const chord_t *prev;       /* chord before the slot (NULL: start of the progression) */
    const chord_t *prev2;      /* the one before that (NULL) */
    const chord_t *next;       /* chord after the slot, when inserting/replacing (NULL) */
    const chord_t *loop_to;    /* first chord, when this slot closes the loop (NULL) */
    int artist;                /* artist profile laid over the style (profile index, 0 = none; artists.h) */
    int producer;              /* producer profile laid over both (profile index, 0 = none) */
    int mixed;                 /* 1: the three weights below are set (0: all 1.0, the default) */
    float wstyle, wartist, wprod;   /* STYLE / ARTIST / PRODUCER influence, 1.0 = normal (50 %), 0..2 */
} hctx_t;

typedef struct { chord_t c; float score; } cand_t;

#define MAX_CANDS 1024
typedef struct {
    cand_t all[MAX_CANDS];
    int n;
} cand_list_t;

/* every candidate scored (higher = better), sorted best first */
void harmony_score_all(const hctx_t *h, cand_list_t *out);
float harmony_score(const hctx_t *h, const chord_t *c);

/* Fill pool[0..n-1] with distinct, varied picks. seen: per-group flags (size HARMONY_GROUPS) of groups already
 * shown since the last pick (updated); rng: state. Returns how many were filled. */
#define HARMONY_GROUPS (12 * NUM_FAM * 13)
int harmony_pool(const hctx_t *h, chord_t *pool, int n, unsigned char *seen, uint32_t *rng, const chord_t *avoid, int navoid);

/* a whole progression of len chords (len <= 16) */
/* mix: STYLE / ARTIST / PRODUCER influence weights (NULL: all 1.0) */
void harmony_generate(int key, int scale, int style, int artist, int producer, const float *mix, float spice, int len,
                      chord_t *out, uint32_t *rng);

/* the next chord for a slot (CRUISE): the best-scoring chord after h->prev (and before h->next when filling a gap),
 * with the rest of the progression as context (used[0..nused-1]: avoid overusing chords already there) */
chord_t harmony_next(const hctx_t *h, const chord_t *used, int nused);

/* a pitch class moved from (okey, oscale) to (key, scale): scale degrees map to the same degree, other notes keep
 * their distance from the tonic */
int harmony_map_pc(int pc, int okey, int oscale, int key, int scale);
/* c re-fitted to the setting in h (key, scale, style, artist, producer, neighbours): its root moves with the key and
 * scale; recolour: its chord type is chosen anew for the new style / profile / scale, close to the old colour */
chord_t harmony_refit(const hctx_t *h, int okey, int oscale, const chord_t *c, int recolour);

int harmony_class(int key, int scale, const chord_t *c);
/* roman numeral text such as "vi", "V/V", "bVII", "subV/ii" */
void harmony_roman(int key, int scale, const chord_t *c, char *buf, int len);

uint32_t rng_next(uint32_t *s);
float rng_unit(uint32_t *s);
