/* SPDX-License-Identifier: MIT */
/* battle.h — 戦闘は式 1 本で決める（仕様 §6）。演出は ui 側 */
#ifndef CORE_BATTLE_H
#define CORE_BATTLE_H
#include "../types.h"

typedef struct {
    u32 deck_atk, deck_def;
    u16 player_hp;
    u32 enemy_hp;
    u16 enemy_def, enemy_atk;
} BattleIn;

typedef struct {
    u8  win;
    u16 turns;
    u32 dmg_per_turn;      /* 敵へ */
    u32 taken_per_turn;    /* 自分へ */
    u32 taken_total;
} BattleOut;

void battle_run(const BattleIn *in, BattleOut *out);
#endif
