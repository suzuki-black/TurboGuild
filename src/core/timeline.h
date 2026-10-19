/* timeline.h — 運営年表（仕様 §3.1）。ゲーム内の日付で「いま何が起きているか」を表から読む。
 * 状態は保存しない（日付が決まれば毎回同じ答え）。贈り物だけは 1 回きりなので timeline_cursor で数える。 */
#ifndef CORE_TIMELINE_H
#define CORE_TIMELINE_H
#include "../types.h"

#define RATEUP_DAYS 7

typedef struct {
    u16 day;             /* いまのゲーム内日数 */
    u16 rateup_pct;      /* 100 = 通常。キャンペーン中は 200 / 300 */
    u16 era_pct;         /* いまの時代のインフレ倍率 */
    u8  drop_pct;        /* NPC が辞めた割合（%、累計） */
    u8  announce;        /* サービス終了の告知が出ている */
    u8  ended;           /* サービス終了 */
    u16 days_to_end;
} TlState;

typedef struct { u8 n_news; u16 gift_gems; } TlResult;

void timeline_state(TlState *s);
void timeline_update(TlResult *r);       /* マイページに入るたび。新しく来た行を数え、贈り物を配る */
u8   timeline_news(u16 k, u16 *text_id);  /* k 番目に新しいお知らせ（0 = 最新）。無ければ 0 */
u16  timeline_drop_day(u8 nth);          /* nth 回目の PLAYER_DROP の日（無ければ 0xFFFF） */
#endif
