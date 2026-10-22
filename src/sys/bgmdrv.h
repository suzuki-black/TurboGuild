/* SPDX-License-Identifier: MIT */
/* bgmdrv.h — BGM の割り込みドライバ（カーネル）。曲の開始・停止は ui/bgm.c（バンク）から */
#ifndef SYS_BGMDRV_H
#define SYS_BGMDRV_H
#include "../types.h"

void opll_w(u8 reg, u8 val);
void bgm_isr(void);
void bgm_silence(void);
void bgm_install(void);     /* 起動時に 1 回: H.TIMI へつなぐ */
void bgm_hold(void);        /* ディスクの前 */
void bgm_release(void);     /* ディスクの後 */
void bgm_clock_reset(void); /* 曲の始めに時計を合わせる */
#endif
