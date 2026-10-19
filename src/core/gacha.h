/* gacha.h — ガチャ（仕様 §4.3）。抽選は rand(10000) と累積閾値、小数は使わない */
#ifndef CORE_GACHA_H
#define CORE_GACHA_H
#include "../types.h"

#define GACHA_ONE_MAX 10

#define G_OK          0
#define G_NO_MONEY    1
#define G_FREE_USED   2     /* 無料は 1 ゲーム日 1 回 */
#define G_FULL        3     /* 所持カードが満杯 */
#define G_UNAVAILABLE 4     /* 10 連が無いガチャ、開催していないイベント */

typedef struct {
    u8  gacha;          /* 入力: gacha.csv の id */
    u8  count;          /* 入力: 1 か 10 */
    u8  status;
    u8  got;            /* 実際に引けた枚数（満杯なら途中で止まる） */
    u8  pity_hit;       /* 天井で SSR を確定させた */
    u16 ids[GACHA_ONE_MAX];
} GachaReq;

void gacha_pull(GachaReq *g);     /* 支払い → 抽選 → 所持に追加 */
void gacha_roll(GachaReq *g);     /* 抽選だけ（最初の 10 連・リセマラ用。支払いも追加もしない） */
u8   gacha_can_pay(GachaReq *g);  /* G_OK / G_NO_MONEY / G_FREE_USED / G_UNAVAILABLE */
void gacha_box_sync(u8 gacha);    /* BOX ガチャの箱を開催中のイベントにそろえる（g_save.box_*） */
#endif
