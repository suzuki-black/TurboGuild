/* SPDX-License-Identifier: MIT */
/* login.c — 何日空いても 1 回分だけ。連続が途切れても周期はリセットしない（当時風の緩さ） */
#include "login.h"
#include "player.h"
#include "save.h"
#include "tables.h"
#include "time.h"
#include "../far.h"

FAR(void, login_check, (LoginResult *r))
{
    u32 gday = time_gday(g_save.g_time);
    LoginRec lr;
    r->given = 0;
    if (g_save.last_login_gday == gday) return;
    if (g_save.login_day < 1 || g_save.login_day > LOGIN_DAYS) g_save.login_day = 1;
    r->day = g_save.login_day;
    tbl_login(r->day, &lr);
    r->item = lr.item;
    r->amount = lr.amount;
    player_add_item(lr.item, lr.amount);
    g_save.login_day = (u8)(r->day % LOGIN_DAYS + 1);
    g_save.last_login_gday = gday;
    r->given = 1;
}
