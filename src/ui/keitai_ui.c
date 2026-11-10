/* SPDX-License-Identifier: MIT */
/* keitai_ui.c — メンテナンス中の画面と足あとの画面（core/keitai.h, DECISIONS #95） */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/board.h"
#include "../core/keitai.h"
#include "../core/ranking.h"
#include "../data/tables.h"
#include "../core/player.h"
#include "../core/save.h"
#include "../core/tables.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

/* ---- 画面: メンテナンス中（SPACE / ESC で終了予定まで待つ。延長されていれば、もう一度） ---- */

static void hhmm(u8 x, u8 y, u32 t, u8 fg)
{
    u16 m = (u16)((t % GSEC_PER_DAY) / 60);
    char s[6];
    s[0] = (char)('0' + m / 600); s[1] = (char)('0' + m / 60 % 10); s[2] = ':';
    s[3] = (char)('0' + m % 60 / 10); s[4] = (char)('0' + m % 10); s[5] = 0;
    txt_draw(x, y, s, fg, COL_BLACK);
}

/* 画面の状態は持たない（静的 RAM が少ない）。描くたび・押すたびにその日のメンテを求め直す */
FAR(void, maint_draw, (void))
{
    Maint mt;
    u8 late;
    maint_now(&mt);
    late = (u8)(mt.extended && g_save.g_time >= mt.end0);
    ui_clear();
    ui_header(mt.kind == MAINT_EMERG ? T_MT_EMERG : T_MT_TITLE);
    txt_id(2, 48, T_MT_NOW, COL_WHITE, COL_BLACK);
    txt_id(2, 72, late ? T_MT_EXT : T_MT_END, late ? COL_ORANGE : COL_LGRAY, COL_BLACK);
    hhmm(20, 72, late ? mt.end : mt.end0, COL_YELLOW);
    txt_id(2, 104, T_MT_SORRY, COL_LGRAY, COL_BLACK);
    ui_guide(T_MT_GUIDE);
}

FAR(void, maint_update, (void))
{
    Maint mt;
    if (!(input_pressed() & (BTN_OK | BTN_CANCEL))) return;
    if (!maint_now(&mt)) { screen_change(&scr_home); return; }
    psg_beep(0x80, 4);
    player_ap_update();
    g_save.g_time = (mt.extended && g_save.g_time < mt.end0) ? mt.end0 : mt.end;   /* 延長は、はじめの予定を過ぎてから分かる */
    player_ap_update();
    if (g_save.g_time >= mt.end) screen_change(&scr_home);
    else g_dirty = 1;
}

SCREEN(scr_maint, 0, maint_update, maint_draw, 0)

/* ---- 画面: 足あと（お知らせの 2） ---- */

FAR(void, foot_draw, (void))
{
    FootList l;
    NpcRec n;
    u8 k, y, cx;
    ui_clear();
    ui_header(T_FOOT_TITLE);
    foot_list(&l);
    if (!l.n) txt_id(2, 40, T_FOOT_EMPTY, COL_LGRAY, COL_BLACK);
    for (k = 0, y = 20; k < l.n; k++, y = (u8)(y + 21)) {
        const FootRow *r = &l.r[k];
        u8 bg = k & 1 ? COL_BLACK : COL_DGRAY;
        vdp_fill(0, y, SCREEN_W, 20, bg);
        cx = txt_num(1, y + 2, r->day + 1, COL_LGRAY, bg);
        cx = txt_id(cx, y + 2, T_DAY, COL_LGRAY, bg);
        cx = txt_num((u8)(cx + 1), y + 2, r->hour, COL_LGRAY, bg);
        cx = txt_id(cx, y + 2, T_FOOT_HOUR, COL_LGRAY, bg);
        tbl_npc(r->npc, &n);
        cx = txt_id((u8)(cx + 1), y + 2, (u16)(NPC_PRE_TEXT + n.name_id / NPC_SUF_COUNT), r->kind == FOOT_THIEF ? COL_ORANGE : COL_WHITE, bg);
        cx = txt_id(cx, y + 2, (u16)(NPC_SUF_TEXT + n.name_id % NPC_SUF_COUNT), r->kind == FOOT_THIEF ? COL_ORANGE : COL_WHITE, bg);
        if (r->kind != FOOT_OTHER) txt_id((u8)(cx + 1), y + 2, (u16)(T_FOOT_K0 + r->kind), COL_CYAN, bg);
    }
    ui_guide(T_FOOT_GUIDE);
}

FAR(void, foot_update, (void))
{
    if (input_pressed() & (BTN_OK | BTN_CANCEL)) screen_change(&scr_news);
}

SCREEN(scr_foot, 0, foot_update, foot_draw, 0)

/* ---- 画面: ギルドのメンバー（伝言板の 1）。上下で選び、SPACE でプロフィール ---- */

/* 最後のログインから m 分を「○分前・○時間前・○日前」で */
static u8 ago(u8 cx, u8 y, u16 m, u8 fg, u8 bg)
{
    if (m < 60) { cx = txt_num(cx, y, m, fg, bg); return txt_id(cx, y, T_RS_MIN, fg, bg); }
    if (m < 1440) { cx = txt_num(cx, y, m / 60, fg, bg); return txt_id(cx, y, T_RS_HOUR, fg, bg); }
    cx = txt_num(cx, y, m / 1440, fg, bg);
    return txt_id(cx, y, T_RS_DAY, fg, bg);
}

static u8 name_at(u8 cx, u8 y, u16 npc, u8 fg, u8 bg)
{
    NpcRec n;
    tbl_npc(npc, &n);
    cx = txt_id(cx, y, (u16)(NPC_PRE_TEXT + n.name_id / NPC_SUF_COUNT), fg, bg);
    return txt_id(cx, y, (u16)(NPC_SUF_TEXT + n.name_id % NPC_SUF_COUNT), fg, bg);
}

#define RS_Y 20
#define RS_H 20

FAR(void, roster_draw, (void))
{
    Roster r;
    NpcNow n;
    u8 k, cx;
    ui_clear();
    ui_header(T_RS_TITLE);
    board_roster(&r);
    cx = txt_num(20, 0, r.n, COL_YELLOW, COL_DBLUE);
    txt_id(cx, 0, T_PERSON_N, COL_LGRAY, COL_DBLUE);
    if (g_cursor >= r.n) g_cursor = r.n ? (u8)(r.n - 1) : 0;
    if (!r.n) txt_id(2, 40, T_RS_EMPTY, COL_LGRAY, COL_BLACK);
    for (k = 0; k < r.n; k++) {
        const RosterRow *o = &r.r[k];
        u8 y = (u8)(RS_Y + k * RS_H), bg = k == g_cursor ? COL_DBLUE : (k & 1 ? COL_BLACK : COL_DGRAY);
        u8 away = (u8)(o->last >= 1440);
        vdp_fill(0, y, SCREEN_W, RS_H, bg);
        if (o->gm) txt_id(0, (u8)(y + 2), T_BOARD_GM, COL_YELLOW, bg);
        name_at(2, (u8)(y + 2), o->npc, away ? COL_LGRAY : COL_WHITE, bg);
        npc_now(o->npc, &n);
        cx = txt_id(16, (u8)(y + 2), T_RS_LV, COL_LGRAY, bg);
        txt_num(cx, (u8)(y + 2), n.level, COL_WHITE, bg);
        ago(23, (u8)(y + 2), o->last, away ? COL_ORANGE : COL_LGRAY, bg);
    }
    ui_guide(T_RS_GUIDE);
}

/* 2011 年 7 月 1 日から bd 日目の月と日 */
static u8 birthday(u8 cx, u8 y, u16 bd, u8 bg)
{
    static const u8 mlen[12] = { 31, 31, 30, 31, 30, 31, 31, 28, 31, 30, 31, 30 };
    u8 m = 0;
    while (m < 11 && bd >= mlen[m]) bd -= mlen[m++];
    cx = txt_num(cx, y, (u8)((m + 6) % 12 + 1), COL_WHITE, bg);
    cx = txt_id(cx, y, T_RS_MONTH, COL_WHITE, bg);
    cx = txt_num(cx, y, bd + 1, COL_WHITE, bg);
    return txt_id(cx, y, T_RS_DATE, COL_WHITE, bg);
}

static void profile(const RosterRow *o)
{
    NpcNow n;
    u8 cx, y = 44;
    npc_now(o->npc, &n);
    ui_box(16, 40, 224, 118, COL_DBLUE, COL_CYAN);
    cx = 3;
    if (o->gm) cx = txt_id(3, y, T_BOARD_GM, COL_YELLOW, COL_DBLUE);
    cx = name_at(cx, y, o->npc, COL_CYAN, COL_DBLUE);
    cx = txt_id((u8)(cx + 1), y, T_RS_LV, COL_LGRAY, COL_DBLUE);
    txt_num(cx, y, n.level, COL_WHITE, COL_DBLUE);
    cx = txt_id(3, (u8)(y + 20), T_RS_PAY, COL_LGRAY, COL_DBLUE);
    txt_id(cx, (u8)(y + 20), (u16)(T_RS_PAY0 + o->course), COL_WHITE, COL_DBLUE);
    cx = txt_id(3, (u8)(y + 38), T_RS_TONE, COL_LGRAY, COL_DBLUE);
    txt_id(cx, (u8)(y + 38), o->tone ? (u16)(T_BOARD_TONE1 + o->tone - 1) : T_RS_NOTONE, COL_WHITE, COL_DBLUE);
    cx = txt_id(3, (u8)(y + 56), T_RS_BDAY, COL_LGRAY, COL_DBLUE);
    birthday(cx, (u8)(y + 56), o->bd, COL_DBLUE);
    cx = txt_id(3, (u8)(y + 74), T_RS_LAST, COL_LGRAY, COL_DBLUE);
    ago(cx, (u8)(y + 74), o->last, o->last >= 1440 ? COL_ORANGE : COL_WHITE, COL_DBLUE);
    psg_beep(0x80, 4);
    ui_wait_key();
    g_dirty = 1;
}

FAR(void, roster_update, (void))
{
    u8 p = input_pressed();
    Roster r;
    if (!(p & (BTN_UP | BTN_DOWN | BTN_OK | BTN_CANCEL))) return;
    if (p & BTN_CANCEL) { screen_change(&scr_board); return; }
    board_roster(&r);
    if (!r.n) { if (p & BTN_OK) screen_change(&scr_board); return; }
    if (ui_menu_input(&g_cursor, r.n, p)) { g_dirty = 1; return; }
    if ((p & BTN_OK) && g_cursor < r.n) profile(&r.r[g_cursor]);
}

SCREEN(scr_roster, 0, roster_update, roster_draw, 0)

/* ---- 画面: メール（お知らせの 3。終わった後は思い出の 3）。上下で 1 通ずつ ---- */

#define ML_END 196

/* 本文を写して $ に自分の名前を入れ、32 桁で折り返して描く。次の y */
static u8 body(u8 y, u16 id, u8 bg)
{
    char b[96];
    const char *s = txt_ptr(id);
    u8 n = 0, k;
    for (; *s && n < sizeof(b) - 1; s++) {
        if (*s != '$') { b[n++] = *s; continue; }
        for (k = 0; k < sizeof(g_save.name) && g_save.name[k] && n < sizeof(b) - 1; k++) b[n++] = g_save.name[k];
    }
    b[n] = 0;
    s = b;
    while (*s && y + 16 <= ML_END) {
        vdp_fill(0, y, SCREEN_W, 16, bg);
        txt_draw(1, y, s, COL_WHITE, bg);
        s = g_txt_rest;
        y = (u8)(y + 16);
    }
    return y;
}

FAR(void, mail_draw, (void))
{
    MailList l;
    u8 k, y = 18, cx;
    ui_clear();
    ui_header(T_ML_TITLE);
    board_mail(&l);
    cx = txt_num(20, 0, l.n, COL_YELLOW, COL_DBLUE);
    txt_id(cx, 0, T_ML_COUNT, COL_LGRAY, COL_DBLUE);
    if (!l.n) txt_id(2, 40, T_ML_EMPTY, COL_LGRAY, COL_BLACK);
    if (g_cursor >= l.n) g_cursor = l.n ? (u8)(l.n - 1) : 0;
    for (k = g_cursor; k < l.n && y + 32 <= ML_END; k++) {
        u8 bg = k & 1 ? COL_BLACK : COL_DGRAY;
        vdp_fill(0, y, SCREEN_W, 16, bg);
        cx = txt_num(1, y, l.r[k].day + 1, COL_LGRAY, bg);
        cx = txt_id(cx, y, T_BOARD_DAY, COL_LGRAY, bg);
        cx = txt_id((u8)(cx + 1), y, T_ML_FROM, COL_LGRAY, bg);   /* 英語は「from 名前」、日本語は「名前さんから」 */
        cx = name_at(cx, y, l.r[k].npc, COL_CYAN, bg);
        txt_id(cx, y, T_ML_FROM2, COL_LGRAY, bg);
        y = body((u8)(y + 16), l.r[k].text, bg);
        y = (u8)(y + 2);
    }
    ui_guide(T_ML_GUIDE);
}

FAR(void, mail_update, (void))
{
    u8 p = input_pressed();
    if ((p & BTN_UP) && g_cursor) { g_cursor--; psg_beep(0x100, 2); g_dirty = 1; }
    if ((p & BTN_DOWN) && g_cursor < MAIL_MAX - 1) { g_cursor++; psg_beep(0x100, 2); g_dirty = 1; }
    if (p & (BTN_OK | BTN_CANCEL)) screen_change(g_save.g_time >= (u32)SERVICE_END_DAY * GSEC_PER_DAY ? &scr_album : &scr_news);
}

SCREEN(scr_mail, 0, mail_update, mail_draw, 0)
