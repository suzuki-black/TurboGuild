/* deck_ui.c — デッキ編成。上に 5 枠の絵（1 枠目がリーダー＝マイページの顔）、
 * 下に選んだ枠のカードと、デッキの攻撃・防御・コストのゲージ。[6] でおまかせ編成。
 * 枠を選ぶとカードの一覧（cardgrid）から入れるカードを選ぶ。コスト超過は断る。 */
#include "screen.h"
#include "widget.h"
#include "cardgrid.h"
#include "../far.h"
#include "../ram.h"
#include "../core/deck.h"
#include "../core/inventory.h"
#include "../core/player.h"
#include "../core/tables.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define SLOT_Y 20
#define ITEMS  (DECK_SIZE + 1)

static void slot(u8 i)
{
    u8 x = (u8)(4 + i * 42), on = (u8)(i == g_cursor);
    u8 idx;
    if (i == DECK_SIZE) {
        ui_box(214, SLOT_Y, 40, 48, on ? COL_BLUE : COL_DBLUE, on ? COL_YELLOW : COL_DGRAY);
        ui_key(6, 218, SLOT_Y + 4);
        gfx_sprite(ATL_ICON_DECK, 222, SLOT_Y + 18);
        return;
    }
    ui_box(x, SLOT_Y, 40, 48, on ? COL_BLUE : COL_DBLUE, on ? COL_YELLOW : COL_DGRAY);
    ui_key((u8)(i + 1), (u8)(x + 2), SLOT_Y + 2);
    idx = g_save.deck[i];
    if (idx != DECK_EMPTY && idx < g_save.card_count) gfx_thumb(g_save.cards[idx].id, (u8)(x + 6), SLOT_Y + 8);
    else gfx_part(ATL_CARDBACK_S, (u8)(x + 6), SLOT_Y + 8);
}

static void detail(void)
{
    CardRec c;
    u8 idx = g_cursor < DECK_SIZE ? g_save.deck[g_cursor] : DECK_EMPTY, cx;
    vdp_fill(0, 72, SCREEN_W, 76, COL_BLACK);
    if (g_cursor == DECK_SIZE) { txt_id(2, 80, T_DECK_AUTO, COL_YELLOW, COL_BLACK); return; }
    if (g_cursor == 0) txt_id(9, 74, T_LEADER, COL_YELLOW, COL_BLACK);
    if (idx == DECK_EMPTY || idx >= g_save.card_count) { txt_id(9, 92, T_EMPTY, COL_LGRAY, COL_BLACK); return; }
    tbl_card(g_save.cards[idx].id, &c);
    gfx_card(c.id, 4, 74);
    cx = ui_rarity(9, 92, c.rarity, COL_BLACK);
    txt_id((u8)(cx + 1), 92, c.name_text, COL_WHITE, COL_BLACK);
    cx = txt_id(9, 110, T_ATK, COL_LGRAY, COL_BLACK);
    cx = txt_num_r(cx, 110, card_atk_of(idx), 6, COL_YELLOW, COL_BLACK);
    cx = txt_id((u8)(cx + 1), 110, T_COST, COL_LGRAY, COL_BLACK);
    txt_num(cx, 110, c.cost, COL_WHITE, COL_BLACK);
    txt_id(9, 128, (u16)(T_SKILL_0 + c.skill), COL_CYAN, COL_BLACK);
}

static void totals(void)
{
    DeckStats d;
    u16 cmax = PLAYER_COST_MAX(g_save.level);
    u8 cx;
    deck_stats_init(&d, ELEM_NONE);
    deck_calc(&d);
    vdp_fill(0, 150, SCREEN_W, 42, COL_BLACK);
    cx = txt_id(0, 150, T_ATK, COL_RED, COL_BLACK);
    txt_num_r(cx, 150, d.atk, 6, COL_WHITE, COL_BLACK);
    cx = txt_id(16, 150, T_DEF, COL_BLUE, COL_BLACK);
    txt_num_r(cx, 150, d.def, 6, COL_WHITE, COL_BLACK);
    cx = txt_id(0, 170, T_COST, COL_LGRAY, COL_BLACK);
    cx = txt_num(cx, 170, d.cost, d.cost > cmax ? COL_RED : COL_WHITE, COL_BLACK);
    cx = txt_draw(cx, 170, "/", COL_LGRAY, COL_BLACK);
    txt_num(cx, 170, cmax, COL_LGRAY, COL_BLACK);
    gfx_gauge(96, 175, 152, d.cost, cmax, COL_ORANGE);
}

FAR(void, deck_init, (void))
{
    g_cursor = g_deck_slot;
}

FAR(void, deck_draw, (void))
{
    u8 i;
    ui_clear();
    ui_header(T_DECK_TITLE);
    for (i = 0; i < ITEMS; i++) slot(i);
    detail();
    totals();
    ui_guide(T_GUIDE_DECK);
}

static void choose(u8 i)
{
    g_deck_slot = i;
    if (i == DECK_SIZE) {
        deck_auto();
        g_save_dirty = 1;
        psg_beep(0x060, 8);
        g_dirty = 1;
        return;
    }
    g_list.cursor = 0;
    screen_change(&scr_deckpick);
}

FAR(void, deck_update, (void))
{
    u8 p = input_pressed();
    u8 d = input_digit();
    u8 old = g_cursor;
    if (p & (BTN_LEFT | BTN_UP)) g_cursor = (u8)((g_cursor + ITEMS - 1) % ITEMS);
    if (p & (BTN_RIGHT | BTN_DOWN)) g_cursor = (u8)((g_cursor + 1) % ITEMS);
    if (d >= 1 && d <= ITEMS) g_cursor = (u8)(d - 1);
    if (old != g_cursor) { psg_beep(0x100, 2); slot(old); slot(g_cursor); detail(); }
    if ((p & BTN_OK) || (d >= 1 && d <= ITEMS)) { choose(g_cursor); return; }
    if (p & BTN_CANCEL) { g_deck_slot = 0; screen_change(&scr_home); }
}

SCREEN(scr_deck, deck_init, deck_update, deck_draw, 0)

/* ---- 入れるカードを選ぶ ---- */

/* idx を枠 slot へ。別の枠にいれば入れ替え。コスト超過なら元に戻して 0 */
static u8 assign(u8 s, u8 idx)
{
    DeckStats d;
    u8 before[DECK_SIZE];
    u8 i;
    for (i = 0; i < DECK_SIZE; i++) before[i] = g_save.deck[i];
    for (i = 0; i < DECK_SIZE; i++) if (g_save.deck[i] == idx) g_save.deck[i] = g_save.deck[s];
    g_save.deck[s] = idx;
    deck_stats_init(&d, ELEM_NONE);
    deck_calc(&d);
    if (d.cost <= PLAYER_COST_MAX(g_save.level)) return 1;
    for (i = 0; i < DECK_SIZE; i++) g_save.deck[i] = before[i];
    return 0;
}

FAR(void, deckpick_init, (void))
{
    grid_setup(1);
}

FAR(void, deckpick_draw, (void))
{
    ui_clear();
    ui_header(T_DECK_PICK);
    grid_draw();
    ui_guide(T_GUIDE_LIST);
}

FAR(void, deckpick_update, (void))
{
    u8 p = input_pressed();
    grid_input(p);
    if (p & BTN_OK) {
        s16 i = grid_card();
        if (i < 0) {
            g_save.deck[g_deck_slot] = DECK_EMPTY;
        } else if (!assign(g_deck_slot, (u8)i)) {
            ui_msg(T_DECK_OVER);
            return;
        }
        g_save_dirty = 1;
        psg_beep(0x080, 4);
        screen_change(&scr_deck);
    }
    if (p & BTN_CANCEL) screen_change(&scr_deck);
}

SCREEN(scr_deckpick, deckpick_init, deckpick_update, deckpick_draw, 0)
