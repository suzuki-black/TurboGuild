/* time.c — RTC の日時を通算秒にし、前回保存からの差分を TIME_SCALE 倍してゲーム時間へ足す。
 * 起点は RTC の年の下限に合わせて 1980 年。逆行は 0、30 日超は切り捨て。 */
#include "time.h"
#include "../config.h"

static const u16 days_before_month[12] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

static u8 is_leap(u16 y)
{
    return (u8)((y % 4 == 0 && y % 100 != 0) || y % 400 == 0);
}

static u32 days_since_1980(u16 year, u8 mon, u8 day)
{
    u32 days = 0;
    u16 y;
    for (y = 1980; y < year; y++) {
        days += is_leap(y) ? 366 : 365;
    }
    days += days_before_month[mon - 1];
    if (mon > 2 && is_leap(year)) days++;
    return days + (u32)(day - 1);
}

u32 time_to_secs(const DateTime *dt)
{
    u32 d = days_since_1980(dt->year, dt->mon, dt->day);
    return d * 86400UL + (u32)dt->hour * 3600UL + (u32)dt->min * 60UL + dt->sec;
}

u32 time_advance(u32 gtime, u32 real_last, u32 real_now, u8 *status)
{
    u32 delta;
    *status = TIME_OK;
    if (real_now < real_last) {
        *status = TIME_BACKWARD;
        return gtime;
    }
    delta = real_now - real_last;
    if (delta > OFFLINE_MAX_REAL) {
        delta = OFFLINE_MAX_REAL;
        *status = TIME_CLAMPED;
    }
    return gtime + delta * TIME_SCALE;
}

u32 time_gday(u32 gtime)
{
    return gtime / GSEC_PER_DAY;
}
