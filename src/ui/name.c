/* name.c — 名前入力。半角カナ・英数字の表から選ぶ（8 文字まで）。
 * 半角カナは MSX の文字コードと Shift-JIS の 1 バイトカナが同じなので、そのまま名前にできる。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"

#define NAME_MAX   8
#define COLS       16
#define CELLS      95           /* 94 文字 + 「決定」 */
#define CELL_END   (CELLS - 1)
#define GRID_Y     ROW(4)

/* 表の並び: ｱ〜ﾝ(B1-DD) ｧ〜ｯ(A7-AF) ｰ ﾞ ﾟ ｦ A〜Z 0〜9（{個数, 先頭の文字コード} の列） */
static const u8 runs[8][2] = {
    { 45, 0xB1 }, { 9, 0xA7 }, { 1, 0xB0 }, { 1, 0xDE }, { 1, 0xDF }, { 1, 0xA6 }, { 26, 'A' }, { 10, '0' },
};

static u8 cell_char(u8 i)
{
    u8 r;
    for (r = 0; r < 8; r++) {
        if (i < runs[r][0]) return (u8)(runs[r][1] + i);
        i -= runs[r][0];
    }
    return ' ';
}

static void draw_cell(u8 i, u8 on)
{
    u8 x = (u8)((i % COLS) * 2), y = (u8)(GRID_Y + (i / COLS) * LINE_H);
    u8 bg = on ? COL_BLUE : COL_BLACK;
    if (i == CELL_END) {
        txt_id(x, y, T_NAME_END, on ? COL_LYELLOW : COL_GREEN, bg);
    } else {
        char s[2];
        s[0] = (char)cell_char(i);
        s[1] = 0;
        txt_draw(x, y, " ", COL_WHITE, bg);
        txt_draw((u8)(x + 1), y, s, COL_WHITE, bg);
    }
}

static u8 name_len(void)
{
    u8 n = 0;
    while (n < NAME_MAX && g_save.name[n]) n++;
    return n;
}

static void draw_name(void)
{
    ui_fill_rows(ROW(2), 1, COL_DGREEN);
    txt_draw(8, ROW(2), g_save.name, COL_WHITE, COL_DGREEN);
}

FAR(void, name_init, (void))
{
    const char *def;
    u8 i = 0;
    if (g_save.name[0]) return;
    def = txt_ptr(T_NAME_DEFAULT);
    while (def[i] && i < NAME_MAX) { g_save.name[i] = def[i]; i++; }
    g_save.name[i] = 0;
    g_cursor = CELL_END;
}

FAR(void, name_draw, (void))
{
    u8 i;
    ui_clear();
    ui_title(T_NAME_PROMPT);
    draw_name();
    for (i = 0; i < CELLS; i++) draw_cell(i, (u8)(i == g_cursor));
    ui_footer(T_NAME_HELP);
}

static void move(u8 p)
{
    u8 old = g_cursor;
    if (p & BTN_LEFT)  g_cursor = (u8)((g_cursor + CELLS - 1) % CELLS);
    if (p & BTN_RIGHT) g_cursor = (u8)((g_cursor + 1) % CELLS);
    if (p & BTN_UP)    g_cursor = (u8)(g_cursor >= COLS ? g_cursor - COLS : g_cursor);
    if (p & BTN_DOWN)  g_cursor = (u8)(g_cursor + COLS < CELLS ? g_cursor + COLS : CELL_END);
    if (old != g_cursor) {
        draw_cell(old, 0);
        draw_cell(g_cursor, 1);
    }
}

FAR(void, name_update, (void))
{
    u8 p = input_pressed();
    u8 n = name_len();
    move(p);
    if (p & BTN_CANCEL) {
        if (n) g_save.name[n - 1] = 0;
        psg_beep(0x180, 3);
        draw_name();
    }
    if (!(p & BTN_OK)) return;
    psg_beep(0x80, 4);
    if (g_cursor == CELL_END) {
        if (n) screen_change(&scr_firstgacha);
        return;
    }
    if (n < NAME_MAX) {
        g_save.name[n] = (char)cell_char(g_cursor);
        g_save.name[n + 1] = 0;
        draw_name();
    }
}

SCREEN(scr_name, name_init, name_update, name_draw, 0)
