/* ranking.h — ランキング（仕様 §8）。プレイヤー以外は NPC 1000 人。
 * Phase2 の NPC は静的データ: イベントポイントは「最終値 × 開催の経過割合」で伸ばし、総戦力は固定。
 * 表は大きい順に並べてあるので、順位は二分探索で数える（1000 件をソートしない）。 */
#ifndef CORE_RANKING_H
#define CORE_RANKING_H
#include "../types.h"

#define RANK_EVENT 0
#define RANK_POWER 1
#define RANK_ROWS  10
#define RANK_PLAYER 0xFFFFu

typedef struct { u16 npc; u32 value; u16 rank; } RankRow;

typedef struct {
    u8  mode;              /* 入力: RANK_EVENT / RANK_POWER */
    u16 first;             /* 入力: 表示する先頭の順位（1 始まり）。0 なら自分の前後 */
    u16 player_rank;       /* 以下が出力 */
    u32 player_value;
    u16 total;
    u8  n;
    RankRow rows[RANK_ROWS];
} RankList;

void ranking_list(RankList *l);
u16  ranking_final_rank(u32 points);     /* イベント終了時の順位（NPC は最終値）。★CORE2 バンクの中からだけ呼ぶ */
u32  player_power(void);                 /* 総戦力 = デッキの攻撃 + 防御（属性補正なし） */
#endif
