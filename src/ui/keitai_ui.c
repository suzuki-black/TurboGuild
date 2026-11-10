/* SPDX-License-Identifier: MIT */
/* keitai_ui.c — メンテナンス中の画面と足あとの画面（core/keitai.h, DECISIONS #95） */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/keitai.h"
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
