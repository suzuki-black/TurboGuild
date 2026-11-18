/* SPDX-License-Identifier: MIT */
/* trade.h — トレード掲示板（core/trade.c, DECISIONS #115） */
#ifndef CORE_TRADE_H
#define CORE_TRADE_H
#include "../types.h"

#define TRADE_ROWS     5
#define TRADE_SELL     0        /* 売ります: NPC の出品を石で買う */
#define TRADE_WANT     1        /* 買います: NPC が求めるカードを手放して石を受け取る */
#define TRADE_SLOT     21600UL  /* 6 時間ごとに入れ替わる */
#define TRADE_AUDIT_DAY 135     /* この日から出品の審査が始まり、詐欺が減る */
#define TRADE_SEQ()    ((u8)((g_save.flags >> 2) & 3))   /* 売り買いのたびに入れ替わる（save.h の flags の 2〜3 ビット） */

typedef struct {
    u16 npc;
    u16 card;
    u8  rarity;
    u8  cheap;      /* 相場の 3 割の「格安」 */
    u8  scam;       /* 払っても届かない */
    u32 price;      /* 石 */
} TradeRow;

u8   trade_row(u16 tk, TradeRow *o);         /* TRADE_TK(tab, k) の行。サービス終了後は 0 */
#define TRADE_TK(tab, k) ((u16)(((tab) << 8) | (k)))
u32  trade_market(u8 rarity);                 /* いまの相場（石） */
void trade_next(void);                         /* 売り買いの後: 掲示板を入れ替える */
#endif
