/* title.c — タイトル。はじめから / つづきから。ESC で Phase0 の診断画面へ */
#include "screen.h"
#include "widget.h"
#include "savedisk.h"
#include "../far.h"
#include "../ram.h"
#include "../core/time.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/rtc.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

static u8 can_continue(void)
{
    return (u8)(g_save.flags & SF_STARTED);
}

static void draw_menu(void)
{
    static const u16 items[2] = { T_TITLE_NEW, T_TITLE_CONT };
    ui_menu_draw(10, ROW(8), items, can_continue() ? 2 : 1, g_cursor);
}

FAR(void, title_init, (void))
{
    g_cursor = can_continue();
}

FAR(void, title_draw, (void))
{
    ui_clear();
    vdp_fill(0, ROW(2), SCREEN_W, 48, COL_DRED);
    vdp_fill(0, ROW(2) + 4, SCREEN_W, 2, COL_LYELLOW);
    vdp_fill(0, ROW(2) + 42, SCREEN_W, 2, COL_LYELLOW);
    txt_draw(11, ROW(3), "TurboGuild", COL_LYELLOW, COL_DRED);
    txt_id(5, ROW(5) + 4, T_TITLE_SUB, COL_WHITE, COL_BLACK);
    if (can_continue()) {
        u8 cx = txt_id(6, ROW(6) + 8, T_TITLE_DAY, COL_GRAY, COL_BLACK);
        cx = txt_num((u8)(cx + 1), ROW(6) + 8, time_gday(g_save.g_time) + 1, COL_WHITE, COL_BLACK);
        txt_id(cx, ROW(6) + 8, T_DAY, COL_GRAY, COL_BLACK);
    }
    draw_menu();
    if (g_save_status == SD_NODISK) txt_id(1, ROW(11), T_TITLE_NOSAVE, COL_GRAY, COL_BLACK);
    ui_footer(T_HELP_SELECT);
}

static void start_new(void)
{
    if (can_continue() && !ui_confirm(T_TITLE_OVERWRITE)) {
        g_dirty = 1;
        return;
    }
    save_new(&g_save, rtc_now_secs());
    screen_change(&scr_name);
}

FAR(void, title_update, (void))
{
    u8 p = input_pressed();
    if (can_continue() && ui_menu_input(&g_cursor, 2, p)) draw_menu();
    if (p & BTN_OK) {
        psg_beep(0x80, 4);
        if (g_cursor == 0) start_new();
        else screen_change(&scr_home);
    }
    if (p & BTN_CANCEL) screen_change(&scr_p0menu);
}

SCREEN(scr_title, title_init, title_update, title_draw, 0)
