/* SPDX-License-Identifier: MIT */
/* title.h — 称号（DECISIONS #107）。自分の称号はセーブにある数字（思い出・課金・Lv）から毎回決めるので、
 * 持っている称号は保存しない。保存するのは「どれをつけているか」だけ（g_save.title_sel）。
 * 計算は core/mates.c（バンク 100）。仲間の称号は NPC 番号・課金コース・Lv のハッシュ。並びはおおよそ取りにくい順（おまかせは持っている中でいちばん下の行）。 */
#ifndef CORE_TITLE_H
#define CORE_TITLE_H
#include "../types.h"

#define TITLE_COUNT 16
#define TT_ROOKIE    0    /* かけだし冒険者（はじめから） */
#define TT_GUILD     1    /* ギルドの一員 */
#define TT_STOLEN    2    /* 盗まれ上手 */
#define TT_LIGHT     3    /* 微課金戦士（課金した） */
#define TT_LV50      4
#define TT_EVOLVE    5
#define TT_STAGE100  6
#define TT_RIVAL     7
#define TT_FREE      8    /* 無課金の星（課金 0 円のまま 60 日目・Lv30） */
#define TT_RAID      9
#define TT_GVG       10
#define TT_HEAVY     11   /* 重課金者（3 万円） */
#define TT_TOP100    12
#define TT_OIL       13   /* 石油王（50 万円。結末と同じ） */
#define TT_TOP10     14
#define TT_LASTDAY   15   /* 最後の日まで */
#define TT_FREE_DAY  60
#define TT_FREE_LV   30
#define TT_HEAVY_YEN 30000UL
#define TT_OIL_YEN   500000UL

u16 title_earned(void);          /* 持っている称号のビット */
u8  title_now(void);             /* つけている称号（おまかせなら持っている中でいちばん下の行） */
u8  title_npc(u16 npc, u16 level); /* 仲間の称号（課金コースは番号から。core/mates.c） */
#endif
