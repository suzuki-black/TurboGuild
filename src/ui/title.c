/* SPDX-License-Identifier: MIT */
/* title.c — タイトル。はじめから / つづきから。[0] でライセンス、ESC で Phase0 の診断画面へ。
 * サービス終了後は 思い出を見る / 思い出をリセット / スタッフロール（初めてならスタッフロールから） */
#include "screen.h"
#include "widget.h"
#include "savedisk.h"
#include "../far.h"
#include "../data/bgm_data.h"
void bgm_play(u8 song);
#include "../ram.h"
#include "../core/time.h"
#include "../core/timeline.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/rtc.h"
#include "../sys/text.h"
#include "../sys/vdp.h"
#include "../sys/gfx.h"

static u8 can_continue(void)
{
    return (u8)(g_save.flags & SF_STARTED);
}

/* 看板のカード（SSR と SR を並べて見せる） */
static const u8 parade[4] = { 39, 33, 34, 40 };

static u8 ended(void)
{
    TlState tl;
    if (!can_continue()) return 0;
    timeline_state(&tl);
    return tl.ended;
}

static void draw_menu(void)
{
    static const u16 normal[2] = { T_TITLE_NEW, T_TITLE_CONT };
    static const u16 after[3] = { T_MEMORIES, T_RESET, T_STAFFROLL };   /* サービス終了後 */
    const u16 *items = ended() ? after : normal;
    u8 i, n = ended() ? 3 : can_continue() ? 2 : 1;
    for (i = 0; i < n; i++) {
        u8 y = (u8)((ended() ? 132 : 146) + i * 20), on = (u8)(i == g_cursor);
        ui_box(64, y, 128, 20, on ? COL_BLUE : COL_DBLUE, on ? COL_YELLOW : COL_DGRAY);
        ui_key((u8)(i + 1), 68, (u8)(y + 4));
        txt_id(11, (u8)(y + 2), items[i], on ? COL_YELLOW : COL_WHITE, on ? COL_BLUE : COL_DBLUE);
    }
}

FAR(void, title_init, (void))
{
    g_cursor = (u8)(can_continue() && !ended());
    bgm_play(BGM_HOME);
    g_viewonly = 0;
    if (ended() && !g_save.credits_seen) screen_change(&scr_credits);   /* 終了後はじめて: スタッフロール */
}

FAR(void, title_draw, (void))
{
    u8 i, k;
    ui_clear();
    for (k = 0; k < 6; k++) vdp_fill(0, (u16)(k * 4), SCREEN_W, 4, (u8)(k & 1 ? COL_RED : COL_DRED));
    vdp_fill(0, 24, SCREEN_W, 2, COL_YELLOW);
    txt_draw(11, 4, "TurboGuild", COL_YELLOW, COL_DRED);
    gfx_sprite(ATL_SPARK, 76, 2);
    gfx_sprite(ATL_SPARK, 172, 10);
    for (i = 0; i < 4; i++) gfx_card(parade[i], (u8)(10 + i * 60), (u8)(32 + (i & 1) * 6));
    if (!ended()) txt_id(5, 116, T_TITLE_SUB, COL_WHITE, COL_BLACK);
    if (ended()) {
        txt_id(6, 122, T_ENDED_TITLE, COL_YELLOW, COL_BLACK);
        txt_id(4, 196, T_THANKS, COL_LGRAY, COL_BLACK);
    } else if (can_continue()) {
        u8 cx = txt_id(6, 130, T_TITLE_DAY, COL_LGRAY, COL_BLACK);
        cx = txt_num((u8)(cx + 1), 130, time_gday(g_save.g_time) + 1, COL_WHITE, COL_BLACK);
        txt_id(cx, 130, T_DAY, COL_LGRAY, COL_BLACK);
    }
    draw_menu();
    if (g_save_status == SD_NODISK) txt_id(1, 190, T_TITLE_NOSAVE, COL_LGRAY, COL_BLACK);
    else if (!ended()) txt_id(20, 196, T_TITLE_LICENSE, COL_DGRAY, COL_BLACK);
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
    u8 d = input_digit();
    if (can_continue() && ui_menu_input(&g_cursor, (u8)(ended() ? 3 : 2), p)) draw_menu();
    if (d == 1 || (d == 2 && can_continue()) || (d == 3 && ended())) { g_cursor = (u8)(d - 1); p = BTN_OK; }
    if (d == 0) { screen_change(&scr_license); return; }
    if (p & BTN_OK) {
        psg_beep(0x80, 4);
        if (ended()) {
            if (g_cursor == 0) { g_viewonly = 1; screen_change(&scr_cardlist); }    /* 思い出を見る */
            else if (g_cursor == 2) screen_change(&scr_credits);
            else if (ui_confirm(T_RESET_CONFIRM)) { save_new(&g_save, rtc_now_secs()); screen_change(&scr_name); }
            else g_dirty = 1;
        } else if (g_cursor == 0) start_new();
        else { g_from_title = 1; screen_change(&scr_home); }
    }
    if (p & BTN_CANCEL) screen_change(&scr_p0menu);
}

SCREEN(scr_title, title_init, title_update, title_draw, 0)
