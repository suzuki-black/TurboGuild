/* SPDX-License-Identifier: MIT */
/* time.h — 現実時刻とゲーム内時刻 g_time の換算（仕様 §3）。ハード非依存 */
#ifndef CORE_TIME_H
#define CORE_TIME_H
#include "../types.h"
#include "../config.h"

typedef struct { u16 year; u8 mon, day, hour, min, sec; } DateTime;

#define TIME_OK        0
#define TIME_BACKWARD  1   /* 時刻が逆行した */
#define TIME_CLAMPED   2   /* オフライン上限で切り捨てた */

u32 time_to_secs(const DateTime *dt);       /* 1980-01-01 00:00:00 からの秒 */
u32 time_advance(u32 gtime, u32 real_last, u32 real_now, u8 *status);
#define time_gday(t) ((u32)(t) / GSEC_PER_DAY)
#endif
