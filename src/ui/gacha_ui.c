/* gacha_ui.c — ガチャのバナー一覧（数字キー 1〜5）→ 1 回 / 10 連 → 開封演出 → 結果。
 * イベントガチャ（id 4）はイベントが始まる Phase2 まで出さない。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../data/bgm_data.h"
void bgm_play(u8 song);
#include "../ram.h"
#include "../core/gacha.h"
#include "../core/tables.h"
#include "../core/time.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

void gacha_show(u8 kind);

#define KINDS    g_kind_n            /* イベント開催中は 6 種（イベントガチャが加わる） */
#define BANNER_Y 18
#define BANNER_H (g_kind_n > 5 ? 28 : 34)
#define kinds    g_kinds

/* ガチャ id ごとのバナーの 2 色（縞）と、左の絵（添字 = id） */
static const u8 stripe[7][2] = {
    { 0, 0 }, { COL_DGREEN, COL_GREEN }, { COL_DRED, COL_ORANGE }, { COL_DBLUE, COL_PURPLE },
    { COL_DRED, COL_PURPLE }, { COL_DBLUE, COL_BLUE }, { COL_ORANGE, COL_YELLOW },
};
static const u8 art[7][4] = {
    { ATL_ICON_GACHA }, { ATL_ICON_GACHA }, { ATL_ITEM_GOLD }, { ATL_CIRCLE }, { ATL_CIRCLE },
    { ATL_SCROLL }, { ATL_ITEM_SRTICKET },
};
static const u8 cur_icon[5][2] = {
    { 0, 0 }, { ATL_ITEM_GOLD_X, ATL_ITEM_GOLD_Y }, { ATL_ITEM_GEMS_X, ATL_ITEM_GEMS_Y },
    { ATL_ITEM_TICKET_X, ATL_ITEM_TICKET_Y }, { ATL_ITEM_SRTICKET_X, ATL_ITEM_SRTICKET_Y },
};

static void banner(u8 i)
{
    GachaRec g;
    u8 y = (u8)(BANNER_Y + i * BANNER_H);
    u8 on = (u8)(i == g_cursor), k, cx;
    u8 id = kinds[i];
    u8 a = stripe[id][0], b = stripe[id][1];
    tbl_gacha(kinds[i], &g);
    vdp_fill(0, y, SCREEN_W, BANNER_H - 2, on ? COL_YELLOW : COL_DGRAY);
    for (k = 0; k < 4; k++) vdp_fill(2, (u16)(y + 2 + k * 6), SCREEN_W - 4, (u16)(k == 3 ? BANNER_H - 22 : 6), (u8)(k & 1 ? b : a));
    gfx_sprite(art[id][0], art[id][1], art[id][2] > 40 ? 40 : art[id][2], art[id][3] > 24 ? 24 : art[id][3], 6, (u8)(y + 2));
    ui_key((u8)(i + 1), 2, (u8)(y + 2));
    txt_id(6, (u8)(y + 1), g.name_text, on ? COL_YELLOW : COL_WHITE, a);
    y = (u8)(y + BANNER_H - 34);           /* 6 種のときは 2 行目を詰める */
    if (g.currency == CUR_FREE) {
        u8 used = (u8)(g_save.last_free_gacha_gday == time_gday(g_save.g_time));
        txt_id(6, (u8)(y + 16), used ? T_GACHA_FREE_USED : T_TODAY_FREE, used ? COL_LGRAY : COL_YELLOW, b);
        return;
    }
    gfx_sprite(cur_icon[g.currency][0], cur_icon[g.currency][1], 14, 14, 48, (u8)(y + 18));
    cx = txt_num(8, (u8)(y + 16), g.price, COL_WHITE, b);
    if (g.price10) {
        cx = txt_id((u8)(cx + 2), (u8)(y + 16), T_GACHA_10, COL_YELLOW, b);
        txt_num(cx, (u8)(y + 16), g.price10, COL_WHITE, b);
    }
    if (g.currency == CUR_SRTICKET) txt_id(20, (u8)(y + 16), T_SR_UP, COL_RED, b);
}

static void owned(void)
{
    u8 cx;
    vdp_fill(0, 188, SCREEN_W, 4, COL_BLACK);
    vdp_fill(0, ROW_LAST, SCREEN_W, 20, COL_DGRAY);
    gfx_sprite(ATL_ITEM_GOLD, 2, ROW_LAST + 1);
    cx = txt_num(2, ROW_LAST, g_save.gold, COL_YELLOW, COL_DGRAY);
    gfx_sprite(ATL_ITEM_GEMS, (u8)(cx * 8 + 8), ROW_LAST + 1);
    cx = txt_num((u8)(cx + 3), ROW_LAST, g_save.gems, COL_CYAN, COL_DGRAY);
    gfx_sprite(ATL_ITEM_TICKET, (u8)(cx * 8 + 8), ROW_LAST + 1);
    cx = txt_num((u8)(cx + 3), ROW_LAST, g_save.gacha_tickets, COL_WHITE, COL_DGRAY);
    gfx_sprite(ATL_ITEM_SRTICKET, (u8)(cx * 8 + 8), ROW_LAST + 1);
    txt_num((u8)(cx + 3), ROW_LAST, g_save.sr_tickets, COL_WHITE, COL_DGRAY);
}

FAR(void, gacha_init, (void))
{
    static const u8 base[5] = { 1, 2, 3, 5, 6 };
    u8 i;
    bgm_play(BGM_GACHA);
    g_kind_n = 0;
    for (i = 0; i < 5; i++) {
        kinds[g_kind_n++] = base[i];
        if (base[i] == 3 && g_save.event_id) kinds[g_kind_n++] = 4;   /* プレミアの次にイベントガチャ */
    }
    g_cursor = (u8)(g_sel_gacha < KINDS ? g_sel_gacha : 2);
}

FAR(void, gacha_draw, (void))
{
    u8 i;
    ui_clear();
    ui_header(T_GACHA_TITLE);
    for (i = 0; i < KINDS; i++) banner(i);
    owned();
}

static u8 ask_count(void)
{
    GachaRec g;
    u8 cur = 1, k;
    tbl_gacha(kinds[g_cursor], &g);
    if (!g.price10) return 1;
    for (;;) {
        u8 p;
        for (k = 0; k < 2; k++) {
            u8 x = (u8)(40 + k * 92);
            ui_box(x, 96, 84, 22, k == cur ? COL_BLUE : COL_DBLUE, k == cur ? COL_YELLOW : COL_LGRAY);
            txt_id((u8)(x / 8 + 1), 99, k ? T_PULL_10 : T_PULL_1, COL_WHITE, k == cur ? COL_BLUE : COL_DBLUE);
        }
        g_ui_state = UIS_WAITKEY;
        do { ui_frame(); p = input_pressed(); } while (!p);
        if (p & (BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN)) { cur ^= 1; psg_beep(0x100, 2); }
        if (p & BTN_CANCEL) { g_dirty = 1; return 0; }
        if (p & BTN_OK) break;
    }
    return (u8)(cur ? GACHA_ONE_MAX : 1);
}

static void pull(void)
{
    static const u16 errs[5] = { 0, T_GACHA_NO_MONEY, T_GACHA_FREE_USED, T_GACHA_FULL, T_GACHA_NA };
    u8 n = ask_count();
    if (!n) return;
    g_gacha.gacha = kinds[g_cursor];
    g_gacha.count = n;
    gacha_pull(&g_gacha);
    if (g_gacha.status != G_OK) { ui_msg(errs[g_gacha.status]); return; }
    g_save_dirty = 1;
    g_sel_gacha = g_cursor;
    screen_change(&scr_gacharesult);
}

FAR(void, gacha_update, (void))
{
    u8 p = input_pressed();
    u8 d = input_digit();
    u8 old = g_cursor;
    if (ui_menu_input(&g_cursor, KINDS, p)) { banner(old); banner(g_cursor); }
    if (d >= 1 && d <= KINDS) { g_cursor = (u8)(d - 1); banner(old); banner(g_cursor); pull(); return; }
    if (p & BTN_OK) pull();
    if (p & BTN_CANCEL) screen_change(&scr_home);
}

SCREEN(scr_gacha, gacha_init, gacha_update, gacha_draw, 0)

/* ---- 結果: 演出を流して、最後の画面のまま押下を待つ ---- */

FAR(void, gacharesult_draw, (void))
{
    ui_clear();
    ui_header(T_GACHA_RESULT);
    ui_guide(T_GUIDE_ANY);
    gacha_show(g_gacha.gacha);
    if (g_gacha.pity_hit) txt_id(1, 168, T_GACHA_PITY, COL_PURPLE, COL_BLACK);
}

FAR(void, gacharesult_update, (void))
{
    if (input_pressed() & (BTN_OK | BTN_CANCEL)) screen_change(&scr_gacha);
}

SCREEN(scr_gacharesult, 0, gacharesult_update, gacharesult_draw, 0)
