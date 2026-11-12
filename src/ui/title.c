/* SPDX-License-Identifier: MIT */
/* title.c — タイトル。はじめから / つづきから。[0] でライセンス、ESC で Phase0 の診断画面へ。
 * サービス終了後は 思い出を見る（アルバム）/ 思い出をリセット / スタッフロール（結末から）/ 運営モード（初めてなら結末とスタッフロールから） */
#include "screen.h"
#include "widget.h"
#include "savedisk.h"
#include "../far.h"
#include "../data/bgm_data.h"
void bgm_play(u8 song);
#include "../ram.h"
#include "../core/time.h"
#include "../core/timeline.h"
#include "../config.h"
#include "../data/screens.h"
#include "../data/tables.h"
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

/* 終了後に来続けると、跡地が古びていく（DECISIONS #97）。現実の経過で: 翌日から払い戻しの受付、
 * 30 日で受付終了、半年でページが無くなり、1 年で別の会社の広告になる */
static u16 ended_line(void)
{
    u32 e = g_save.g_time - (u32)SERVICE_END_DAY * GSEC_PER_DAY, d = e / (GSEC_PER_DAY * TIME_SCALE);   /* 現実の日数 */
    if (g_save.g_time < (u32)SERVICE_END_DAY * GSEC_PER_DAY || d < 1) return T_ENDED_TITLE;
    if (d < 30) return T_ENDED_REFUND;
    if (d < 182) return T_ENDED_REFUND_END;
    if (d < 365) return T_ENDED_404;
    return T_ENDED_AD;
}

static void draw_menu(void)
{
    static const u16 normal[3] = { T_TITLE_NEW, T_TITLE_CONT, T_OPS_MENU };   /* 運営モードは最初から入れる（DECISIONS #98） */
    static const u16 after[4] = { T_MEMORIES, T_RESET, T_STAFFROLL, T_OPS_MENU };   /* サービス終了後 */
    const u16 *items = ended() ? after : normal;
    u8 i, n = ended() ? 4 : 3;
    for (i = 0; i < n; i++) {
        u8 y = (u8)(ended() ? 120 + i * 19 : 138 + i * 19), on = (u8)(i == g_cursor);
        u8 off = (u8)(!ended() && i == 1 && !can_continue());   /* セーブが無ければ「つづきから」は選べない */
        ui_box(48, y, 160, 19, on ? COL_BLUE : COL_DBLUE, on ? COL_YELLOW : COL_DGRAY);   /* 「思い出をリセット」が入る幅 */
        ui_key((u8)(i + 1), 52, (u8)(y + 4));
        txt_id(9, (u8)(y + 2), items[i], off ? COL_DGRAY : on ? COL_YELLOW : COL_WHITE, on ? COL_BLUE : COL_DBLUE);
    }
}

FAR(void, title_init, (void))
{
    g_cursor = (u8)(can_continue() && !ended());
    bgm_play(BGM_HOME);
    g_viewonly = 0;
    if (ended() && !g_save.credits_seen) screen_change(&scr_ending);    /* 終了後はじめて: 結末とスタッフロール */
}

FAR(void, title_draw, (void))
{
    u8 i, k;
    ui_clear();
    for (k = 0; k < 6; k++) vdp_fill(0, (u16)(k * 4), SCREEN_W, 4, (u8)(k & 1 ? COL_RED : COL_DRED));
    vdp_fill(0, 24, SCREEN_W, 2, COL_YELLOW);
    txt_draw(1, 4, "TurboGuild", COL_YELLOW, COL_DRED);
    txt_draw(12, 4, "(C)2026 suzuki-black", COL_WHITE, COL_DRED);               /* 著作権表示（仕様 §11.5）。下の行は時計に使う */
    for (i = 0; i < 4; i++) gfx_card(parade[i], (u8)(10 + i * 60), (u8)(ended() ? 28 : 32 + (i & 1) * 6));   /* 終了後はメニューが 4 つなので詰める */
    if (!ended() && !can_continue()) {          /* ディスクが無ければ副題の代わりに知らせる（下に出すと 3 つ目の運営モードの欄に重なった） */
        if (g_save_status == SD_NODISK) txt_id(2, 116, T_TITLE_NOSAVE, COL_LGRAY, COL_BLACK);
        else txt_id(5, 116, T_TITLE_SUB, COL_WHITE, COL_BLACK);
    }
    if (ended()) {
        u16 t = ended_line();
        txt_id(t == T_ENDED_TITLE ? 6 : 1, 102, t, t == T_ENDED_AD ? COL_CYAN : t == T_ENDED_404 ? COL_LGRAY : COL_YELLOW, COL_BLACK);
    } else if (can_continue()) {
        u8 cx = txt_id(6, 116, T_TITLE_DAY, COL_LGRAY, COL_BLACK);        /* 運営モードの行が増えたので副題の位置に */
        cx = txt_num((u8)(cx + 1), 116, time_gday(g_save.g_time) + 1, COL_WHITE, COL_BLACK);
        txt_id(cx, 116, T_DAY, COL_LGRAY, COL_BLACK);
    }
    draw_menu();
    clock_title();                                                             /* 本体の時計と [9] 時刻修正（DECISIONS #101） */
    if (!ended()) txt_id(27, 196, T_TITLE_LICENSE, COL_DGRAY, COL_BLACK);
}

static void start_new(void)
{
    if (can_continue() && !ui_confirm(T_TITLE_OVERWRITE)) {
        g_dirty = 1;
        return;
    }
    save_new(&g_save, rtc_now_secs());
    g_time_status = TIME_OK;                     /* 新しく始めるなら、疑っていた時計の分は関係ない */
    screen_change(&scr_name);
}

FAR(void, title_update, (void))
{
    u8 p = input_pressed();
    u8 d = input_digit();
    if (ui_menu_input(&g_cursor, (u8)(ended() ? 4 : 3), p)) draw_menu();
    if (d == 1 || (d == 2 && can_continue()) || d == 3 || (d == 4 && ended())) { g_cursor = (u8)(d - 1); p = BTN_OK; }
    if (d == 0) { screen_change(&scr_license); return; }
    if (d == 9) { psg_beep(0x80, 4); screen_change(&scr_clock); return; }   /* 時刻修正 */
    if (g_frames == 0) clock_title();                                        /* 時計は 1 秒ごとに描き直す */
    if (p & BTN_OK) {
        psg_beep(0x80, 4);
        if (ended()) {
            if (g_cursor == 0) screen_change(&scr_album);                        /* 思い出を見る（アルバム） */
            else if (g_cursor == 2) screen_change(&scr_ending);                  /* 結末からスタッフロール */
            else if (g_cursor == 3) screen_change(&scr_ops);              /* 運営モード（裏面） */
            else if (ui_confirm(T_RESET_CONFIRM)) { save_new(&g_save, rtc_now_secs()); g_time_status = TIME_OK; screen_change(&scr_name); }
            else g_dirty = 1;
        } else if (g_cursor == 0) start_new();
        else if (g_cursor == 2) screen_change(&scr_ops);                   /* 運営モード（本編の前でも） */
        else if (can_continue()) {
            if (g_time_status >= TIME_ASK_SET) savedisk_clock_done(1);   /* 時計を直さずに進む: 留守の時間は足さない */
            g_from_title = 1;
            screen_change(&scr_home);
        }
    }
    if (p & BTN_CANCEL) screen_change(&scr_p0menu);
}

SCREEN(scr_title, title_init, title_update, title_draw, 0)
