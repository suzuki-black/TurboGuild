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

static const u8 rar_col[4] = { COL_GRAY, COL_LBLUE, COL_LYELLOW, COL_PINK };
static const u8 elem_col[5] = { COL_RED, COL_LBLUE, COL_YELLOW, COL_WHITE, COL_MAGENTA };

u8 rarity_color(u8 r) { return r < 4 ? rar_col[r] : COL_WHITE; }
u8 elem_color(u8 e)   { return e < 5 ? elem_col[e] : COL_WHITE; }

void ui_fill_rows(u8 y, u8 lines, u8 color)
{
    vdp_fill(0, y, SCREEN_W, (u16)lines * LINE_H, color);
}

void ui_clear(void) { vdp_fill(0, 0, SCREEN_W, SCREEN_H, COL_BLACK); }

void ui_title(u16 id)
{
    ui_fill_rows(0, 1, COL_BLUE);
    txt_id(1, 0, id, COL_WHITE, COL_BLUE);
}

void ui_footer(u16 id)
{
    vdp_fill(0, ROW_LAST, SCREEN_W, SCREEN_H - ROW_LAST, COL_BLUE);
    txt_id(1, ROW_LAST, id, COL_WHITE, COL_BLUE);
}

void ui_cursor(u8 cx, u8 y, u8 on)
{
    txt_draw(cx, y, on ? ">" : " ", COL_LYELLOW, COL_BLACK);
}

void ui_menu_draw(u8 cx, u8 y, const u16 *ids, u8 n, u8 cursor)
{
    u8 i;
    for (i = 0; i < n; i++) {
        u8 yy = (u8)(y + i * LINE_H);
        ui_cursor(cx, yy, (u8)(i == cursor));
        txt_id((u8)(cx + 2), yy, ids[i], i == cursor ? COL_LYELLOW : COL_WHITE, COL_BLACK);
    }
}

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

/* ★[SSR]炎 名前…………  攻撃 */
void ui_card_row(u8 y, u16 card_id, u8 lv, u8 mark, u8 bg)
{
    CardRec c;
    u8 cx;
    ui_fill_rows(y, 1, bg);
    if (!tbl_card(card_id, &c)) return;
    if (mark) txt_id(1, y, T_IN_DECK, COL_LYELLOW, bg);
    ui_rarity(3, y, c.rarity, bg);
    txt_id(7, y, (u16)(T_ELEM_0 + c.element), elem_color(c.element), bg);
    cx = txt_id(10, y, c.name_text, COL_WHITE, bg);
    (void)cx;
    txt_num_r(26, y, (u32)c.atk * (100 + (u16)lv * 5) / 100, 6, COL_GRAY, bg);
}
