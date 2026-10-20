/* event.h — 期間限定イベント（仕様 §4.4）。kind=0 は周回ボス型（初級・中級・上級の 3 面）。
 * 開催はゲーム内の日付で決まる（events.csv の start_day から duration 日）。参加・終了・報酬は自動。 */
#ifndef CORE_EVENT_H
#define CORE_EVENT_H
#include "../types.h"

#define EVENT_STAGE_BASE 0xFF00u     /* quest_run(EVENT_STAGE_BASE + 面) でイベントの面 */
#define EVENT_LEVELS     3
#define REWARD_MAX       4

/* イベントの型（events.csv の kind, DECISIONS #63） */
#define EV_HUNT          0           /* 討伐: 初級・中級・上級の周回 */
#define EV_RAID          1           /* レイド: 倒すまで HP が残るボス。1 = 通常攻撃 2 = 全力攻撃 */
#define EV_PVP           2           /* バトル: 1 = 格下 2 = 同格 3 = 格上 の相手と戦う */

typedef struct { u8 n; u8 item[REWARD_MAX]; u16 amount[REWARD_MAX]; } RewardList;

typedef struct {
    u8  started;          /* このたび参加した */
    u8  ended;            /* このたび終わった（最終順位と報酬が入る） */
    u16 id;
    u16 rank;
    u32 points;
    RewardList gain;
} EventResult;

/* クエストへ渡すイベントの面の値（quest.c が使う） */
typedef struct {
    u8  ap;
    u32 hp;
    u16 def, atk, exp, gold, points;
    u8  element;
    u16 boss_card, bonus_card, evo_card;
    u8  bonus_mul, evo_mul;
    u8  kind;             /* EV_* */
} EventStage;

u16  event_active(void);                          /* いま開催中のイベント id（無ければ 0） */
u8   event_kind(void);                            /* 参加中のイベントの型（EV_*） */
u16  event_days_left(void);                       /* 開催中なら残りゲーム日数 */
void event_update(EventResult *r);                /* 参加と終了の処理（マイページに入るたび） */
u8   event_stage(u16 level, EventStage *s);       /* level 1..（型ごとの面の数）。無ければ 0 */
void event_add_points(u16 pts, RewardList *g);    /* ポイントを足し、届いた報酬ラインを受け取る */
u32  event_next_line(void);                       /* 次の報酬ラインのポイント（無ければ 0） */
#endif
