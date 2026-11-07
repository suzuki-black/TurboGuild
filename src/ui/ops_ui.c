/* SPDX-License-Identifier: MIT */
/* ops_ui.c — 運営モード（裏面, core/opsim.h, DECISIONS #88）。サービス終了後のタイトルから入る。
 * 上に今日の数字、まん中に 270 日のグラフ（DAU 緑・課金率 黄・引退 赤・新規 水色・売上 暗い赤の棒）。
 * 節目の日にはグラフの上に会議の箱を出して選ばせ、閉じたらグラフを描き直す。
 * グラフの点は日ごとに VRAM のページ 2（VRAM2_HIST）へ残すので、何度でも描き直せる。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../data/bgm_data.h"
void bgm_play(u8 song);
#include "../ram.h"
#include "../core/opsim.h"
#include "../core/save.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define GX     8                  /* グラフの左（240 ドットに 270 日） */
#define GW     240
#define GB     178                /* グラフの底 */
#define GH     120                /* 高さ（点は 0..GH。8 ビット目は会議の印） */
#define MEET   0x80
#define BOX_Y  52
#define BOX_H  132
#define HIST   5                  /* 1 日ぶんの点のバイト数 */

#define ST_RUN  0
#define ST_MEET 1
#define ST_DONE 2

static u8 st;
static u8 newrec;                 /* 決算で新記録だった */
static u8 ad_seen;                /* 知らせた広告の止まり方（下の帯に出す） */
static OpsMeeting mt;
static u8 last[4];                /* 前の日の点（線をつなぐ） */
static u16 pay_avg;               /* 課金率（0.1%）をならしたもの。日ごとでは上下しすぎて読めない */

static u8 day_x(u16 d) { return (u8)(GX + (u16)(d * 8 / 9)); }

static void vline(u8 x, u8 a, u8 b, u8 col)
{
    VdpCmd c;
    u8 t;
    if (a > b) { t = a; a = b; b = t; }
    c.sx = 0; c.sy = 0;
    c.dx = x; c.dy = (u16)(GB - b);
    c.nx = 1; c.ny = (u16)(b - a + 1);
    c.clr = col;
    c.arg = 0;
    c.cmd = 0x80;                 /* LMMV: 1 ドット幅 */
    vdp_cmd(&c);
}

static u8 scale(u32 v, u32 max)
{
    if (v >= max) return GH;
    return (u8)(v * GH / max);
}

static s32 profit(void) { return (s32)(g_ops.sales_total - g_ops.ad_total); }

/* 1 日ぶんの点: DAU（1000 人で上まで）・課金率（ならして 20%）・引退（1000 人）・新規（1 日 30 人）・売上の棒（1 日 40 万円） */
static void points(u8 *p)
{
    p[0] = scale(g_ops.dau, 1000);
    pay_avg = (u16)((pay_avg * 3 + (g_ops.dau ? (u32)g_ops.payers * 1000 / g_ops.dau : 0)) / 4);
    p[1] = scale(pay_avg, 200);
    p[2] = scale(g_ops.retired, 1000);
    p[3] = scale(g_ops.fresh_users, 30);
    p[4] = scale(g_ops.sales_day, 400000UL);
}

static void plot(u16 d, const u8 *p, u8 *prev)
{
    u8 x = day_x(d), k;
    static const u8 col[4] = { COL_GREEN, COL_YELLOW, COL_RED, COL_CYAN };
    if (p[0] & MEET) vline(x, 0, GH, COL_DGRAY);
    if (p[4]) vline(x, 0, p[4], COL_DRED);
    for (k = 4; k--; ) {
        u8 v = (u8)(p[k] & ~MEET);
        vline(x, d ? prev[k] : v, v, col[k]);
        prev[k] = v;
    }
}

static void graph(void)
{
    u8 buf[12 * HIST], prev[4];
    u16 d = 0;
    VdpXfer x;
    ui_box(GX - 2, GB - GH - 4, GW + 4, GH + 8, COL_BLACK, COL_DGRAY);
    while (d < g_ops.day) {
        u8 n = (u8)(g_ops.day - d > 12 ? 12 : g_ops.day - d), k;
        x.addr = (u16)(VRAM2_HIST + d * HIST); x.src = buf; x.len = (u8)(n * HIST);
        vdp_page2_read(&x);
        for (k = 0; k < n; k++, d++) plot(d, buf + k * HIST, prev);
    }
    for (d = 0; d < 4; d++) last[d] = prev[d];
}

/* 1234万円 / -320万円 */
static u8 money(u8 cx, u8 y, s32 v, u8 fg, u8 bg)
{
    if (v < 0) { cx = txt_draw(cx, y, "-", fg, bg); v = -v; }
    cx = txt_num(cx, y, (u32)v / 10000, fg, bg);
    return txt_id(cx, y, T_OPS_MAN, COL_LGRAY, bg);
}

/* 上の帯の右に日付、その下 2 行に今日の数字（色はグラフの線と同じ）。
 * 毎日描くので消してから描かない（ちらつく）。数字は幅を決めて上書きし、長さの変わる利益の後ろだけ消す */
static void kpi(void)
{
    u8 cx;
    u16 pct = g_ops.dau ? (u16)((u32)g_ops.payers * 100 / g_ops.dau) : 0;
    cx = txt_id(20, 0, T_OPS_DAYP, COL_LGRAY, COL_DBLUE);
    cx = txt_num_r((u8)(cx + 1), 0, g_ops.day, 3, COL_WHITE, COL_DBLUE);
    txt_id(cx, 0, T_OPS_DAYS, COL_LGRAY, COL_DBLUE);
    cx = txt_id(1, 18, T_OPS_DAU, COL_GREEN, COL_BLACK);
    cx = txt_num_r((u8)(cx + 1), 18, g_ops.dau, 4, COL_WHITE, COL_BLACK);
    cx = txt_id((u8)(cx + 1), 18, T_OPS_NEW, COL_CYAN, COL_BLACK);
    cx = txt_num_r((u8)(cx + 1), 18, g_ops.fresh_users, 3, COL_WHITE, COL_BLACK);
    cx = txt_id((u8)(cx + 1), 18, T_OPS_RET, COL_RED, COL_BLACK);
    txt_num_r((u8)(cx + 1), 18, g_ops.retired, 4, COL_WHITE, COL_BLACK);
    cx = txt_id(1, 34, T_OPS_PAY, COL_YELLOW, COL_BLACK);
    cx = txt_num_r((u8)(cx + 1), 34, pct, 2, COL_WHITE, COL_BLACK);
    cx = txt_draw(cx, 34, "%", COL_WHITE, COL_BLACK);
    cx = txt_id((u8)(cx + 1), 34, T_OPS_PROFIT, COL_ORANGE, COL_BLACK);
    cx = money((u8)(cx + 1), 34, profit(), COL_WHITE, COL_BLACK);
    if (cx < 32) vdp_fill((u16)(cx * 8), 34, (u16)(SCREEN_W - cx * 8), 16, COL_BLACK);
    if (g_ops.ad_on && cx < 26) txt_id(26, 34, T_OPS_ADON, COL_CYAN, COL_BLACK);
}

/* 箱の中の行（2 桁目から 28 桁。選択肢は 4 桁目から 25 桁） */
static u8 row_y(u8 row) { return (u8)(BOX_Y + 6 + row * 18); }

static u8 line(u8 row, u16 id, u8 fg)
{
    return txt_id(2, row_y(row), id, fg, COL_DBLUE);
}

static void points_text(u8 cx, u8 y, u16 v, u8 fg)
{
    cx = txt_num(cx, y, v / 100, fg, COL_DBLUE);
    cx = txt_draw(cx, y, ".", fg, COL_DBLUE);
    if (v % 100 < 10) cx = txt_draw(cx, y, "0", fg, COL_DBLUE);
    cx = txt_num(cx, y, v % 100, fg, COL_DBLUE);
    txt_id(cx, y, T_OPS_PT, fg, COL_DBLUE);
}

static void meeting_box(void)
{
    u8 k, cx, y;
    ui_box(GX, BOX_Y, GW, BOX_H, COL_DBLUE, COL_YELLOW);
    cx = line(0, T_OPS_MEETING, COL_YELLOW);
    cx = txt_num((u8)(cx + 1), row_y(0), mt.day, COL_YELLOW, COL_DBLUE);
    txt_id(cx, row_y(0), T_OPS_DAYS, COL_YELLOW, COL_DBLUE);
    line(1, mt.text, COL_WHITE);
    line(2, (u16)(mt.text + 1), COL_WHITE);
    line(3, (u16)(mt.text + 2), COL_CYAN);
    if (mt.n) {
        for (k = 0; k < mt.n; k++) {
            y = row_y((u8)(4 + k));
            ui_key((u8)(k + 1), 16, (u8)(y + 2));
            txt_id(4, y, (u16)(mt.text + 3 + k), COL_WHITE, COL_DBLUE);
        }
        ui_guide(T_OPS_GUIDE_PICK);
        return;
    }
    /* 判定（ops_decide はもう済ませてある）: 利益 1432万円 DAU 240 → 1.87 点（基準 2.00 点） */
    y = row_y(4);
    cx = txt_id(2, y, T_OPS_PROFIT, COL_ORANGE, COL_DBLUE);
    cx = money((u8)(cx + 1), y, g_ops.profit_judge, COL_WHITE, COL_DBLUE);
    cx = txt_id((u8)(cx + 2), y, T_OPS_DAU, COL_GREEN, COL_DBLUE);
    txt_num((u8)(cx + 1), y, g_ops.dau_judge, COL_WHITE, COL_DBLUE);
    y = row_y(5);
    cx = txt_id(2, y, T_OPS_JUDGE, COL_LGRAY, COL_DBLUE);
    points_text((u8)(cx + 1), y, g_ops.points, COL_WHITE);
    cx = txt_id(17, y, T_OPS_NEED, COL_LGRAY, COL_DBLUE);
    points_text((u8)(cx + 1), y, OPS_CONT_POINTS, COL_LGRAY);
    line(6, g_ops.verdict == OPS_CONTINUE ? T_OPS_CONT : T_OPS_ENDV, g_ops.verdict == OPS_CONTINUE ? COL_GREEN : COL_RED);
    ui_guide(T_OPS_GUIDE_NEXT);
}

static void result_box(void)
{
    u8 cx, saved = (u8)(g_ops.verdict == OPS_CONTINUE), more = (u8)(profit() >= (s32)OPS_HIST_PROFIT);
    u16 boss = saved ? (more ? T_OPS_BOSS0 : T_OPS_BOSS1) : (more ? T_OPS_BOSS2 : T_OPS_BOSS3);
    ui_box(GX, BOX_Y, GW, BOX_H, COL_DBLUE, COL_YELLOW);
    line(0, T_OPS_RESULT, COL_YELLOW);
    cx = line(1, T_OPS_SALES, COL_ORANGE);
    money((u8)(cx + 1), row_y(1), (s32)g_ops.sales_total, COL_WHITE, COL_DBLUE);
    cx = line(2, T_OPS_AD, COL_CYAN);
    money((u8)(cx + 1), row_y(2), (s32)g_ops.ad_total, COL_WHITE, COL_DBLUE);
    cx = line(3, T_OPS_PROFIT, COL_YELLOW);
    money((u8)(cx + 1), row_y(3), profit(), COL_YELLOW, COL_DBLUE);
    cx = line(4, T_OPS_R_STAY, COL_GREEN);
    cx = txt_num((u8)(cx + 1), row_y(4), (u32)(g_ops.reg - g_ops.retired), COL_WHITE, COL_DBLUE);
    txt_id(cx, row_y(4), T_OPS_NIN, COL_LGRAY, COL_DBLUE);
    cx = line(5, T_OPS_R_BEST, COL_LGRAY);
    cx = money((u8)(cx + 1), row_y(5), (s32)g_save.ops_best, COL_WHITE, COL_DBLUE);
    if (newrec) txt_id((u8)(cx + 1), row_y(5), T_OPS_R_NEW, COL_YELLOW, COL_DBLUE);
    line(6, boss, COL_WHITE);
    ui_guide(T_OPS_GUIDE_NEXT);
}

/* 270 日を回し終えた: 最高の利益と「続けられた」をセーブに残す */
static void finish(void)
{
    s32 v = profit();
    newrec = (u8)(v > 0 && (u32)v > g_save.ops_best);
    if (newrec) g_save.ops_best = (u32)v;
    g_save.ops_flags |= (u8)(OPS_PLAYED | (g_ops.verdict == OPS_CONTINUE ? OPS_SAVED : 0));
    g_save_dirty = 1;
    st = ST_DONE;
    result_box();
}

/* 下の帯: 走っている間は ESC の案内。広告が止まったら、そのわけを次の会議まで出す */
static void guide_run(void)
{
    ad_seen = g_ops.ad_stop;
    ui_guide(ad_seen == OPS_AD_CVR ? T_OPS_AD_CVR : ad_seen == OPS_AD_SPENT ? T_OPS_AD_SPENT : T_OPS_GUIDE_RUN);
}

FAR(void, ops_init, (void))
{
    ops_start();
    st = ST_RUN;
    ad_seen = 0;
    pay_avg = 0;
    bgm_play(BGM_EVENT);
}

FAR(void, ops_draw, (void))
{
    ui_clear();
    ui_header(T_OPS_TITLE);
    kpi();
    graph();
    if (st == ST_MEET) meeting_box();
    else if (st == ST_DONE) result_box();
    else guide_run();
}

FAR(void, ops_update, (void))
{
    u8 p = input_pressed(), d = input_digit(), pt[HIST];
    VdpXfer x;
    if (st == ST_MEET) {
        if (mt.n ? (d >= 1 && d <= mt.n) : (p & BTN_OK) != 0) {
            psg_beep(0x080, 4);
            if (mt.n) ops_decide((u8)(d - 1));
            g_ops.ad_stop = 0;                 /* 会議を開いたら広告の知らせは消す */
            st = ST_RUN;
            g_dirty = 1;
        }
        return;
    }
    if (st == ST_DONE) {
        if (p & (BTN_OK | BTN_CANCEL)) screen_change(&scr_title);
        return;
    }
    if (p & BTN_CANCEL) {
        if (ui_confirm(T_OPS_QUIT)) { screen_change(&scr_title); return; }
        g_dirty = 1;
        return;
    }
    if (ops_meeting(&mt)) {
        if (!mt.n) ops_decide(0);          /* 判定の会議は選ばない（先に決めて、箱で見せる） */
        st = ST_MEET;
        psg_beep(0x100, 6);
        meeting_box();
        return;
    }
    {
        u8 meet = (u8)(g_ops.next && g_ops.day == mt.day);   /* 会議の日の点に印 */
        ops_step();
        points(pt);
        if (meet) pt[0] |= MEET;
        x.addr = (u16)(VRAM2_HIST + (g_ops.day - 1) * HIST); x.src = pt; x.len = HIST;
        vdp_page2_write(&x);
        plot((u16)(g_ops.day - 1), pt, last);
        kpi();
        if (g_ops.ad_stop != ad_seen) guide_run();
    }
    if (g_ops.day >= OPS_DAYS) finish();
}

SCREEN(scr_ops, ops_init, ops_update, ops_draw, 0)
