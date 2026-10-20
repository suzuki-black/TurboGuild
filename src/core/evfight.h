/* evfight.h — レイドとバトル（対人戦）の 1 回の戦い。討伐は quest.c の通常の流れで戦う。
 * レイド: ボスの HP は倒すまで残る。1 回の攻撃は最大 RAID_TURNS ターン。倒すと Lv が上がって次のボス。
 *   救援依頼（ボス 1 体に 1 回）で同じギルドの NPC が削ってくれる。
 * バトル: 自分の総戦力（特効込み）に揺らぎを掛けて、相手（NPC）の戦力と比べる。 */
#ifndef CORE_EVFIGHT_H
#define CORE_EVFIGHT_H
#include "../types.h"
#include "quest.h"

#define RAID_TURNS     5        /* 1 回の攻撃で殴るターン数 */
#define RAID_FULL_MUL  3        /* 全力攻撃は攻撃 3 倍（AP も 3 倍） */
#define RESCUE_MAX     3        /* 救援に来てくれる仲間の数 */
#define PVP_TIERS      3

typedef struct {
    u8  n;                      /* 来てくれた人数（0 = もう依頼済み・レイドでない） */
    u16 npc[RESCUE_MAX];
    u32 dmg[RESCUE_MAX];
    u8  kill;                   /* 仲間の攻撃で倒した */
    u16 points;                 /* 倒したときに自分へ入るポイント */
    RewardList gain;
} RescueResult;

typedef struct { u16 npc; u32 power; } PvpFoe;

void event_fight(u16 level, QuestResult *r);   /* AP を払った後に quest_run から呼ぶ */
void raid_sync(void);                          /* 参加中のイベントのボスにそろえる */
u32  raid_max_hp(u16 lv);
void raid_rescue(RescueResult *r);
void pvp_foe(u16 tier, PvpFoe *f);             /* tier 1..3 の相手 */
#endif
