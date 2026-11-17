/* SPDX-License-Identifier: MIT */
/* life.c — 自分の側の暮らし（DECISIONS #114）。状態は持たず、日付とコースから毎回決める。
 *   テスト期間: おこづかい（学生）は、2 学期の中間・期末と学年末のテストの前 7 日間、20〜23 時は勉強で戦に入れない。
 *   残業: 社会人は平日の 4 日に 1 日、18〜21 時は戦に入れない。年末（12 月）は 2 日に 1 日で、23 時まで。
 *   給料日の広告: 本体の時計（RTC）が 25 日なら、流れるお知らせに給料日の広告が出る。
 * 親バレ（おこづかいで 1 か月に PARENT_LIMIT 円以上使うと、次のおこづかいが没収）は core/wallet.c。 */
#include "life.h"
#include "save.h"
#include "time.h"
#include "wallet.h"
#include "../data/text_ids.h"
#include "../far.h"
#include "../sys/rtc.h"

static const u16 test_week[3] = { 101, 150, 234 };   /* 0 日目 = 7 月 1 日。10/10・11/28・2/20 から 7 日間 */

static u8 mix(u16 d)
{
    u16 h = (u16)(d * 0x9E37u);
    h ^= h >> 7;
    h *= 0x2B5u;
    return (u8)(h >> 8);
}

/* g_time の時刻に戦に入れないなら、そのわけの文。入れるなら 0 */
FAR(u16, life_busy, (void))
{
    u16 d = (u16)(g_save.g_time / GSEC_PER_DAY);
    u8 h = (u8)((g_save.g_time % GSEC_PER_DAY) / 3600), k, wd = (u8)((d + 5) % 7);   /* 2011/7/1 は金曜（0 = 日曜） */
    if (g_save.course == COURSE_KID) {
        for (k = 0; k < sizeof(test_week) / sizeof(test_week[0]); k++)
            if (d >= test_week[k] && d < test_week[k] + 7 && h >= 20 && h < 23) return T_LF_TEST;
        return 0;
    }
    if (g_save.course != COURSE_WORK || wd == 0 || wd == 6) return 0;
    if (d >= 153 && d < 184) return (mix(d) & 1) || h < 18 || h >= 23 ? 0 : T_LF_BUSY;   /* 12 月は繁忙期 */
    return (mix(d) & 3) || h < 18 || h >= 21 ? 0 : T_LF_OVER;
}

/* 流れるお知らせに混ぜる広告。本体の時計が 25 日なら給料日の広告、ほかは 0 */
FAR(u16, life_ad, (void))
{
    DateTime t;
    rtc_read(&t);
    return t.day == 25 ? T_LF_AD25 : 0;
}
