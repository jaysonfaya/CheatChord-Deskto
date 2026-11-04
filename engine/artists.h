/* Chord Pilot artist / producer profiles: each one is laid over the chosen STYLE and shifts the harmony model
 * towards that writer's typical chords, degrees and moves. Profiles 1-25 are hand-written in src/artists.c; 26 on (4.0) come from tools/cc_data.py, like the
 * screen lines and the alphabetical ARTIST / PRODUCER lists (tools/gen_data.py writes src/gen/profiles_gen.inc). */
#pragma once
#include "style_types.h"

#include "gen/counts.h"   /* NUM_ARTISTS, NUM_ARTIST_LIST, NUM_PRODUCER_LIST (tools/gen_data.py) */
typedef struct {
    const char *name;    /* list label */
    int style;           /* the style it is usually heard in (picked when the artist is chosen) */
    const char *traits;  /* one line for the screen: what it does */
    const char *why;     /* one line: what that is based on */
    int kind;            /* 1 artist, 2 producer */
} artist_info_t;

extern const style_t ARTISTS[NUM_ARTISTS];
extern const artist_info_t ARTIST_INFO[NUM_ARTISTS];
/* the ARTIST and PRODUCER browser lists: option index -> profile index (0 = none) */
extern const int ARTIST_LIST[NUM_ARTIST_LIST];
extern const int PRODUCER_LIST[NUM_PRODUCER_LIST];
/* the lists of 3.x (projects saved before 4.0 store an index into these) */
#define NUM_OLD_ARTIST_LIST 14
#define NUM_OLD_PRODUCER_LIST 13
extern const int OLD_ARTIST_LIST[NUM_OLD_ARTIST_LIST];
extern const int OLD_PRODUCER_LIST[NUM_OLD_PRODUCER_LIST];
int artist_list_index(int profile);     /* option index in ARTIST_LIST of a profile (0 when absent) */
int producer_list_index(int profile);
