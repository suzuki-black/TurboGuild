/* SPDX-License-Identifier: MIT */
/* player.h — レベル・経験値・AP（仕様 §3, 企画書「スタミナ」） */
#ifndef CORE_PLAYER_H
#define CORE_PLAYER_H
#include "../types.h"
#include "../config.h"

#define PLAYER_HP(lv)       ((u16)(PLAYER_HP_BASE + PLAYER_HP_PER_LV * (u16)(lv)))
#define PLAYER_COST_MAX(lv) ((u16)(DECK_COST_BASE + DECK_COST_PER_LV * (u16)(lv)))
#define EXP_NEED(lv)        ((u16)(EXP_NEED_BASE + EXP_NEED_K * (u16)(lv) * (u16)(lv)))

void player_ap_update(void);      /* g_time までの自然回復を反映 */
u8   player_add_exp(u16 exp);     /* 上がったレベル数。上がったら AP 全回復 */
u8   player_use_potion(void);     /* 0 = 回復薬が無い */
typedef struct {
    u32 at;              /* 次の予定の時刻（g_time） */
    u8  kind;            /* PLAN_* */
    u16 text;            /* イベントの名前（PLAN_EVENT） */
    u8  match;           /* 対抗戦の第何戦か（0 始まり。偶数が昼 12:00、奇数が夜 21:00） */
} Plan;
#define PLAN_NONE  0
#define PLAN_GVG   1     /* ギルド対抗戦の戦の始まり */
#define PLAN_EVENT 2     /* イベントの始まり */
void plan_next(Plan *p);                  /* いまより後でいちばん近い予定（年表とイベントの表から） */
u8   player_wait(Plan *p);                /* 宿屋で次の予定まで休む（無料。AP は進んだぶんだけ自然に回復）。予定が無ければ 0 */
u8   player_sleep(void);          /* 宿屋に泊まる: 翌朝 6 時まで進めて AP 全回復。宿代が足りなければ 0、泊まれば寝た時間（時） */
void player_add_item(u16 item, u16 amount);  /* ITEM_*。所持数の上限は 99 */
#endif
