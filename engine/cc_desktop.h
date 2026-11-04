/* CheatChord desktop (VST3 / AU / Standalone): what the plugin shell needs beyond the MPC engine interface (engine.h).
 * Every call here, like every mpc_engine_t call, must be serialised per instance by the caller (one lock). */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* MIDI the engine sends (the MPC build's ALSA port): offset = frames into the current render()/song_pos() block. */
typedef void (*cc_midi_sink_t)(void *ctx, int offset, uint8_t status, uint8_t d1, uint8_t d2);
void cc_set_midi_sink(void *inst, cc_midi_sink_t fn, void *ctx);

/* the folder for user presets and MIDI exports (UTF-8), e.g. Documents/JJ DA PLUG/CheatChord. Set before create(). */
void cc_set_docs_root(const char *utf8_dir);
const char *cc_docs_root(void);

/* the host's sample rate (any; default 44100). Stops what sounds. */
void cc_set_sample_rate(void *inst, double sr);

/* write the progression as a MIDI file now (for drag and drop into the DAW); 1 ok. msg: what happened */
int cc_write_midi(void *inst, const char *path_utf8, char *msg, int msglen);

/* small facts the desktop screen draws itself */
int cc_count(void *inst);            /* chords in the progression */
int cc_loop_len(void *inst);         /* 4 or 8 */
int cc_play_slot(void *inst);        /* the chord that sounds now, -1 */
int cc_selected_idea(void *inst);    /* -1 */
int cc_cursor(void *inst);
int cc_target(void *inst);
int cc_busy(void *inst);             /* a worker job is running or waiting */
double cc_bpm(void *inst);
/* the notes of the shown chord (bass first if any): returns how many, -1 none */
int cc_shown_notes(void *inst, int *notes, int max, int *bass);

/* one block (1..128 frames) in float, without the int16 step of render() */
void cc_render_float(void *inst, float *l, float *r, int frames);

int cc_kb_lo(void *inst);                     /* the lowest note of the 72-key keyboard (kbx_1) */
int cc_idea_class(void *inst, int i);         /* 0 in key, 1 borrowed, 2 leads to next, 3 substitute, 4 spicy; -1 */
int cc_slot_class(void *inst, int i);
int cc_slot_roman(void *inst, int i, char *buf, int len);

/* tests */
int cp_test_count(void *inst);
int cp_test_pool(void *inst);
void cp_test_wait(void *inst);

#ifdef __cplusplus
}
#endif
