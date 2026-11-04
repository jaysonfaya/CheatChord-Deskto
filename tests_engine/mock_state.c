/* the plugin's texts after a short session, for the mockup images (tools/mockup.py): reads keys on stdin, prints
 * "key<TAB>value" lines. usage: mock_state < keys.txt */
#include "engine.h"
#include "seq_out.h"
#include <stdio.h>
#include <string.h>
void cp_test_wait(void *inst);
static void sink(int c, int n, int v) { (void)c; (void)n; (void)v; }
static const mpc_engine_t *E; static void *S;
static double ppq; static int playing;
static void run(int ms) { int16_t b[256]; for (int i = 0; i < ms * 441 / 1280 + 1; i++) { E->song_pos(S, playing ? ppq : -1, 96, playing, 128); E->render(S, b, 128); if (playing) ppq += 128 * 96 / 60.0 / 44100.0; } }
static void set(const char *k, const char *v) { E->set_param(S, k, v); }
static void setw(const char *k, const char *v) { set(k, v); run(30); cp_test_wait(S); run(30); }
static void tap(const char *k) { set(k, "1"); run(450); set(k, "0"); run(100); cp_test_wait(S); run(20); }
int main(void) {
    seq_out_set_sink(sink);
    E = mpc_engine(); S = E->create(NULL);
    cp_test_wait(S); run(900);
    setw("midi_out", "1");
    tap("pre_1"); run(500);                 /* Velvet Neo Soul */
    setw("scope", "1");                     /* EDIT ALL */
    setw("loop_sync", "1");
    playing = 1; run(300);                  /* MPC plays */
    setw("producer_mix", "80");             /* turn the producer knob while it plays: the keys show the change */
    run(2100);
    setw("spice", "75"); run(2100);
    tap("prog_4");
    setw("scope", "0"); setw("slot_rev", "1"); setw("scope", "1");   /* chord 4 reversed */
    setw("strum_mode", "3"); setw("strum", "25"); setw("humanize", "35");
    playing = 0; run(50);
    setw("panel", "2");
    char k[128], v[300];
    while (fgets(k, sizeof k, stdin)) {
        k[strcspn(k, "\r\n")] = 0;
        if (!k[0]) continue;
        if (!strncmp(k, "#panel", 6)) { setw("panel", k + 6); continue; }
        v[0] = 0;
        E->get_param(S, k, v, sizeof v);
        for (char *c = v; *c; c++) if (*c == '\t' || *c == '\n') *c = ' ';
        printf("%s\t%s\n", k, v);
    }
    E->destroy(S);
}
