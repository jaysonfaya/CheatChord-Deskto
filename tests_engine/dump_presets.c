/* prints one line per factory preset: "<n>\t<name>\t<state>" (tools/make_xpl.py) */
#include "engine.h"
#include "seq_out.h"
#include <stdio.h>
#include <string.h>
void cp_test_wait(void *inst);
int cp_test_state(void *inst, char *buf, int len);
static void sink(int c, int n, int v) { (void)c; (void)n; (void)v; }
int main(void) {
    seq_out_set_sink(sink);
    const mpc_engine_t *E = mpc_engine();
    static char st[16384], nm[64], k[32];
    for (int i = 0; i < 64; i++) {
        void *S = E->create(NULL);
        int16_t b[256];
        for (int j = 0; j < 400; j++) E->render(S, b, 128);   /* the quiet time after create */
        cp_test_wait(S);
        snprintf(k, sizeof k, "preset:%d", i);
        E->get_param(S, k, nm, sizeof nm);
        snprintf(k, sizeof k, "pre_%d", i + 1);
        char v[8] = "";
        if (E->get_param(S, k, v, sizeof v) <= 1) { E->destroy(S); break; }   /* past the factory presets */
        E->set_param(S, k, "1");
        cp_test_wait(S);
        for (int j = 0; j < 40; j++) E->render(S, b, 128);
        cp_test_wait(S);
        cp_test_state(S, st, sizeof st);
        printf("%d\t%s\t%s\n", i, nm, st);
        E->destroy(S);
    }
    return 0;
}
