/* test_time.c — 日時→通算秒、ゲーム時間の進め方（逆行・30 日上限） */
#include "check.h"
#include "core/time.h"
#include "config.h"

static u32 secs(u16 y, u8 mo, u8 d, u8 h, u8 mi, u8 s)
{
    DateTime dt = { y, mo, d, h, mi, s };
    return time_to_secs(&dt);
}

int main(void)
{
    u8 st;
    u32 g;
    CHECK(secs(1980, 1, 1, 0, 0, 0) == 0);
    CHECK(secs(1980, 1, 2, 0, 0, 0) == 86400UL);
    CHECK(secs(1980, 3, 1, 0, 0, 0) == 60UL * 86400UL);          /* 1980 はうるう年 */
    CHECK(secs(1981, 1, 1, 0, 0, 0) == 366UL * 86400UL);
    /* UNIX 時刻との差（1980-01-01 = 315532800）で検算 */
    CHECK(secs(2000, 1, 1, 0, 0, 0) == 946684800UL - 315532800UL);
    CHECK(secs(2011, 7, 15, 12, 34, 56) == 1310733296UL - 315532800UL);
    CHECK(secs(2026, 10, 18, 23, 59, 59) - secs(2026, 10, 18, 0, 0, 0) == 86399UL);

    /* 現実 1 秒 = ゲーム 10 秒 */
    g = time_advance(1000, 5000, 5021, &st);
    CHECK(st == TIME_OK && g == 1000 + 210);
    /* 現実 2.4 時間 = ゲーム 1 日 */
    g = time_advance(0, 0, 8640, &st);
    CHECK(time_gday(g) == 1 && st == TIME_OK);
    /* 逆行は 0 扱いで警告 */
    g = time_advance(777, 5000, 4000, &st);
    CHECK(g == 777 && st == TIME_BACKWARD);
    /* 30 日を超えたら切り捨て */
    g = time_advance(0, 0, 100UL * 86400UL, &st);
    CHECK(st == TIME_CLAMPED && g == OFFLINE_MAX_REAL * TIME_SCALE);
    /* ちょうど 30 日は切り捨てない */
    g = time_advance(0, 0, OFFLINE_MAX_REAL, &st);
    CHECK(st == TIME_OK);
    /* サービス 270 日でも 32bit に収まる */
    CHECK(270UL * GSEC_PER_DAY < 0xFFFFFFFFUL);
    CHECK_DONE("time");
    return 0;
}
