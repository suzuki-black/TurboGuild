/* SPDX-License-Identifier: MIT */
/* rtc.c — BIOS REDCLK でブロック 0 の BCD 桁を読む。
 * 秒の桁上がりを跨ぐと値が崩れるので、2 回読んで一致するまで繰り返す。 */
#include "rtc.h"
#include "bios.h"
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

FAR(u8, rtc_sec, (void))
{
    return bcd2(0);
}
