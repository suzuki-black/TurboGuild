/* SPDX-License-Identifier: MIT */
/* thread.h — 攻略スレ（DECISIONS #104, 提案 17）。2ch 風の掲示板。ゲーム内 30 分に 1 レス、1000 レスで次の Part。
 * どのレスも番号のハッシュと年表・イベントの表だけで決まる（保存しない）。
 *   ボーダー予想: 10 位・100 位・500 位の点。正解（イベント終了時のその順位の点, data/border.h）の 85〜115% を言うが、
 *                 1/4 はデマ（4〜6 割か、1.6〜2.2 倍）。デマには何レスか後に「>>N デマ乙」が付くことがある
 *   特効・次イベ: いまのイベントの特効カード、イベントの無い時期は次のイベントの特効。1/4 は関係ないカード（デマ）
 *   返事:         「>>N それな」「ソースは？」「釣り乙」
 *   雑談:         年表で替わる（USR・XUSR・終了の告知の後）
 * 正解の点は ROM の表（UI が持つ）から引くので、ここでは「正解の何 % を言ったか」だけ返す。 */
#ifndef CORE_THREAD_H
#define CORE_THREAD_H
#include "../types.h"

#define TH_SECS      1800UL      /* ゲーム内 30 分に 1 レス */
#define TH_PART      1000        /* 1000 レスで次のスレ */
#define TH_TIERS     3           /* ボーダーの順位: 10 位・100 位・500 位 */

#define TH_CHAT      0
#define TH_BORDER    1
#define TH_DEMA      2           /* 前のデマのボーダーへの突っ込み */
#define TH_BOON      3           /* いまのイベントの特効 */
#define TH_NEXT      4           /* 次のイベントの特効 */
#define TH_REPLY     5

typedef struct {
    u16 no;          /* レス番（1..1000） */
    u16 id;          /* ID の元（4 文字にする） */
    u16 text;        /* 文（% にカード名か点を入れる） */
    u16 card;        /* TH_BOON / TH_NEXT のカード */
    u16 ref;         /* >>N（0 = なし） */
    u16 real;        /* TH_BOON / TH_NEXT: 本当の特効カード（運営モードの答え合わせ） */
    u8  kind;
    u8  ev;          /* TH_BORDER: イベント（1..EVENT_COUNT） */
    u8  tier;        /* TH_BORDER: 0 = 10 位・1 = 100 位・2 = 500 位 */
    u8  pct;         /* TH_BORDER: 正解の何 % を言ったか */
    u8  lie;         /* デマ（TH_BORDER・TH_BOON・TH_NEXT） */
} ThPost;

u16  thread_last(void);                 /* いちばん新しいレスの通し番号（ゲーム内の時刻から。終了の日で止まる） */
void thread_post(u16 p, ThPost *o);     /* 通し番号 p のレス */
u8   thread_event_at(u16 day);          /* その日に開催中のイベント（無ければ 0） */
void ops_thread(void);                  /* 運営モード（ui/thread_ui.c）: その日のスレを、模型の様子と答え合わせつきで見せる */
#endif
