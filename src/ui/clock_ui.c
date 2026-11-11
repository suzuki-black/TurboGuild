/* SPDX-License-Identifier: MIT */
/* clock_ui.c — 時計を合わせる画面（DECISIONS #101）。turboR の実機は時計の電池が切れていると、電源を入れるたびに
 * RTC が昔の日付（1980 年など）に戻る。このゲームは「前に保存した時刻からの経過 × 10」でゲーム内の時間を進めるので、
 * 起動したときに RTC の年が CLOCK_MIN_YEAR より前なら、セーブを読む前にここで合わせてもらう。
 * 左右で年・月・日・時・分を選び、上下で ±1、数字キーで直接入れる。SPACE で RTC に書いてからセーブを読む。
 * ESC は合わせずに進む（留守の間の時間は進まない）。
 * 入力中の日時は、まだ読み込んでいないセーブの場所（g_save の先頭）に置く（静的 RAM が残り少ないため。
 * この後の savedisk_boot が g_save をすべて書き直す）。 */
#include "screen.h"
#include "widget.h"
#include "savedisk.h"
#include "../far.h"
#include "../ram.h"
#include "../config.h"
#include "../core/time.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/rtc.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define EDIT  ((DateTime *)(void *)&g_save)
#define FIELDS 5
#define ROW_Y 96

static u8 mdays(u16 y, u8 m)
{
    static const u8 d[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return d[(m - 1) % 12];
}

static u16 get(u8 f)
{
    switch (f) {
    case 0: return EDIT->year;
    case 1: return EDIT->mon;
    case 2: return EDIT->day;
    case 3: return EDIT->hour;
    default: return EDIT->min;
    }
}

static void set(u8 f, u16 v)
{
    switch (f) {
    case 0: EDIT->year = v; break;
    case 1: EDIT->mon = (u8)v; break;
    case 2: EDIT->day = (u8)v; break;
    case 3: EDIT->hour = (u8)v; break;
    default: EDIT->min = (u8)v; break;
    }
}

static u16 lo(u8 f) { return f == 0 ? CLOCK_MIN_YEAR : f <= 2 ? 1 : 0; }

static u16 hi(u8 f)
{
    switch (f) {
    case 0: return 2079;                          /* RTC の年は 1980 + 2 桁 */
    case 1: return 12;
    case 2: return mdays(EDIT->year, EDIT->mon);
    case 3: return 23;
    default: return 59;
    }
}

static u8 valid(void)
{
    u8 f;
    for (f = 0; f < FIELDS; f++) if (get(f) < lo(f) || get(f) > hi(f)) return 0;
    return 1;
}

FAR(void, clock_init, (void))
{
    EDIT->year = CLOCK_MIN_YEAR; EDIT->mon = 1; EDIT->day = 1;
    EDIT->hour = 12; EDIT->min = 0; EDIT->sec = 0;
    g_cursor = 0;
}

/* 2026 年 01 月 01 日 12:00 */
static void fields(void)
{
    static const u8 x[FIELDS] = { 3, 9, 13, 17, 20 };
    static const u8 w[FIELDS] = { 4, 2, 2, 2, 2 };
    u8 f, cx;
    vdp_fill(0, ROW_Y, SCREEN_W, 16, COL_BLACK);
    for (f = 0; f < FIELDS; f++) {
        u8 on = (u8)(f == g_cursor), ok = (u8)(get(f) >= lo(f) && get(f) <= hi(f));
        char s[5];
        u16 v = get(f);
        u8 k;
        for (k = w[f]; k--; v /= 10) s[k] = (char)('0' + v % 10);   /* ゼロで埋める（01 月・09:05） */
        s[w[f]] = 0;
        cx = txt_draw(x[f], ROW_Y, s, on ? COL_BLACK : ok ? COL_WHITE : COL_RED, on ? COL_YELLOW : COL_BLACK);
        if (f < 3) txt_id(cx, ROW_Y, (u16)(T_CLK_Y + f), COL_LGRAY, COL_BLACK);
        else if (f == 3) txt_draw(cx, ROW_Y, ":", COL_LGRAY, COL_BLACK);
    }
}

FAR(void, clock_draw, (void))
{
    ui_clear();
    ui_header(T_CLK_TITLE);
    txt_id(2, 32, T_CLK_NOTE1, COL_WHITE, COL_BLACK);
    txt_id(2, 50, T_CLK_NOTE2, COL_LGRAY, COL_BLACK);
    txt_id(2, 68, T_CLK_NOTE3, COL_LGRAY, COL_BLACK);
    fields();
    txt_id(2, 130, T_CLK_HOW1, COL_LGRAY, COL_BLACK);
    txt_id(2, 148, T_CLK_HOW2, COL_LGRAY, COL_BLACK);
    ui_guide(T_CLK_GUIDE);
}

static void go_on(void)
{
    savedisk_boot();                              /* 合わせた時計でセーブを読み、留守の間の時間を足す */
    screen_change(&scr_title);
}

FAR(void, clock_update, (void))
{
    u8 p = input_pressed(), d = input_digit(), f = g_cursor;
    u16 v = get(f);
    if (p & BTN_CANCEL) { psg_beep(0x200, 4); go_on(); return; }   /* 合わせずに進む */
    if (p & BTN_OK) {
        if (!valid()) { psg_beep(0x400, 8); return; }
        EDIT->sec = 0;
        rtc_write(EDIT);
        psg_beep(0x080, 6);
        go_on();
        return;
    }
    if (p & BTN_LEFT) g_cursor = (u8)((f + FIELDS - 1) % FIELDS);
    else if (p & BTN_RIGHT) g_cursor = (u8)((f + 1) % FIELDS);
    else if (p & BTN_UP) set(f, v >= hi(f) ? lo(f) : (u16)(v + 1));
    else if (p & BTN_DOWN) set(f, v <= lo(f) ? hi(f) : (u16)(v - 1));
    else if (d <= 9) set(f, (u16)((v * 10 + d) % (f == 0 ? 10000 : 100)));   /* 打った数字を右から送り込む */
    else return;
    psg_beep(0x100, 2);
    fields();
}

SCREEN(scr_clock, clock_init, clock_update, clock_draw, 0)
