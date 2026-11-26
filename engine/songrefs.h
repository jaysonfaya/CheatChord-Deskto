/* CheatChord desktop: song references (real songs' chord loops, from MIDI CONDUCTA 3.3, see tools/song_refs.py) */
#pragma once
typedef struct { signed char deg, q, bass, len; } rchord_t;   /* deg/bass: semitones above the tonic (-1 = root); len 0/1/2 = Half/Normal/Double */
typedef struct {
    const char *artist, *title, *roman;
    unsigned char minor;
    unsigned long long styles;   /* bit i = STYLE i fits */
    unsigned char w, rich, bars, kind, n;   /* w: weight; rich: % rich chords; bars: CHORD LENGTH index; kind 0 song, 1 library */
    rchord_t c[8];
} songref_t;
#ifdef __cplusplus
extern "C" {
#endif
extern const songref_t SONGREFS[];
extern const int NUM_SONGREFS;
#ifdef __cplusplus
}
#endif
