/* CheatChord MIDI out: MPC OS ignores a VST2 plugin's own MIDI output, so the notes leave through an ALSA
 * sequencer port of our own ("CheatChord" / "MIDI Out"). MPC picks it up like a new MIDI device: enable it for
 * tracks in Preferences > MIDI, then choose it as the MIDI input of any track that should play the chords. */
#pragma once

int seq_out_acquire(void);                 /* first instance opens the port; returns 1 when output works */
void seq_out_release(void);
void seq_out_note(int ch, int note, int vel);   /* vel 0 = note off; ch 0..15 */
void seq_out_cc(int ch, int cc, int value);     /* a controller (4.1: CC 11 expression for reversed chords) */
const char *seq_out_status(void);

/* tests: route every note to a callback instead of ALSA */
typedef void (*seq_out_sink_t)(int ch, int note, int vel);
void seq_out_set_sink(seq_out_sink_t s);
/* tests: controllers go to the note sink as note 128 + cc, vel = value (when no cc sink is set) */
typedef void (*seq_out_cc_sink_t)(int ch, int cc, int value);
void seq_out_set_cc_sink(seq_out_cc_sink_t s);
