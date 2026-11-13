/* SPDX-License-Identifier: MIT */
/* help_ui.c — ヘルプ・規約・お問い合わせ（DECISIONS #103）。お知らせの下のリンク（4）から。
 *   1 よくある質問        当時の FAQ の口ぶり（補償はいたしかねます）
 *   2 特定商取引法に基づく表記
 *   3 お問い合わせ        4 つの中から選んで送ると、翌日に運営事務局から定型の返事がメールで届く（g_save.ask_day）。
 *                         返事の文は、運営モードで「定型文で返す」を選んだときに送っていた文と同じ（T_ML_ASK0〜3）
 *   4 退会はこちら        3 回引き止められ、最後は「PC サイトから」。データは消えない
 * 画面の状態は持たない（静的 RAM が少ない）。下の頁は押されるまで待つ。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../config.h"
#include "../core/save.h"
#include "../core/time.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define HL_ITEMS 4
#define HL_Y     24
#define HL_H     24

/* 文を 32 桁で折り返して描く（背景の帯つき）。次の y */
static u8 para(u8 y, u16 id, u8 fg, u8 bg)
{
    const char *s = txt_ptr(id);
    while (*s && y + 16 <= ROW_LAST) {
        vdp_fill(0, y, SCREEN_W, 16, bg);
        txt_draw(1, y, s, fg, bg);
        s = g_txt_rest;
        y = (u8)(y + 16);
    }
    return y;
}

/* 下の頁: 題と、文 n 本（first から続きの番号）。押されたら戻る */
static void page(u16 title, u16 first, u8 n, u8 qa)
{
    u8 k, y = 20;
    ui_clear();
    ui_header(title);
    for (k = 0; k < n; k++) {
        u8 q = (u8)(qa && !(k & 1));               /* よくある質問: 偶数が問い、奇数が答え */
        y = para(y, (u16)(first + k), q ? COL_YELLOW : COL_WHITE, qa ? (k & 2 ? COL_DGRAY : COL_BLACK) : (k & 1 ? COL_DGRAY : COL_BLACK));
        if (!qa || (k & 1)) y = (u8)(y + 2);
    }
    ui_guide(T_HL_BACK);
    psg_beep(0x80, 4);
    ui_wait_key();
    g_dirty = 1;
}

static void row(u8 i, u8 on)
{
    u8 y = (u8)(HL_Y + i * HL_H), bg = on ? COL_BLUE : COL_DBLUE;
    ui_box(16, y, 224, 20, bg, on ? COL_YELLOW : COL_LGRAY);
    ui_key((u8)(i + 1), 24, (u8)(y + 4));
    txt_id(5, (u8)(y + 2), (u16)(T_HL_M0 + i), i == 3 ? COL_ORANGE : COL_WHITE, bg);
}

FAR(void, help_draw, (void))
{
    u8 i;
    ui_clear();
    ui_header(T_HL_TITLE);
    if (g_cursor >= HL_ITEMS) g_cursor = 0;
    for (i = 0; i < HL_ITEMS; i++) row(i, (u8)(i == g_cursor));
    txt_id(2, 130, T_HL_NOTE, COL_LGRAY, COL_BLACK);
    ui_guide(T_HL_GUIDE);
}

/* お問い合わせ: 種類を選んで送る */
static void ask(void)
{
    u8 i, k = 0;
    ui_clear();
    ui_header(T_HL_M2);
    txt_id(2, 22, T_HL_ASKQ, COL_WHITE, COL_BLACK);
    for (i = 0; i < 4; i++) {
        ui_box(16, (u8)(44 + i * 24), 224, 20, COL_DBLUE, COL_LGRAY);
        ui_key((u8)(i + 1), 24, (u8)(48 + i * 24));
        txt_id(5, (u8)(46 + i * 24), (u16)(T_HL_ASK0 + i), COL_WHITE, COL_DBLUE);
    }
    ui_guide(T_HL_ASKG);
    for (;;) {
        u8 p, d;
        ui_frame();
        p = input_pressed(); d = input_digit();
        if (p & BTN_CANCEL) { g_dirty = 1; return; }
        if (d >= 1 && d <= 4) { k = (u8)(d - 1); break; }
    }
    psg_beep(0x100, 2);
    if (!ui_confirm(T_HL_SEND)) return;
    g_save.ask_day = (u16)(time_gday(g_save.g_time) + 2);   /* 翌日に返事（届く日 + 1 で持つ） */
    g_save.ask_kind = k;
    g_save_dirty = 1;
    ui_msg(T_HL_SENT);
}

/* 退会: 3 回引き止められて、最後は PC サイトへ。データは消さない */
static void quit(void)
{
    u8 k;
    for (k = 0; k < 3; k++)
        if (!ui_confirm((u16)(T_HL_QUIT0 + k))) { ui_msg(T_HL_STAY); return; }
    ui_msg(T_HL_PC);
}

static void open(u8 i)
{
    psg_beep(0x80, 4);
    switch (i) {
    case 0: page(T_HL_M0, T_HL_FAQ0, 6, 1); break;
    case 1: page(T_HL_M1, T_HL_LAW0, 6, 0); break;
    case 2: ask(); break;
    default: quit(); break;
    }
}

FAR(void, help_update, (void))
{
    u8 p = input_pressed(), d = input_digit(), old = g_cursor;
    if (p & BTN_CANCEL) { psg_beep(0x200, 4); screen_change(&scr_news); return; }
    if (d >= 1 && d <= HL_ITEMS) { g_cursor = (u8)(d - 1); row(old, 0); row(g_cursor, 1); open(g_cursor); return; }
    if (p & BTN_OK) { open(g_cursor); return; }
    if (ui_menu_input(&g_cursor, HL_ITEMS, p)) { psg_beep(0x100, 2); row(old, 0); row(g_cursor, 1); }
}

SCREEN(scr_help, 0, help_update, help_draw, 0)
