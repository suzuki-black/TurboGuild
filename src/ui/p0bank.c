/* p0bank.c — バンク切替テスト。全ページを 3 つの窓へ順に出し、見出しのページ番号を照合する。
 * 窓 6000h の切り替えはカーネルの bank_probe() がやる（自分のバンクを外さないため）。 */
#include "screen.h"
#include "../far.h"
#include "../data/screens.h"
#include "../sys/vdp.h"
#include "../ram.h"
#include "../config.h"
#include "../data/rompages.h"
#include "../sys/bank.h"
#include "../sys/font.h"
#include "../sys/input.h"
#include "../sys/psg.h"

void back_on_cancel(void)
{
    if (input_pressed() & BTN_CANCEL) {
        psg_beep(0x180, 3);
        screen_change(&scr_p0menu);
    }
}

static void bank_draw_row(u8 row, const char *name, u8 window)
{
    u8 ok = 0;
    u8 bad = 0xFF;
    u8 p;
    for (p = 1; p < ROM_PAGE_COUNT; p++) {
        if (bank_probe(window, p)) ok++;
        else if (bad == 0xFF) bad = p;
    }
    font_puts(1, row, name, 7, 1);
    font_putdec(14, row, ok, 15, 1);
    font_puts(17, row, "/", 7, 1);
    font_putdec(18, row, ROM_PAGE_COUNT - 1, 15, 1);
    font_puts(22, row, bad == 0xFF ? "OK" : "NG", bad == 0xFF ? 3 : 8, 1);
    if (bad != 0xFF) font_putdec(25, row, bad, 8, 1);
}

FAR(void, p0bank_draw, (void))
{
    vdp_fill(0, 0, SCREEN_W, SCREEN_H, 1);
    dbg_header("BANK SWITCH TEST");
    font_puts(1, 3, "WINDOW       PAGES", 7, 1);
    bank_draw_row(5, "CODE  6000H", 0);
    bank_draw_row(6, "DATAA 8000H", 1);
    bank_draw_row(7, "DATAB A000H", 2);
    font_puts(1, 9, "MIRROR", 7, 1);
    font_putdec(8, 9, g_bank_code, 15, 1);
    font_putdec(11, 9, g_bank_dataA, 15, 1);
    font_putdec(14, 9, g_bank_dataB, 15, 1);
    dbg_footer("ESC:BACK");
}

FAR(void, p0bank_update, (void)) { back_on_cancel(); }
SCREEN(scr_banktest, 0, p0bank_update, p0bank_draw, 0)


void dbg_header(const char *title)
{
    vdp_fill(0, 0, SCREEN_W, 8, 4);
    font_puts(0, 0, title, 15, 4);
}

void dbg_footer(const char *help)
{
    vdp_fill(0, 200, SCREEN_W, 12, 4);
    font_puts(0, 25, help, 15, 4);
}
