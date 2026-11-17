/* SPDX-License-Identifier: MIT */
/* border_ui.c — ボーダー予想（DECISIONS #112）。ランキングの 3 から（イベントの間だけ）。
 * NPC のイベントポイントは「腕前 × 経過割合」で全員が同じ割合で伸びるので（core/ranking.c）、順位は開催中に入れ替わらない。
 * だから 10 位・100 位・500 位のいまの点は、攻略スレの正解と同じ終了の日のボーダー（data/border.h）× 経過割合で出せる。
 * 1000 人をなめないので一瞬で出る。必要な点は「最終日のボーダー - 自分の点」を残りの時間で割る。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../config.h"
#include "../core/save.h"
#include "../core/tables.h"
#include "../data/border.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define BO_Y   40
#define BO_H   18

static const u16 bo_rank[3] = { 10, 100, 500 };

FAR(void, border_init, (void)) { }

FAR(void, border_draw, (void))
{
    EventRec e;
    u32 start, len, now = g_save.g_time, left, b, pt = g_save.event_pt;
    u16 prog = 256, hours;
    u8 k, y, cx, bg;
    ui_clear();
    ui_header(T_BO_TITLE);
    ui_guide(T_HELP_BACK);
    if (!g_save.event_id || g_save.event_id > sizeof(th_border) / sizeof(th_border[0])) {
        txt_id(2, 40, T_BO_NOEV, COL_LGRAY, COL_BLACK);
        return;
    }
    tbl_event((u8)g_save.event_id, &e);
    start = (u32)e.start_day * GSEC_PER_DAY;
    len = (u32)e.duration * GSEC_PER_DAY;
    if (now <= start) prog = 0;
    else if (now - start < len) prog = (u16)((now - start) / (len / 256));   /* ranking.c と同じ刻み */
    left = now >= start + len ? 0 : start + len - (now > start ? now : start);
    hours = (u16)((left + 3599) / 3600);
    txt_id(1, 18, e.name_text, COL_CYAN, COL_BLACK);
    cx = txt_id(20, 0, T_BO_LEFT, COL_LGRAY, COL_DBLUE);          /* 残りの時間は見出しの右に（イベント名が長くても重ならない） */
    cx = txt_num(cx, 0, hours, COL_WHITE, COL_DBLUE);
    txt_id(cx, 0, T_BO_HOURS, COL_LGRAY, COL_DBLUE);
    vdp_fill(0, 36, SCREEN_W, 1, COL_DGRAY);
    txt_id(1, BO_Y, T_BO_RANK, COL_LGRAY, COL_BLACK);
    txt_id(8, BO_Y, T_BO_NOW, COL_LGRAY, COL_BLACK);
    txt_id(15, BO_Y, T_BO_FINAL, COL_LGRAY, COL_BLACK);
    txt_id(24, BO_Y, T_BO_NEED, COL_LGRAY, COL_BLACK);
    for (k = 0; k < 3; k++) {
        y = (u8)(BO_Y + (k + 1) * BO_H);
        bg = (u8)(k & 1 ? COL_BLACK : COL_DGRAY);
        b = th_border[g_save.event_id - 1][k];
        vdp_fill(0, y, SCREEN_W, 16, bg);
        cx = txt_num_r(1, y, bo_rank[k], 3, COL_WHITE, bg);
        txt_id(cx, y, T_BO_POS, COL_LGRAY, bg);
        txt_num_r(6, y, (b >> 8) * prog + (((b & 255) * prog) >> 8), 8, COL_WHITE, bg);   /* いま = 最終 × 経過割合 */
        txt_num_r(14, y, b, 8, COL_YELLOW, bg);
        if (pt >= b) txt_id(25, y, T_BO_IN, COL_CYAN, bg);                                /* もう届いている */
        else if (!hours) txt_id(25, y, T_BO_OUT, COL_LGRAY, bg);
        else txt_num_r(23, y, (b - pt + hours - 1) / hours, 8, COL_ORANGE, bg);
    }
    y = (u8)(BO_Y + 4 * BO_H + 10);
    cx = txt_id(1, y, T_BO_YOU, COL_LGRAY, COL_BLACK);
    cx = txt_num((u8)(cx + 1), y, pt, COL_WHITE, COL_BLACK);
    txt_id(cx, y, T_BO_PT, COL_LGRAY, COL_BLACK);
    if (prog >= 32 && prog < 256) {                            /* 開催の 1/8 を過ぎてから（ライバルの見込みと同じ） */
        cx = txt_id(1, (u8)(y + 18), T_BO_PACE, COL_LGRAY, COL_BLACK);
        cx = txt_num((u8)(cx + 1), (u8)(y + 18), pt >> 24 ? pt : (pt << 8) / prog, COL_YELLOW, COL_BLACK);
        txt_id(cx, (u8)(y + 18), T_BO_PT, COL_LGRAY, COL_BLACK);
    }
    txt_id(1, 172, T_BO_NOTE, COL_DGRAY, COL_BLACK);
}

FAR(void, border_update, (void))
{
    u8 p = input_pressed();
    g_ui_state = UIS_WAITKEY;
    if (p & (BTN_CANCEL | BTN_OK)) { psg_beep(0x80, 4); screen_change(&scr_ranking); }
}

SCREEN(scr_border, border_init, border_update, border_draw, 0)
