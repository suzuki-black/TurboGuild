/* news_ui.c — お知らせ（運営年表）と「みんなの動き」（NPC の出来事）、流れるお知らせの文を作る。
 * 文は文言の部品と数字・名前を RAM の 1 本の文字列に組み立てる（マイページのマーキーが流す）。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/ranking.h"
#include "../core/tables.h"
#include "../core/timeline.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define BUF_MAX (sizeof(g_ticker_buf) - 1)

static u8 len;

static void add_str(const char *s)
{
    while (*s && len < BUF_MAX) g_ticker_buf[len++] = *s++;
    g_ticker_buf[len] = 0;
}

static void add_text(u16 id) { add_str(txt_ptr(id)); }

static void add_num(u32 v)
{
    char d[11];
    u8 n = 0;
    do { d[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 10);
    while (n && len < BUF_MAX) g_ticker_buf[len++] = d[--n];
    g_ticker_buf[len] = 0;
}

static void add_npc(u16 npc)
{
    NpcRec r;
    tbl_npc(npc, &r);
    add_text((u16)(NPC_PRE_TEXT + r.name_id / NPC_SUF_COUNT));
    add_text((u16)(NPC_SUF_TEXT + r.name_id % NPC_SUF_COUNT));
}

/* 出来事 1 件を文にする */
static void feed_text(const FeedLine *f)
{
    CardRec c;
    add_npc(f->npc);
    switch (f->kind) {
    case FEED_SSR:    tbl_card(f->param, &c); add_text(T_FD_GA); add_text(c.name_text); add_text(T_FD_SSR); break;
    case FEED_BOSS:   add_text(T_FD_BOSS); break;
    case FEED_LEVEL:  add_text(T_FD_LEVEL); add_num(f->param); add_text(T_FD_LEVEL2); break;
    case FEED_RANKUP: add_text(T_FD_RANKUP); add_num(f->param); add_text(T_FD_RANKUP2); break;
    default:          add_text(T_FD_LOGIN); break;
    }
}

/* マーキーの次の 1 本: 年表の最新のお知らせと、みんなの動きを交互に */
FAR(void, ticker_next, (void))
{
    FeedLine f;
    u16 tx;
    len = 0;
    g_ticker_buf[0] = 0;
    if ((g_ticker_k & 3) == 0 && timeline_news(0, &tx)) {
        add_text(tx);
    } else {
        feed_line((u16)(g_ticker_k >> 1), &f);
        feed_text(&f);
    }
    g_ticker_k++;
}

/* g_ticker_buf を 32 桁で折り返して描く。次の y を返す（limit を超えたら止める） */
static u8 wrapped(u8 y, u8 fg, u8 bg, u8 limit)
{
    const char *s = g_ticker_buf;
    while (*s && y + 16 <= limit) {
        vdp_fill(0, y, SCREEN_W, 16, bg);
        txt_draw(0, y, s, fg, bg);
        s = g_txt_rest;
        y = (u8)(y + 16);
    }
    return y;
}

FAR(void, news_draw, (void))
{
    FeedLine f;
    u16 tx;
    u8 k, y, cx;
    ui_clear();
    ui_header(T_NEWS_TITLE);
    cx = txt_id(20, 0, T_PLAYERS, COL_LGRAY, COL_DBLUE);
    cx = txt_num(cx, 0, npc_active_count() + 1, COL_YELLOW, COL_DBLUE);
    txt_id(cx, 0, T_PERSON, COL_LGRAY, COL_DBLUE);
    for (k = 0, y = 18; k < 3 && timeline_news(k, &tx); k++) {
        len = 0;
        add_text(tx);
        y = wrapped(y, k == 0 ? COL_YELLOW : COL_WHITE, k & 1 ? COL_BLACK : COL_DGRAY, 98);
    }
    vdp_fill(0, 100, SCREEN_W, 16, COL_DBLUE);
    gfx_sprite(ATL_ICON_NEWS, 2, 96);
    txt_id(4, 100, T_FEED_TITLE, COL_WHITE, COL_DBLUE);
    for (k = 0, y = 118; k < 4; k++) {
        len = 0;
        feed_line(k, &f);
        feed_text(&f);
        y = wrapped(y, COL_WHITE, k & 1 ? COL_BLACK : COL_DGRAY, (u8)(y + 16));   /* 流れは 1 件 1 行 */
    }
    ui_guide(T_HELP_BACK);
}

FAR(void, news_update, (void))
{
    if (input_pressed() & (BTN_OK | BTN_CANCEL)) screen_change(&scr_home);
}

SCREEN(scr_news, 0, news_update, news_draw, 0)
