/* SPDX-License-Identifier: MIT */
/* ending.h — サービス終了の結末（DECISIONS #89）。スタッフロールの前に、遊び方で変わる短い結末文と
 * 「最後まで残った仲間」を出す。結末は上から順に当てはまったもの:
 *   石油王（課金 50 万円以上）→ 図鑑の番人（カードの種類が半分以上）→ 最後のひとり（ギルドの仲間が全員去った）→
 *   ギルドの柱（対抗戦で勝ったことがある）→ 無課金の旅（課金 0 円）→ いつもの帰り道。
 * 仲間とその引退の日は伝言板と同じ式（board.h の mate_info）。保存するものは無い。 */
#ifndef CORE_ENDING_H
#define CORE_ENDING_H
#include "../types.h"

#define END_WHALE    0
#define END_COLLECT  1
#define END_ALONE    2
#define END_GUILD    3
#define END_FREE     4
#define END_NORMAL   5
#define ENDINGS      6

#define END_WHALE_YEN   500000UL
#define END_COLLECT_PCT 50
#define END_MATES_MAX   8
#define END_NEVER       0xFFFFu

typedef struct {
    u32 spent;                   /* 課金の合計（円） */
    u8  kinds;                   /* 持っているカードの種類 */
    u8  kinds_all;               /* カードの種類の総数 */
    u8  mates;                   /* ギルドの仲間 */
    u8  stayed;                  /* そのうち最後まで残った人 */
    u16 stay[END_MATES_MAX];     /* 残った人の NPC 番号 */
    u8  tone[END_MATES_MAX];     /* その口癖（伝言板と同じ, 0 = なし） */
    u16 first_leave;             /* 仲間が初めてギルドを去った日（END_NEVER = いない） */
    u8  gvg_won;                 /* 対抗戦で勝ったことがある */
    u8  kind;                    /* END_* */
} EndingStats;

void ending_stats(EndingStats *e);        /* g_save から集めて結末を決める */
u8   ending_kind(const EndingStats *e);   /* 集めた数字から結末を決める（ending_stats が呼ぶ。同じバンクから直接） */
#endif
