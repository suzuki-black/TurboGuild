/* SPDX-License-Identifier: MIT */
/* rtc.c — BIOS REDCLK でブロック 0 の BCD 桁を読む。
 * 秒の桁上がりを跨ぐと値が崩れるので、2 回読んで一致するまで繰り返す。 */
#include "rtc.h"
#include "bios.h"
#include "../config.h"
#include "../far.h"

static u8 bcd2(u8 reg_lo)
{
    return (u8)(bios_redclk(reg_lo + 1) * 10 + bios_redclk(reg_lo));
}

static void read_once(DateTime *dt)
{
    dt->sec  = bcd2(0);
    dt->min  = bcd2(2);
    dt->hour = bcd2(4);
    dt->day  = bcd2(7);
    dt->mon  = bcd2(9);
    dt->year = (u16)(1980 + bcd2(11));
}

FAR(void, rtc_read, (DateTime *dt))
{
    DateTime b;
    do {
        read_once(dt);
        read_once(&b);
    } while (dt->sec != b.sec || dt->min != b.min);
}

FAR(u32, rtc_now_secs, (void))
{
    DateTime dt;
    rtc_read(&dt);
    return time_to_secs(&dt);
}

static void wr2(u8 reg_lo, u8 v)
{
    bios_wrtclk(reg_lo, (u8)(v % 10));
    bios_wrtclk((u8)(reg_lo + 1), (u8)(v / 10));
}

/* 時計を合わせる（DECISIONS #101）。電池の切れた RTC は 12 時間制に戻っていることがあるので 24 時間制にし、
 * うるう年の数え役（年 % 4）と曜日も書く。秒は 0 から */
FAR(void, rtc_write, (const DateTime *dt))
{
    u16 y = dt->year < 1980 ? 0 : (u16)(dt->year - 1980);
    bios_wrtclk(0x1A, 1);                         /* ブロック 1: 24 時間制 */
    bios_wrtclk(0x1B, (u8)(y & 3));               /* ブロック 1: うるう年の数え役（1980 年はうるう年） */
    wr2(0, 0);
    wr2(2, dt->min);
    wr2(4, dt->hour);
    bios_wrtclk(6, (u8)((time_to_secs(dt) / 86400UL + 2) % 7));   /* 曜日（1980-01-01 は火曜 = 2） */
    wr2(7, dt->day);
    wr2(9, dt->mon);
    wr2(11, (u8)(y % 100));
}

/* 時計が止まっていた（年が CLOCK_MIN_YEAR より前）か。電池の切れた RTC は桁がでたらめなこともあるので、
 * 2079 年（2 桁の上限）より後や、あり得ない月日・時分も止まっていたと見なす */
FAR(u8, rtc_stale, (void))
{
    DateTime dt;
    rtc_read(&dt);
    return (u8)(dt.year < CLOCK_MIN_YEAR || dt.year > 2079 || dt.mon < 1 || dt.mon > 12 || dt.day < 1 || dt.day > 31 ||
                dt.hour > 23 || dt.min > 59);
}

FAR(u8, rtc_sec, (void))
{
    return bcd2(0);
}
