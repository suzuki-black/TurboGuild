/* SPDX-License-Identifier: MIT */
/* ending_ui.c — サービス終了の結末と思い出アルバム（DECISIONS #89）。
 * 結末: スタッフロールの前に 2 枚。1 枚目は遊び方で変わる結末文と数字、2 枚目は最後まで残った仲間とその別れの言葉。
 *   終了で一度だけ流れ（スタッフロールと同じく credits_seen）、タイトルの「スタッフロール」からも見られる。
 * アルバム: g_save.memo[] の出来事を日付の順に並べる（core/memo.h）。仲間が初めて去った日は伝言板の式から足す。
 *   サービス中はお知らせの [1]、終了後はタイトルの「思い出を見る」から。終了後は [1] でカード一覧（見るだけ）。
 * g_cursor をページ番号に使う。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/ending.h"
#include "../core/memo.h"
#include "../core/save.h"
#include "../core/tables.h"
#include "../core/timeline.h"
#include "../data/screens.h"
#include "../data/tables.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define END_TEXTS   5            /* 結末 1 つの文: 題・本文 4 行 */
#define STAY_SHOW   5            /* 2 枚目に出す仲間 */

static void npc_name(u8 cx, u8 y, u16 npc, u8 fg, u8 bg)
{
    NpcRec n;
    tbl_npc(npc, &n);
    cx = txt_id(cx, y, (u16)(NPC_PRE_TEXT + n.name_id / NPC_SUF_COUNT), fg, bg);
    txt_id(cx, y, (u16)(NPC_SUF_TEXT + n.name_id % NPC_SUF_COUNT), fg, bg);
}

static u8 stat_line(u8 y, u16 label, u32 v, u16 unit)
{
    u8 cx = txt_id(3, y, label, COL_LGRAY, COL_DBLUE);
    cx = txt_num((u8)(cx + 1), y, v, COL_WHITE, COL_DBLUE);
    return txt_id(cx, y, unit, COL_LGRAY, COL_DBLUE);
}

static void ending_page0(const EndingStats *e)
{
    u16 base = (u16)(T_END0 + e->kind * END_TEXTS);
    u8 i, cx;
    ui_header(T_END_HEAD);
    txt_id((u8)((32 - txt_id_width(base)) / 2), 30, base, COL_YELLOW, COL_BLACK);
    for (i = 1; i < END_TEXTS; i++) txt_id(2, (u8)(42 + i * 18), (u16)(base + i), COL_WHITE, COL_BLACK);
    ui_box(8, 136, 240, 56, COL_DBLUE, COL_DGRAY);
    stat_line(140, T_END_SPENT, e->spent, T_YEN);
    cx = stat_line(156, T_END_KINDS, e->kinds, T_END_SLASH);
    txt_num(cx, 156, e->kinds_all, COL_LGRAY, COL_DBLUE);
    stat_line(172, T_END_LEFT, (u32)(e->mates - e->stayed), T_PERSON);
}

static void ending_page1(const EndingStats *e)
{
    u8 i, cx, y;
    ui_header(T_END_STAY);
    cx = txt_num(24, 0, e->stayed, COL_YELLOW, COL_DBLUE);
    txt_id(cx, 0, T_PERSON, COL_LGRAY, COL_DBLUE);
    if (!e->stayed) { txt_id(2, 40, T_END_NONE, COL_LGRAY, COL_BLACK); return; }
    for (i = 0; i < e->stayed && i < STAY_SHOW; i++) {
        y = (u8)(22 + i * 34);
        npc_name(2, y, e->stay[i], COL_CYAN, COL_BLACK);
        cx = txt_id(4, (u8)(y + 16), T_RV_Q0, COL_WHITE, COL_BLACK);
        cx = txt_id(cx, (u8)(y + 16), (u16)(T_END_BYE0 + ((e->stay[i] >> 7) + g_save.guild) % 4), COL_WHITE, COL_BLACK);
        if (e->tone[i]) cx = txt_id(cx, (u8)(y + 16), (u16)(T_BOARD_TONE1 + e->tone[i] - 1), COL_WHITE, COL_BLACK);
        txt_id(cx, (u8)(y + 16), T_RV_Q1, COL_WHITE, COL_BLACK);
    }
}

FAR(void, ending_init, (void))
{
    g_cursor = 0;
}

FAR(void, ending_draw, (void))
{
    EndingStats e;
    ending_stats(&e);
    ui_clear();
    if (g_cursor) ending_page1(&e);
    else ending_page0(&e);
    ui_guide(T_OPS_GUIDE_NEXT);
}

FAR(void, ending_update, (void))
{
    u8 p = input_pressed();
    g_ui_state = UIS_WAITKEY;                        /* 押下待ち（自動試験が見る） */
    if (p & BTN_CANCEL) { screen_change(&scr_credits); return; }   /* ESC でスタッフロールへ飛ばす */
    if (!(p & BTN_OK)) return;
    psg_beep(0x80, 4);
    if (g_cursor) { screen_change(&scr_credits); return; }
    g_cursor = 1;
    g_dirty = 1;
}

SCREEN(scr_ending, ending_init, ending_update, ending_draw, 0)

/* ---- 思い出アルバム ---- */

#define ALB_ROWS   9            /* 下にページ番号を出すので 9 行 */
#define ALB_Y      20

typedef struct { u16 day; u16 text; } AlbumRow;

/* 日付の順に並べる（16 件ほどなので挿し込みで） */
static u8 album_rows(AlbumRow *r)
{
    EndingStats e;
    u16 today = (u16)((u32)g_save.g_time / GSEC_PER_DAY);
    u8 n = 0, k, j;
    for (k = 0; k <= MEMO_KINDS; k++) {
        u16 d;
        if (k < MEMO_KINDS) {
            if (!g_save.memo[k]) continue;
            d = (u16)(g_save.memo[k] - 1);
        } else {                                         /* 仲間が初めて去った日 */
            ending_stats(&e);
            if (e.first_leave == END_NEVER || e.first_leave >= today) continue;
            d = e.first_leave;
        }
        for (j = n; j && r[j - 1].day > d; j--) r[j] = r[j - 1];
        r[j].day = d;
        r[j].text = (u16)(T_MEMO0 + k);
        n++;
    }
    return n;
}

static u8 album_pages(u8 n) { return (u8)(n ? (n + ALB_ROWS - 1) / ALB_ROWS : 1); }

static u8 ended(void)
{
    TlState tl;
    timeline_state(&tl);
    return tl.ended;
}

FAR(void, album_init, (void))
{
    g_cursor = 0;
}

FAR(void, album_draw, (void))
{
    AlbumRow r[MEMO_KINDS + 1];
    u8 n = album_rows(r), i, cx, y, bg;
    ui_clear();
    ui_header(T_ALB_TITLE);
    cx = txt_num(24, 0, n, COL_YELLOW, COL_DBLUE);
    cx = txt_id(cx, 0, T_END_SLASH, COL_LGRAY, COL_DBLUE);
    txt_num(cx, 0, MEMO_KINDS + 1, COL_LGRAY, COL_DBLUE);
    if (!n) txt_id(2, 40, T_ALB_NONE, COL_LGRAY, COL_BLACK);
    for (i = 0; i < ALB_ROWS && g_cursor * ALB_ROWS + i < n; i++) {
        const AlbumRow *a = &r[g_cursor * ALB_ROWS + i];
        y = (u8)(ALB_Y + i * 17);
        bg = (u8)(i & 1 ? COL_BLACK : COL_DGRAY);
        vdp_fill(0, y, SCREEN_W, 16, bg);
        cx = txt_id(1, y, T_ALB_DAYP, COL_LGRAY, bg);           /* 日本語は「12日目」、英語は「Day 12」 */
        cx = txt_num((u8)(cx + (a->day + 1 < 10 ? 2 : a->day + 1 < 100 ? 1 : 0)), y, a->day + 1, COL_YELLOW, bg);
        txt_id(cx, y, T_ALB_DAYS, COL_LGRAY, bg);
        txt_id(10, y, a->text, COL_WHITE, bg);
    }
    if (album_pages(n) > 1) {
        cx = txt_num(26, 192 - 16, g_cursor + 1, COL_LGRAY, COL_BLACK);
        cx = txt_draw(cx, 192 - 16, "/", COL_LGRAY, COL_BLACK);
        txt_num(cx, 192 - 16, album_pages(n), COL_LGRAY, COL_BLACK);
    }
    ui_guide(ended() ? T_ALB_GUIDE_END : T_ALB_GUIDE);   /* 終了後は [1] でカード一覧 */
}

FAR(void, album_update, (void))
{
    AlbumRow r[MEMO_KINDS + 1];
    u8 p = input_pressed();
    u8 d = input_digit();
    if (p & (BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN)) {
        u8 np = album_pages(album_rows(r));
        u8 old = g_cursor;
        if (p & (BTN_RIGHT | BTN_DOWN)) g_cursor = (u8)((g_cursor + 1) % np);
        else g_cursor = (u8)((g_cursor + np - 1) % np);
        if (old != g_cursor) { psg_beep(0x100, 2); g_dirty = 1; }
        return;
    }
    if (d == 1 && ended()) { psg_beep(0x80, 4); g_viewonly = 1; screen_change(&scr_cardlist); return; }   /* カード一覧（見るだけ） */
    if (d == 3 && ended()) { psg_beep(0x80, 4); screen_change(&scr_mail); return; }   /* 仲間からの最後の手紙 */
    if (p & (BTN_CANCEL | BTN_OK)) screen_change(ended() ? &scr_title : &scr_news);
}

SCREEN(scr_album, album_init, album_update, album_draw, 0)
