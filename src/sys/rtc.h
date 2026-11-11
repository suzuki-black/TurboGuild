/* SPDX-License-Identifier: MIT */
/* rtc.h — 内蔵 RTC の現在時刻 */
#ifndef RTC_H
#define RTC_H
#include "../types.h"
#include "../core/time.h"
void rtc_read(DateTime *dt);
u32  rtc_now_secs(void);
u8   rtc_sec(void);            /* 秒だけ（毎フレームの変化検出用） */
void rtc_write(const DateTime *dt);   /* 時計を合わせる */
u8   rtc_stale(void);          /* 時計が止まっていた（電池切れで昔の日付）なら 1 */
#endif
