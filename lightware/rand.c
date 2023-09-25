#include "internal.h"

uint32_t lw_xorShift32(uint32_t *state) {
    // algorithm by George Marsaglia from https://en.wikipedia.org/wiki/Xorshift
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}