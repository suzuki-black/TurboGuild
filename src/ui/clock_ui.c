/* SPDX-License-Identifier: MIT */
/* clock_ui.c — 時計を合わせる画面と、タイトルの時計（DECISIONS #101）。turboR の実機は時計の電池が切れていると、
 * 電源を入れるたびに RTC がでたらめな日時になる（1980 年のことも、2059 年のように日付として正しく見えることもある）。
 * このゲームは「前に保存した時刻からの経過 × 10」でゲーム内の時間を進めるので、起動時に時計を疑ったとき
 * （年が範囲外 = TIME_ASK_SET、前回の保存より前か 30 日より先 = TIME_ASK_CHECK）は留守の時間を足さずに待ち、
 * タイトルの時計を赤くする。タイトルの [9] 時刻修正でここへ来て合わせると、その時刻で足す（savedisk_clock_done）。
 * 疑っていないときの時刻修正は、合わせた時刻を今として扱うだけ。
 * 左右で年・月・日・時・分を選び、上下で ±1。数字キーは、欄に移って最初の数字で打ち直しになり、桁が埋まると次の欄へ進む
 * （「202611122138」と続けて打てば全部入る）。打った桁数は EDIT->sec に数えておく（書くときに 0 にする）。
 * SPACE で RTC に書く。ESC は何もせずタイトルへ。
 * 入力中の日時は、読み込みの済んだディスクのバッファ g_sector に置く（静的 RAM が残り少ないため）。
 * バンク 2 が埋まったので、このファイルはバンク 102。 */
#include "screen.h"
#include "widget.h"
#include "savedisk.h"
#include "../far.h"
#include "../ram.h"
#include "../sys/gfx.h"
#include "../config.h"
#include "../core/time.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/rtc.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define EDIT  ((DateTime *)(void *)g_sector)
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
    if (g_time_status == TIME_ASK_SET) { EDIT->year = CLOCK_MIN_YEAR; EDIT->mon = 1; EDIT->day = 1; EDIT->hour = 12; EDIT->min = 0; }
    else rtc_read(EDIT);                          /* 確かめる・ふだんの修正: いまの時計の値から */
    EDIT->sec = 0;
    g_cursor = 0;
}

/* 2026/11/12 20:45（年を 4 桁、ほかは 0 で埋めた 2 桁） */
static u8 stamp(u8 cx, u8 y, const DateTime *t, u8 fg, u8 bg)
{
    char s[17];
    u16 v = t->year;
    s[0] = (char)('0' + v / 1000 % 10); s[1] = (char)('0' + v / 100 % 10); s[2] = (char)('0' + v / 10 % 10); s[3] = (char)('0' + v % 10);
    s[4] = '/'; s[5] = (char)('0' + t->mon / 10 % 10); s[6] = (char)('0' + t->mon % 10);
    s[7] = '/'; s[8] = (char)('0' + t->day / 10 % 10); s[9] = (char)('0' + t->day % 10);
    s[10] = ' '; s[11] = (char)('0' + t->hour / 10 % 10); s[12] = (char)('0' + t->hour % 10);
    s[13] = ':'; s[14] = (char)('0' + t->min / 10 % 10); s[15] = (char)('0' + t->min % 10); s[16] = 0;
    return txt_draw(cx, y, s, fg, bg);
}

/* タイトルの最下行: 本体の時計（疑っているときは赤）と [9] 時刻修正 */
FAR(void, clock_title, (void))
{
    DateTime t;
    rtc_read(&t);
    stamp(0, 196, &t, g_time_status >= TIME_ASK_SET ? COL_RED : COL_LGRAY, COL_BLACK);
    ui_key(9, 132, 198);
    txt_id(18, 196, T_CLK_FIX, g_time_status >= TIME_ASK_SET ? COL_YELLOW : COL_LGRAY, COL_BLACK);
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
    u16 t = g_time_status == TIME_ASK_SET ? T_CLK_TITLE : g_time_status == TIME_ASK_CHECK ? T_CLK_CTITLE : T_CLK_MTITLE;   /* 題と説明 3 行（文は続きの番号） */
    ui_header(t);
    txt_id(2, 32, (u16)(t + 1), COL_WHITE, COL_BLACK);
    txt_id(2, 50, (u16)(t + 2), COL_LGRAY, COL_BLACK);
    txt_id(2, 68, (u16)(t + 3), COL_LGRAY, COL_BLACK);
    fields();
    txt_id(2, 130, T_CLK_HOW1, COL_LGRAY, COL_BLACK);
    txt_id(2, 148, T_CLK_HOW2, COL_LGRAY, COL_BLACK);
    ui_guide(T_CLK_GUIDE);
}

static void go_on(void)
{
    savedisk_clock_done(0);                       /* 疑っていたなら、合わせた時計で留守の間の時間を足す */
    screen_change(&scr_title);
}

FAR(void, clock_update, (void))
{
    u8 p = input_pressed(), d = input_digit(), f = g_cursor;
    u16 v = get(f);
    if (p & BTN_CANCEL) { psg_beep(0x200, 4); screen_change(&scr_title); return; }   /* 何もせずタイトルへ */
    if (p & BTN_OK) {
        if (!valid()) { psg_beep(0x400, 8); return; }
        EDIT->sec = 0;                            /* 秒は 0 から（打った桁数の数えにも使っていた） */
        rtc_write(EDIT);
        psg_beep(0x080, 6);
        go_on();
        return;
    }
    if (d <= 9) {                                 /* 数字: 欄の最初の 1 桁で打ち直し、桁が埋まれば次の欄へ */
        u8 w = (u8)(f == 0 ? 4 : 2);
        set(f, (u16)(EDIT->sec ? v * 10 + d : d));
        if (++EDIT->sec >= w) { EDIT->sec = 0; if (f + 1 < FIELDS) g_cursor = (u8)(f + 1); }
    } else {
        if (!(p & (BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN))) return;
        EDIT->sec = 0;                            /* 矢印を押したら打ち直しの数えをやめる */
        if (p & BTN_LEFT) g_cursor = (u8)((f + FIELDS - 1) % FIELDS);
        else if (p & BTN_RIGHT) g_cursor = (u8)((f + 1) % FIELDS);
        else if (p & BTN_UP) set(f, v >= hi(f) ? lo(f) : (u16)(v + 1));
        else set(f, v <= lo(f) ? hi(f) : (u16)(v - 1));
    }
    psg_beep(0x100, 2);
    fields();
}

SCREEN(scr_clock, clock_init, clock_update, clock_draw, 0)
