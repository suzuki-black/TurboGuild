/* login_ui.c — ログインスタンプカード（7 日分のマス）。今日の分は「ポン」と押す演出つき。
 * マイページに入ったときにボーナスがあれば自動で開き、メニューの 6 からも見られる。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/save.h"
#include "../core/tables.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define CELL_X(i) ((u8)(6 + (i) * 35))
#define CELL_Y    44

static const u8 item_icon[5][2] = {
    { ATL_ITEM_GOLD_X, ATL_ITEM_GOLD_Y }, { ATL_ITEM_POTION_X, ATL_ITEM_POTION_Y },
    { ATL_ITEM_TICKET_X, ATL_ITEM_TICKET_Y }, { ATL_ITEM_GEMS_X, ATL_ITEM_GEMS_Y },
    { ATL_ITEM_SRTICKET_X, ATL_ITEM_SRTICKET_Y },
};

/* もう押してある日数（今の周の中で） */
static u8 stamped(void)
{
    if (g_login.given) return (u8)(g_login.day - 1);          /* 今日の分はこれから押す */
    if (g_save.login_day == 1) return g_save.last_login_gday == DAY_NEVER ? 0 : 7;
    return (u8)(g_save.login_day - 1);
}

static void stamp(u8 i)
{
    gfx_sprite(ATL_STAMP, (u8)(CELL_X(i) + 4), CELL_Y + 26);
}

static void cell(u8 i, u8 today)
{
    LoginRec lr;
    u8 x = CELL_X(i), cx;
    tbl_login((u8)(i + 1), &lr);
    ui_box(x, CELL_Y, 33, 60, today ? COL_BLUE : COL_DBLUE, today ? COL_YELLOW : COL_LGRAY);
    cx = txt_num((u8)(x / 8 + 1), CELL_Y + 2, i + 1, COL_WHITE, today ? COL_BLUE : COL_DBLUE);
    (void)cx;
    gfx_sprite(item_icon[lr.item][0], item_icon[lr.item][1], 14, 14, (u8)(x + 10), CELL_Y + 22);
    gfx_num((u8)(x + 4), CELL_Y + 44, lr.amount);
}

FAR(void, login_draw, (void))
{
    u8 i, done = stamped();
    u8 today = g_login.given ? (u8)(g_login.day - 1) : 0xFF;
    ui_clear();
    ui_header(T_STAMP_TITLE);
    for (i = 0; i < LOGIN_DAYS; i++) {
        cell(i, (u8)(i == today));
        if (i < done) stamp(i);
    }
    txt_id(6, 120, T_STAMP_NEXT, COL_WHITE, COL_BLACK);
    ui_guide(T_GUIDE_ANY);
    if (g_login.given) {
        u8 cx;
        ui_wait(30);
        stamp(today);
        psg_beep(0x040, 10);
        cx = txt_num(4, 148, g_login.day, COL_YELLOW, COL_BLACK);
        cx = txt_id(cx, 148, T_LOGIN_DAY, COL_YELLOW, COL_BLACK);
        cx = txt_id((u8)(cx + 1), 148, (u16)(T_ITEM_0 + g_login.item), COL_WHITE, COL_BLACK);
        cx = txt_draw(cx, 148, "x", COL_LGRAY, COL_BLACK);
        cx = txt_num(cx, 148, g_login.amount, COL_WHITE, COL_BLACK);
        txt_id(cx, 148, T_GET, COL_WHITE, COL_BLACK);
        g_login.given = 0;
    }
}

FAR(void, login_update, (void))
{
    if (input_pressed() & (BTN_OK | BTN_CANCEL)) screen_change(&scr_home);
}

SCREEN(scr_login, 0, login_update, login_draw, 0)
