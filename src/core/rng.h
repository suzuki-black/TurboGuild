/* SPDX-License-Identifier: MIT */
/* rng.h — xorshift32。系列ごとに状態を持つ（NPC 用とガチャ用を分ける, 仕様 §2） */
#ifndef CORE_RNG_H
#define CORE_RNG_H
#include "../types.h"
u32 rng_next(u32 *state);
u16 rng_range(u32 *state, u16 n);   /* 0..n-1 */
#endif
