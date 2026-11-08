/* SPDX-License-Identifier: MIT */
/* pev.h — 自分の最近の出来事（DECISIONS #91）。伝言板の仲間が、その後の何回かの書き込みで反応する
 * （「$さん SSR おめでとう！」）。新しい順に PEV_MAX 件だけ覚える（g_save.pev）。
 * どのバンクからも呼べるようカーネルに置く（core/ のうち MEMORY_MAP に無いものはカーネル）。 */
#ifndef CORE_PEV_H
#define CORE_PEV_H
#include "../types.h"

#define PEV_NONE    0
#define PEV_SSR     1    /* SSR 以上を手に入れた（param: カード id） */
#define PEV_STOLEN  2    /* カードを盗まれた（param: 盗んだ人） */
#define PEV_RIVAL   3    /* ライバルを抜いた（param: その人） */
#define PEV_BACK    4    /* 久しぶりに来た（param: 空いた日数） */
#define PEV_GVGWIN  5    /* 対抗戦の戦で勝った */
#define PEV_LEVEL   6    /* Lv が 10 の倍数になった（param: Lv） */
#define PEV_EVTOP   7    /* イベントで 100 位以内（param: 順位） */
#define PEV_INN     8    /* 宿屋に泊まった */
#define PEV_KINDS   9

void pev_push(u8 kind, u16 param);
#endif
