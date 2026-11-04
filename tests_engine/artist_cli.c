/* how the artist profiles differ: 3 generated 4-chord progressions each, C major / C minor, spice 40 */
#include "harmony.h"
#include "artists.h"
#include <stdio.h>
int main(void) {
    for (int a = 0; a < NUM_ARTISTS; a++) {
        int st = ARTIST_INFO[a].style < 0 ? 0 : ARTIST_INFO[a].style;
        for (int sc = 0; sc < 2; sc++) {
            printf("%-11s %-6s ", ARTIST_INFO[a].name, sc ? "minor" : "major");
            uint32_t r = 1000 + a * 31 + sc;
            for (int k = 0; k < 3; k++) {
                chord_t g[4];
                harmony_generate(0, sc, st, a, 0, NULL, 0.4f, 4, g, &r);
                for (int i = 0; i < 4; i++) { char nm[24]; chord_name_key(&g[i], 0, sc, nm, 24); printf("%-9s", nm); }
                printf("| ");
            }
            printf("\n");
        }
    }
}
