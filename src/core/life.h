/* SPDX-License-Identifier: MIT */
/* life.h — 自分の側の暮らし（core/life.c, DECISIONS #114） */
#ifndef CORE_LIFE_H
#define CORE_LIFE_H
#include "../types.h"
u16 life_busy(void);     /* いま戦に入れないわけ（テスト勉強・残業）の文。入れるなら 0 */
u16 life_ad(void);       /* 本体の時計が 25 日なら給料日の広告の文、ほかは 0 */
#endif
