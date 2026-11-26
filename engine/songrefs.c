#include "songrefs.h"
const songref_t SONGREFS[] = {
#include "gen/songrefs_gen.inc"
};
const int NUM_SONGREFS = (int)(sizeof SONGREFS / sizeof SONGREFS[0]);
