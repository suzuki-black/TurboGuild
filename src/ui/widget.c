/* SPDX-License-Identifier: MIT */
/* widget.c — どの画面でも使う部品。カーネルに置くのでコードバンクからそのまま呼べる。
 * 呼び出し元のバンクの const（文言 id の表など）を読んでよい（窓を切り替えないので見える）。 */
#include "widget.h"
#include "../ram.h"
#include "../config.h"
#include "../core/tables.h"
#include "../data/text_ids.h"
#include "../sys/vdp.h"
#include "../sys/text.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/gfx.h"

static const u8 rar_col[4] = { COL_LGRAY, COL_CYAN, COL_YELLOW, COL_PURPLE };
static const u8 elem_col[5] = { COL_RED, COL_CYAN, COL_YELLOW, COL_WHITE, COL_PURPLE };

u8 rarity_color(u8 r) { return r < 4 ? rar_col[r] : COL_WHITE; }
u8 elem_color(u8 e)   { return e < 5 ? elem_col[e] : COL_WHITE; }

void ui_clear(void) { vdp_fill(0, 0, SCREEN_W, SCREEN_H, COL_BLACK); }


u8 ui_menu_input(u8 *cursor, u8 n, u8 pressed)
{
    if (pressed & BTN_UP)   { *cursor = (u8)((*cursor + n - 1) % n); psg_beep(0x100, 2); return 1; }
    if (pressed & BTN_DOWN) { *cursor = (u8)((*cursor + 1) % n); psg_beep(0x100, 2); return 1; }
    return 0;
}

/* 上下で 1 つ、左右で 1 画面ぶん動かす */
u8 ui_list_input(UiList *l, u8 pressed)
{
    u16 old_top = l->top, old = l->cursor;
    if (l->count == 0) return UL_NONE;
    if ((pressed & BTN_UP) && l->cursor > 0) l->cursor--;
    if ((pressed & BTN_DOWN) && l->cursor + 1 < l->count) l->cursor++;
    if (pressed & BTN_LEFT) l->cursor = l->cursor > l->rows ? l->cursor - l->rows : 0;
    if (pressed & BTN_RIGHT) {
        l->cursor += l->rows;
        if (l->cursor >= l->count) l->cursor = l->count - 1;
    }
    if (l->cursor < l->top) l->top = l->cursor;
    if (l->cursor >= l->top + l->rows) l->top = l->cursor - l->rows + 1;
    if (l->cursor == old) return UL_NONE;
    psg_beep(0x100, 2);
    return l->top != old_top ? UL_PAGE : UL_MOVED;
}

void ui_frame(void)
{
    vdp_wait_vsync();
    input_update();
    psg_tick();
}

void ui_wait(u8 frames)
{
    g_ui_state = UIS_BUSY;
    while (frames--) ui_frame();
}

u8 ui_wait_key(void)
{
    u8 p;
    g_ui_state = UIS_WAITKEY;
    do {
        ui_frame();
        p = input_pressed();
    } while (!(p & (BTN_OK | BTN_CANCEL)));
    g_ui_state = UIS_BUSY;
    return p;
}

u8 ui_rarity(u8 cx, u8 y, u8 rarity, u8 bg)
{
    return txt_id(cx, y, (u16)(T_RAR_0 + rarity), rarity_color(rarity), bg);
}

void ui_box(u8 x, u8 y, u8 w, u8 h, u8 bg, u8 border)
{
    vdp_fill(x, y, w, h, border);
    vdp_fill((u16)x + 2, (u16)y + 2, (u16)(w - 4), (u16)(h - 4), bg);
}

void ui_key(u8 digit, u8 x, u8 y)
{
    gfx_sprite((u8)(ATL_KEYS_X + digit * 12), ATL_KEYS_Y, 12, ATL_KEYS_H, x, y);
}
