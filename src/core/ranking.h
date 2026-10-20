/* SPDX-License-Identifier: MIT */
/* ranking.h — NPC 1000 人の「いま」と、ランキング・流れるお知らせ（仕様 §8）。
 * NPC の状態は保存しない。npc.dat の初期値とゲーム時刻から毎回同じ答えを計算する（閉じた式, ループしない）:
 *   Lv・総戦力は日数 × 活動度で伸び、年表の PLAYER_DROP で一部が辞める（辞めた日で止まる）。
 *   イベントポイントは「その人のそのイベントでの腕前 × 開催の経過割合」。
 * ランキングは 1000 人を 1 回なめて、自分より上の人数・上位 10 人・自分の前後を同時に拾う（O(n)）。 */
#ifndef CORE_RANKING_H
#define CORE_RANKING_H
#include "../types.h"

#define RANK_EVENT 0
#define RANK_POWER 1
#define RANK_ROWS  10
#define RANK_PLAYER 0xFFFFu
#define RANK_VIEW_AROUND 0
#define RANK_VIEW_TOP    1

typedef struct { u16 npc; u32 value; u16 rank; } RankRow;

typedef struct {
    u8  mode;              /* 入力: RANK_EVENT / RANK_POWER */
    u16 first;             /* 入力: RANK_VIEW_AROUND（自分の前後）/ RANK_VIEW_TOP（上位 10） */
    u16 player_rank;       /* 以下が出力 */
    u32 player_value;
    u16 total;             /* 参加者（辞めた NPC は数えない）+ 自分 */
    u8  n;
    RankRow rows[RANK_ROWS];
} RankList;

typedef struct {
    u16 level;
    u32 power;
    u8  retired;
} NpcNow;

#define FEED_SSR     0
#define FEED_BOSS    1
#define FEED_LEVEL   2
#define FEED_RANKUP  3
#define FEED_LOGIN   4
#define FEED_KINDS   5
#define FEED_BYE     5         /* サービス最終日: 別れの挨拶（FEED_KINDS の外） */
#define FEED_SLOT_SECS 1800UL     /* ゲーム内 30 分に 1 件 */

typedef struct { u16 npc; u8 kind; u16 param; } FeedLine;   /* param: カード id / Lv / 順位 */

void ranking_list(RankList *l);
u16  ranking_final_rank(void);           /* イベント終了時の順位（g_save.event_pt で数える） */
u32  player_power(void);                 /* 総戦力 = デッキの攻撃 + 防御（属性補正なし） */
void npc_now(u16 index, NpcNow *n);
void feed_line(u16 k, FeedLine *f);      /* k 番目に新しい出来事（0 = 最新） */
u16  npc_active_count(void);
#endif
