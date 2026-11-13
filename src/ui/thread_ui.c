/* SPDX-License-Identifier: MIT */
/* thread_ui.c — 攻略スレの画面（core/thread.h, DECISIONS #104）。お知らせの 5 から。
 * 新しいレスが上。上下で 1 レスずつさかのぼる（さかのぼった数は g_cursor）。
 * ボーダーの点は「正解（data/border.h）× レスが言った %」。正解が 0 の順位（最終イベントの 500 位）は「誰でも入る」。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../config.h"
#include "../core/save.h"
#include "../core/tables.h"
#include "../core/thread.h"
#include "../data/border.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define TH_Y    18
#define TH_END  ROW_LAST
#define TH_BACK 200              /* さかのぼれるレスの数 */

typedef struct { char b[112]; u8 n; } Bld;   /* 組み立てたレスの本文（静的 RAM が残り少ないのでスタックに置く） */

static void put(Bld *w, const char *s)
{
    while (*s && w->n < sizeof(w->b) - 1) w->b[w->n++] = *s++;
    w->b[w->n] = 0;
}

static void put_num(Bld *w, u32 v)
{
    char s[11];
    u8 n = 10;
    s[10] = 0;
    do { s[--n] = (char)('0' + v % 10); v /= 10; } while (v && n);
    put(w, s + n);
}

/* 点を「2.1万」「21k」の形に（JA は 1 万、EN は 1000 で割る） */
static void put_pts(Bld *w, u32 v)
{
    u32 unit = GAME_LANG == LANG_EN_CODE ? 1000UL : 10000UL;
    if (v < unit) { put_num(w, v); return; }
    put_num(w, v / unit);
    if (v < unit * 10) {
        char d[3];
        d[0] = '.'; d[1] = (char)('0' + v % unit * 10 / unit); d[2] = 0;
        if (d[1] != '0') put(w, d);
    }
    put(w, txt_ptr(T_TH_UNIT));
}

/* 文の % に入れるもの */
static void compose(Bld *w, const ThPost *o)
{
    char tmp[80];
    const char *s;
    u8 n = 0, k;
    u16 id = o->text;
    u32 v = 0;
    if (o->kind == TH_BORDER) {
        v = th_border[o->ev - 1][o->tier];
        if (!v) id = T_TH_BDZ;                    /* 参加すれば入れる順位 */
        else v = v * o->pct / 100;
    }
    w->n = 0; w->b[0] = 0;
    if (o->ref) { put(w, ">>"); put_num(w, o->ref); put(w, " "); }
    s = txt_ptr(id);
    while (s[n] && n < sizeof(tmp) - 1) { tmp[n] = s[n]; n++; }
    tmp[n] = 0;
    for (k = 0; tmp[k]; k++) {
        if (tmp[k] != '%') { char c[2]; c[0] = tmp[k]; c[1] = 0; put(w, c); continue; }
        if (o->kind == TH_BORDER) put_pts(w, v);
        else if (o->card) { CardRec c; tbl_card(o->card, &c); put(w, txt_ptr(c.name_text)); }
    }
}

/* レスの頭の行: 123 名無しさん ID:aB3x */
static void head(u8 y, const ThPost *o, u8 bg)
{
    static const char al[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/";
    char id[8];
    u8 cx, k;
    u16 h = o->id;
    vdp_fill(0, y, SCREEN_W, 16, bg);
    cx = txt_num(1, y, o->no, COL_WHITE, bg);
    cx = txt_id((u8)(cx + 1), y, T_TH_NANASHI, COL_GREEN, bg);
    id[0] = 'I'; id[1] = 'D'; id[2] = ':';
    for (k = 0; k < 4; k++) { id[3 + k] = al[h & 63]; h = (u16)((h >> 6) | (h << 10)) ^ 0x5A5Au; }
    id[7] = 0;
    txt_draw((u8)(cx + 1), y, id, COL_LGRAY, bg);
}

/* 2 桁目から右端まで（30 桁）で折り返したときの行数（全角の字は行末の 1 桁に入らず次の行へ） */
static u8 rows(const char *t)
{
    const u8 *s = (const u8 *)t;
    u8 col = 0, n = 1;
    while (*s) {
        u8 wide = (u8)((*s >= 0x81 && *s <= 0x9F) || *s >= 0xE0);
        if (col + (wide ? 2 : 1) > 30) { n++; col = 0; }
        col = (u8)(col + (wide ? 2 : 1));
        s += wide ? 2 : 1;
    }
    return n;
}

FAR(void, thread_draw, (void))
{
    ThPost o;
    Bld w;
    u16 last = thread_last(), p;
    u8 y = TH_Y, cx, bg, i = 0;
    ui_clear();
    ui_header(T_TH_TITLE);
    cx = txt_id(16, 0, T_TH_PART, COL_LGRAY, COL_DBLUE);
    txt_num(cx, 0, last / TH_PART + 1, COL_YELLOW, COL_DBLUE);
    if (g_cursor > last) g_cursor = (u8)last;
    for (p = (u16)(last - g_cursor); y + 32 <= TH_END; p--, i++) {
        const char *s;
        thread_post(p, &o);
        compose(&w, &o);
        if (y + 16 * (1 + rows(w.b)) > TH_END) break;   /* 入り切らないレスは描かない */
        bg = i & 1 ? COL_BLACK : COL_DGRAY;
        head(y, &o, bg);
        y = (u8)(y + 16);
        s = w.b;
        while (*s && y + 16 <= TH_END) {
            vdp_fill(0, y, SCREEN_W, 16, bg);
            txt_draw(2, y, s, o.ref ? COL_CYAN : COL_WHITE, bg);
            s = g_txt_rest;
            y = (u8)(y + 16);
        }
        if (!p || o.no == 1) break;               /* スレの頭より前は前のスレ */
    }
    ui_guide(T_TH_GUIDE);
}

FAR(void, thread_update, (void))
{
    u8 p = input_pressed();
    if (p & (BTN_OK | BTN_CANCEL)) { psg_beep(0x200, 4); screen_change(&scr_news); return; }
    if ((p & BTN_DOWN) && g_cursor < TH_BACK && g_cursor < thread_last()) { g_cursor++; psg_beep(0x100, 2); g_dirty = 1; }
    if ((p & BTN_UP) && g_cursor) { g_cursor--; psg_beep(0x100, 2); g_dirty = 1; }
}

SCREEN(scr_thread, 0, thread_update, thread_draw, 0)
