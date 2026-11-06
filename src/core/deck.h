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
    u8  stance;          /* 入力: STANCE_*（攻撃に掛ける。総戦力・おまかせでは 0 = 通常） */
    u8  lead_elem;       /* 出力: リーダーの属性（同じ属性の攻撃 +LEADER_PCT%）。いなければ ELEM_NONE */
} DeckStats;

#define ELEM_NONE 0xFF

void deck_calc(DeckStats *st);
void deck_stats_init(DeckStats *st, u16 enemy_elem);  /* 特効なしで初期化（u16 は中継のレジスタ渡しの都合） */
void deck_auto(void);                 /* コスト上限内で攻撃の高い順に詰める（おまかせ編成） */
void deck_auto_for(const DeckStats *st); /* 同じく、st の敵の属性・特効込みの攻撃で詰める */
u8   elem_weak(u8 enemy);             /* enemy に有利を取れる属性（無ければ ELEM_NONE） */
u32  card_atk_of(u16 index);          /* 所持カード 1 枚の攻撃（Lv 補正・スキル込み, 属性補正なし） */
u32  card_def_of(u16 index);
u8   elem_pct(u8 mine, u8 enemy);     /* 属性補正 % */
void deck_switch(u8 slot);            /* 控えのデッキ slot に替える（いまのデッキは控えに残す） */
u8   deck_slot_count(u8 slot);        /* そのデッキの枚数（いま使っているものも数えられる） */
#endif
