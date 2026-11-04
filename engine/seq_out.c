/* CheatChord desktop: no ALSA port. The engine sends its notes to the host (cc_set_midi_sink); these stubs remain for
 * the engine test, which routes notes to a callback. */
#include "seq_out.h"
#include <stddef.h>

static seq_out_sink_t g_sink;
static seq_out_cc_sink_t g_cc_sink;
int seq_out_acquire(void) { return 1; }
void seq_out_release(void) {}
void seq_out_note(int ch, int note, int vel) { if (g_sink) g_sink(ch, note, vel); }
void seq_out_cc(int ch, int cc, int value) {
    if (g_cc_sink) g_cc_sink(ch, cc, value);
    else if (g_sink) g_sink(ch, 128 + cc, value);
}
const char *seq_out_status(void) { return "MIDI OUT TO THE DAW"; }
void seq_out_set_sink(seq_out_sink_t s) { g_sink = s; }
void seq_out_set_cc_sink(seq_out_cc_sink_t s) { g_cc_sink = s; }
