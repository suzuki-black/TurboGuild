/* SPDX-License-Identifier: MIT */
/* deco_ui.c — 待ち受け・着うた（DECISIONS #113）。お知らせの下のリンク（6）から。当時の携帯サイトの「特典ダウンロード」。
 * 思い出（core/memo.h）が報酬になり、待ち受け（タイトルに並ぶ 4 枚）と着うた（タイトルの曲）が手に入る。
 * 選んだものは g_save.flags と g_save.ops_flags の上の 4 ビットに置く（静的 RAM を増やさない, save.h）。
 * サービスが終わるとリンクは無くなるが、選んだものはタイトルに残る。
 * 左の列が待ち受け、右の列が着うた。g_cursor = 列 × DC_N + 行。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../config.h"
#include "../core/memo.h"
#include "../core/save.h"
#include "../core/tables.h"
#include "../data/cards.h"
#include "../data/bgm_data.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

void bgm_play(u8 song);

#define DC_N    7            /* 待ち受け・着うたそれぞれの数 */
#define DC_Y    36
#define DC_H    16
#define DC_PREV 150          /* 待ち受けの見本（小さい絵 28x36）か、手に入れる条件 */
#define NO_MEMO 0xFF

static const u8 wall_memo[DC_N] = { NO_MEMO, MEMO_LV10, MEMO_SSR, MEMO_RAID, MEMO_GVGWIN, MEMO_TOP100, MEMO_TOP10 };
static const u8 tone_memo[DC_N] = { NO_MEMO, MEMO_STAGE100, MEMO_LV50, MEMO_PAY, MEMO_TOP100, MEMO_GVGWIN, MEMO_LASTDAY };
static const u8 tone_song[DC_N] = { BGM_HOME, BGM_QUEST, BGM_BOSS, BGM_GACHA, BGM_EVENT, BGM_GAIKA, BGM_LASTDAY };
static const u8 walls[5][4] = {
    { 39, 33, 34, 40 },          /* 0 看板（タイトルの元の絵） */
    { 43, 50, 53, 58 },          /* 3 イベントの強敵 */
    { 118, 120, 114, 61 },       /* 4 ギルドの英雄 */
    { 62, 63, 64, 65 },          /* 5 USR */
    { 121, 116, 58, 115 },       /* 6 XUSR */
};

/* 待ち受け wall の 4 枚。1 = マイデッキ（いまのデッキの先頭 4 枚）、2 = 自慢の 4 枚（レア度の高い順）。空きは看板の絵で埋める */
FAR(void, deco_cards, (u16 wall, u16 *ids))
{
    u8 k, j;
    for (k = 0; k < 4; k++) ids[k] = walls[0][k];
    if (wall >= DC_N) return;
    if (wall == 1) {
        for (k = 0; k < 4 && k < DECK_SIZE; k++)
            if (g_save.deck[k] != DECK_EMPTY && g_save.deck[k] < g_save.card_count) ids[k] = g_save.cards[g_save.deck[k]].id;
    } else if (wall == 2) {
        u16 i, best[4];
        u8 r[4];
        CardRec c;
        for (k = 0; k < 4; k++) r[k] = 0;
        for (i = 0; i < g_save.card_count && i < CARD_MAX; i++) {     /* 同じカードは 1 枚に（並べて見せるので） */
            u16 id = g_save.cards[i].id;
            if (!tbl_card(id, &c)) continue;
            for (k = 0; k < 4 && !(r[k] && best[k] == id); k++) ;
            if (k < 4) continue;
            for (k = 4; k && r[k - 1] < c.rarity + 1; k--) if (k < 4) { r[k] = r[k - 1]; best[k] = best[k - 1]; }
            if (k < 4) { r[k] = (u8)(c.rarity + 1); best[k] = id; }
        }
        for (k = 0; k < 4; k++) if (r[k]) ids[k] = best[k];
    } else if (wall) {
        j = (u8)(wall - 2);
        for (k = 0; k < 4; k++) ids[k] = walls[j][k];
    }
}

/* タイトルで流す曲 */
FAR(u8, deco_song, (void))
{
    u8 t = (u8)(g_save.ops_flags >> 4);
    return t < DC_N ? tone_song[t] : BGM_HOME;
}

static u8 got(u8 col, u8 i)
{
    u8 m = col ? tone_memo[i] : wall_memo[i];
    return (u8)(m == NO_MEMO || g_save.memo[m]);
}

static u8 now_of(u8 col) { return (u8)((col ? g_save.ops_flags : g_save.flags) >> 4); }

static void cell(u8 c)
{
    u8 col = (u8)(c / DC_N), i = (u8)(c % DC_N), x = (u8)(col * 16), y = (u8)(DC_Y + i * DC_H), on = (u8)(c == g_cursor);
    u8 has = got(col, i), bg = on ? COL_BLUE : (i & 1 ? COL_BLACK : COL_DGRAY);
    vdp_fill((u16)x * 8, y, 128, DC_H, bg);
    if (has && now_of(col) == i) txt_id(x, y, T_HP_MARK, COL_YELLOW, bg);
    txt_id((u8)(x + 1), y, has ? (u16)((col ? T_DC_TONE0 : T_DC_WALL0) + i) : T_TT_LOCK, has ? COL_WHITE : COL_LGRAY, bg);
}

static void preview(void)
{
    u8 col = (u8)(g_cursor / DC_N), i = (u8)(g_cursor % DC_N), k;
    vdp_fill(0, DC_PREV, SCREEN_W, ROW_LAST - DC_PREV, COL_BLACK);
    if (!got(col, i)) {                                        /* 手に入れる条件: 思い出の名前 */
        u8 cx = txt_id(1, (u8)(DC_PREV + 10), T_DC_NEED, COL_LGRAY, COL_BLACK);
        cx = txt_id(cx, (u8)(DC_PREV + 10), T_DC_Q0, COL_LGRAY, COL_BLACK);
        cx = txt_id(cx, (u8)(DC_PREV + 10), (u16)(T_MEMO0 + (col ? tone_memo[i] : wall_memo[i])), COL_CYAN, COL_BLACK);
        txt_id(cx, (u8)(DC_PREV + 10), T_DC_Q1, COL_LGRAY, COL_BLACK);
        return;
    }
    if (col) { txt_id(1, (u8)(DC_PREV + 10), T_DC_LISTEN, COL_LGRAY, COL_BLACK); return; }
    {
        u16 ids[4];
        deco_cards(i, ids);
        for (k = 0; k < 4; k++) gfx_thumb(ids[k], (u8)(64 + k * 34), DC_PREV + 2);
    }
}

FAR(void, deco_init, (void)) { g_cursor = (u8)(g_save.flags >> 4); if (g_cursor >= DC_N) g_cursor = 0; }

FAR(void, deco_draw, (void))
{
    u8 c;
    ui_clear();
    ui_header(T_DC_TITLE);
    txt_id(1, 18, T_DC_WALL, COL_CYAN, COL_BLACK);
    txt_id(17, 18, T_DC_TONE, COL_CYAN, COL_BLACK);
    for (c = 0; c < DC_N * 2; c++) cell(c);
    preview();
    ui_guide(T_DC_GUIDE);
}

FAR(void, deco_update, (void))
{
    u8 p = input_pressed(), old = g_cursor, col = (u8)(g_cursor / DC_N), i = (u8)(g_cursor % DC_N);
    g_ui_state = UIS_WAITKEY;
    if (p & BTN_CANCEL) { psg_beep(0x200, 4); bgm_play(BGM_HOME); screen_change(&scr_news); return; }
    if ((p & BTN_UP) && i) g_cursor--;
    if ((p & BTN_DOWN) && i < DC_N - 1) g_cursor++;
    if ((p & BTN_LEFT) && col) g_cursor -= DC_N;
    if ((p & BTN_RIGHT) && !col) g_cursor += DC_N;
    if (g_cursor != old) { psg_beep(0x100, 2); cell(old); cell(g_cursor); preview(); return; }
    if (p & BTN_OK) {
        if (!got(col, i)) { psg_beep(0x300, 6); return; }    /* まだ手に入れていない */
        if (col) { g_save.ops_flags = (u8)((g_save.ops_flags & 0x0F) | (i << 4)); bgm_play(tone_song[i]); }   /* 着うたは試しに鳴らす */
        else { g_save.flags = (u8)((g_save.flags & 0x0F) | (i << 4)); psg_beep(0x080, 6); }
        g_save_dirty = 1;
        for (i = 0; i < DC_N; i++) cell((u8)(col * DC_N + i));
    }
}

SCREEN(scr_deco, deco_init, deco_update, deco_draw, 0)
