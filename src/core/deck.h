/* SPDX-License-Identifier: MIT */
/* deck.h — デッキの総攻撃・総防御・コスト・スキル効果（仕様 §6） */
#ifndef CORE_DECK_H
#define CORE_DECK_H
#include "../types.h"
#include "../data/cards.h"

typedef struct {
    u8  enemy_elem;      /* 入力: 敵の属性（属性補正に使う）。0xFF なら補正なし */
    u16 bonus_card;      /* 入力: イベントの特効カード（0 = なし）。攻撃 × bonus_mul */
    u16 evo_card;        /* 入力: その進化後。攻撃 × evo_mul */
    u8  bonus_mul, evo_mul;
    u32 atk, def;
    u16 cost;
    u8  count;           /* 編成されている枚数 */
    u8  ap_minus;        /* スキル 3（AP 消費-1）を持つカードがいれば 1 */
    u8  drop_bonus;      /* スキル 4 の枚数 × 10（%） */
} DeckStats;

#define ELEM_NONE 0xFF

void deck_calc(DeckStats *st);
void deck_stats_init(DeckStats *st, u16 enemy_elem);  /* 特効なしで初期化（u16 は中継のレジスタ渡しの都合） */
void deck_auto(void);                 /* コスト上限内で攻撃の高い順に詰める（おまかせ編成） */
u32  card_atk_of(u16 index);          /* 所持カード 1 枚の攻撃（Lv 補正・スキル込み, 属性補正なし） */
u32  card_def_of(u16 index);
u8   elem_pct(u8 mine, u8 enemy);     /* 属性補正 % */
#endif
