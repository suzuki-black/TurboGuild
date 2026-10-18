/* deck_ui.c — デッキ編成。5 枠 + 「おまかせ編成」。枠を選ぶとカードを選ぶ一覧へ。
 * コスト上限を超える入れ替えは断る。別の枠にいるカードを選んだら入れ替える。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/deck.h"
#include "../core/inventory.h"
#include "../core/player.h"
#include "../core/tables.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"

#define SLOT_Y ROW(2)
#define ITEMS  (DECK_SIZE + 1)

static void slot_row(u8 i)
{
    u8 y = (u8)(SLOT_Y + i * LINE_H);
    u8 on = (u8)(i == g_cursor);
    u8 bg = on ? COL_BLUE : COL_BLACK;
    u8 idx;
    if (i == DECK_SIZE) {
        ui_fill_rows((u8)(y + LINE_H), 1, bg);
        txt_id(3, (u8)(y + LINE_H), T_DECK_AUTO, COL_LYELLOW, bg);
        return;
    }
    idx = g_save.deck[i];
    if (idx == DECK_EMPTY) {
        ui_fill_rows(y, 1, bg);
        txt_id(10, y, T_EMPTY, COL_GRAY, bg);
    } else {
        ui_card_row(y, g_save.cards[idx].id, g_save.cards[idx].lv, 0, bg);
    }
    txt_num(0, y, i + 1, COL_GRAY, bg);
}

static void totals(void)
{
    DeckStats d;
    u8 cx;
    d.enemy_elem = ELEM_NONE;
    deck_calc(&d);
    ui_fill_rows(ROW(9), 2, COL_BLACK);
    cx = txt_id(1, ROW(9), T_ATK, COL_GRAY, COL_BLACK);
    cx = txt_num((u8)(cx + 1), ROW(9), d.atk, COL_WHITE, COL_BLACK);
    cx = txt_id((u8)(cx + 2), ROW(9), T_DEF, COL_GRAY, COL_BLACK);
    txt_num((u8)(cx + 1), ROW(9), d.def, COL_WHITE, COL_BLACK);
    cx = txt_id(1, ROW(10), T_COST, COL_GRAY, COL_BLACK);
    cx = txt_num((u8)(cx + 1), ROW(10), d.cost, COL_WHITE, COL_BLACK);
    cx = txt_draw(cx, ROW(10), "/", COL_GRAY, COL_BLACK);
    txt_num(cx, ROW(10), PLAYER_COST_MAX(g_save.level), COL_GRAY, COL_BLACK);
}

FAR(void, deck_init, (void))
{
    g_cursor = g_deck_slot;
}

FAR(void, deck_draw, (void))
{
    u8 i;
    ui_clear();
    ui_title(T_DECK_TITLE);
    for (i = 0; i < ITEMS; i++) slot_row(i);
    totals();
    ui_footer(T_HELP_SELECT);
}

FAR(void, deck_update, (void))
{
    u8 p = input_pressed();
    u8 old = g_cursor;
    if (ui_menu_input(&g_cursor, ITEMS, p)) { slot_row(old); slot_row(g_cursor); }
    if (p & BTN_OK) {
        g_deck_slot = g_cursor;
        if (g_cursor == DECK_SIZE) {
            deck_auto();
            g_save_dirty = 1;
            psg_beep(0x080, 6);
            g_dirty = 1;
        } else {
            screen_change(&scr_deckpick);
        }
    }
    if (p & BTN_CANCEL) { g_deck_slot = 0; screen_change(&scr_cardlist); }
}

SCREEN(scr_deck, deck_init, deck_update, deck_draw, 0)

/* ---- カードを選ぶ: 先頭は「はずす」 ---- */

#define PICK_Y ROW(2)
#define PICK_N 9

static void pick_row(u16 i)
{
    u8 y = (u8)(PICK_Y + (i - g_list.top) * LINE_H);
    u8 bg = (i == g_list.cursor) ? COL_BLUE : COL_BLACK;
    if (i == 0) {
        ui_fill_rows(y, 1, bg);
        txt_id(3, y, T_DECK_CLEAR, COL_LYELLOW, bg);
        return;
    }
    ui_card_row(y, g_save.cards[i - 1].id, g_save.cards[i - 1].lv, inv_in_deck(i - 1), bg);
}

static void pick_rows(void)
{
    u16 i;
    ui_fill_rows(PICK_Y, PICK_N, COL_BLACK);
    for (i = g_list.top; i < g_list.count && i < g_list.top + PICK_N; i++) pick_row(i);
}

FAR(void, deckpick_init, (void))
{
    g_list.count = g_save.card_count + 1;
    g_list.rows = PICK_N;
    g_list.cursor = 0;
    g_list.top = 0;
}

FAR(void, deckpick_draw, (void))
{
    ui_clear();
    ui_title(T_DECK_PICK);
    ui_fill_rows(ROW(1), 1, COL_BLACK);
    pick_rows();
    totals();
    ui_footer(T_HELP_SELECT);
}

/* idx を枠 slot へ。別の枠にいれば入れ替え。コスト超過なら元に戻して 0 */
static u8 assign(u8 slot, u8 idx)
{
    DeckStats d;
    u8 before[DECK_SIZE];
    u8 i;
    for (i = 0; i < DECK_SIZE; i++) before[i] = g_save.deck[i];
    for (i = 0; i < DECK_SIZE; i++) if (g_save.deck[i] == idx) g_save.deck[i] = g_save.deck[slot];
    g_save.deck[slot] = idx;
    d.enemy_elem = ELEM_NONE;
    deck_calc(&d);
    if (d.cost <= PLAYER_COST_MAX(g_save.level)) return 1;
    for (i = 0; i < DECK_SIZE; i++) g_save.deck[i] = before[i];
    return 0;
}

FAR(void, deckpick_update, (void))
{
    u8 p = input_pressed();
    u16 old = g_list.cursor;
    u8 r = ui_list_input(&g_list, p);
    if (r == UL_PAGE) pick_rows();
    if (r == UL_MOVED) { pick_row(old); pick_row(g_list.cursor); }
    if (p & BTN_OK) {
        if (g_list.cursor == 0) {
            g_save.deck[g_deck_slot] = DECK_EMPTY;
        } else if (!assign(g_deck_slot, (u8)(g_list.cursor - 1))) {
            ui_msg(T_DECK_OVER);
            return;
        }
        g_save_dirty = 1;
        screen_change(&scr_deck);
    }
    if (p & BTN_CANCEL) screen_change(&scr_deck);
}

SCREEN(scr_deckpick, deckpick_init, deckpick_update, deckpick_draw, 0)
