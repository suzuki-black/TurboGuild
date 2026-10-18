/* wallet.c — 何か月空いても給料はたまる（ゲーム内で寝ていた間も働いていた扱い）。
 * 無敵モードは軍資金を減らさないが、累計課金額は数える（いくら使ったかを見て楽しむ）。 */
#include "wallet.h"
#include "save.h"
#include "tables.h"
#include "time.h"
#include "../far.h"

static u16 month_now(void)
{
    return (u16)(time_gday(g_save.g_time) / DAYS_PER_MONTH);
}

FAR(u8, wallet_infinite, (void))
{
    CourseRec c;
    if (g_save.course == 0) return 0;
    tbl_course(g_save.course, &c);
    return c.infinite;
}

FAR(void, wallet_start, (u8 course))
{
    CourseRec c;
    tbl_course(course, &c);
    g_save.course = course;
    g_save.funds = c.start;
    g_save.spent = 0;
    g_save.payday_month = month_now();
}

FAR(u8, wallet_buy_gems, (u8 pack))
{
    PackRec p;
    tbl_pack(pack, &p);
    if (!wallet_infinite()) {
        if (g_save.funds < p.price) return W_NO_FUNDS;
        g_save.funds -= p.price;
    }
    g_save.spent += p.price;
    g_save.gems += p.gems;
    return W_OK;
}

FAR(u8, wallet_payday, (void))
{
    CourseRec c;
    u16 now = month_now();
    u8 n;
    if (g_save.course == 0 || now <= g_save.payday_month) return 0;
    tbl_course(g_save.course, &c);
    n = (u8)(now - g_save.payday_month > 255 ? 255 : now - g_save.payday_month);
    g_save.funds += c.monthly * n;
    g_save.payday_month = now;
    return n;
}
