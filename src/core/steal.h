/* SPDX-License-Identifier: MIT */
/* steal.h — 盗み・盗まれ（DECISIONS #90）。当時の「お宝を盗み合う」型の遊びを、カードで借りる。
 * 盗まれる: マイページに入るたび、前に数えた時刻からゲーム内 6 時間ごとに「盗まれたか」を決める（放置するほど盗まれる）。
 *   盗まれるのはデッキ（控えのデッキを含む）に入っていないカードだけ。2 枚を選んでレア度の高い方を持っていかれる。
 *   確率はギルドの仲間が残っているほど低い（仲間に守ってもらう）。仲間が去るほど盗まれやすくなる。
 *   盗んだのは自分と総戦力の近い人。盗まれたカードと盗んだ人は新しい順に 3 件まで覚える（g_save.stolen）。
 * 盗む: AP を払って、総戦力の近い 3 人（格下・同格・格上）のどれかからカードを 1 枚盗む。格上ほど良いカードを持つが難しい
 *   （格下は N、同格は R、格上は SR。まれに 1 つ上）。
 *   盗んだ人がまだ遊んでいれば、取り返しにも行ける（成功しやすく、元の Lv で戻る）。引退した人からは取り返せない。
 * 相手の顔ぶれは日付と盗みに行った回数で決まる（入り直しても同じ）。 */
#ifndef CORE_STEAL_H
#define CORE_STEAL_H
#include "../types.h"
#include "save.h"

#define STEAL_TARGETS 3
#define STEAL_ROWS    (STOLEN_MAX + STEAL_TARGETS)

#define STEAL_OK      0
#define STEAL_FAIL    1          /* 見つかって逃げられた（AP は払う） */
#define STEAL_NO_AP   2
#define STEAL_FULL    3          /* カードがいっぱい */

typedef struct {
    u16 npc;
    u16 card;
    u8  lv;
    u8  back;                    /* 取り返し: g_save.stolen の何件目 + 1（0 = 盗む） */
    u8  pct;                     /* 成功する確率（%） */
    u32 power;                   /* 相手の総戦力 */
} StealRow;

typedef struct {
    u8  n;
    u32 me;                      /* 自分の総戦力 */
    StealRow r[STEAL_ROWS];
} StealList;

void steal_list(StealList *l);           /* 取り返せる相手（先）と盗める相手を並べる */
u8   steal_try(StealList *l, u16 k);      /* l->r[k] へ盗みに行く。STEAL_*。l は呼ぶ前に steal_list で作る */
u8   steal_tick(void);                   /* 前に数えたときから今までに盗まれた枚数（マイページに入るたび） */
/* 形見分け（DECISIONS #94）: 去った仲間がカードを 1 枚置いていく（board.h の MF_GIVER）。受け取ったら
 * KEEP_NPC(v) にその仲間、KEEP_CARD(v) にカード。無ければ 0。1 回に 1 枚だけ（マイページに入るたび） */
u16  keep_tick(void);
#define KEEP_K(v)      ((u8)((v) >> 12))
#define KEEP_CARD(v)   ((u16)((v) & 0x0FFF))
#endif
