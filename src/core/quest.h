/* SPDX-License-Identifier: MIT */
/* quest.h — 1 ステージ分のクエスト実行（AP 消費 → 戦闘 → 報酬 → ドロップ → 進行） */
#ifndef CORE_QUEST_H
#define CORE_QUEST_H
#include "../types.h"
#include "battle.h"
#include "event.h"

#define Q_OK      0
#define Q_LOCKED  1
#define Q_NO_AP   2

typedef struct {
    u8  status;
    u8  ap_used;
    u8  enemy_elem;
    u16 player_hp;
    BattleOut b;
    u16 exp, gold;
    u8  levels;          /* 上がったレベル数 */
    u16 drop_card;       /* 0 = なし */
    u8  drop_lost;       /* 満杯で受け取れなかった */
    u8  area_cleared;    /* このボスを初めて倒した */
    u8  event;           /* イベントの面だった */
    u16 points;          /* イベントポイント */
    RewardList gain;     /* 届いた報酬ライン */
    u8  kind;            /* イベントの型（EV_*）。通常の面は EV_HUNT */
    u8  raid_lv;         /* レイド: 戦ったボスの Lv */
    u32 raid_before;     /* レイド: 戦う前の残り HP */
    u32 raid_max;        /* レイド: そのボスの最大 HP / バトル: 相手の HP（見せ方） */
    u16 foe;             /* バトル: 相手の NPC */
} QuestResult;

void quest_run(u16 stage, QuestResult *r);     /* stage >= EVENT_STAGE_BASE ならイベントの面 */
u16  quest_max_stage(void);          /* 挑戦できる最大のステージ番号 */
u8   quest_ap_cost(u16 stage);       /* スキル込みの AP 消費 */
#endif
