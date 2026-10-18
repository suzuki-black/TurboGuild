/* rng.c — xorshift32 (13,17,5)。状態 0 は不動点なので 1 に逃がす */
#include "rng.h"

u32 rng_next(u32 *state)
{
    u32 x = *state;
    if (x == 0) x = 1;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

u16 rng_range(u32 *state, u16 n)
{
    return (u16)((u16)(rng_next(state) >> 16) % n);
}
