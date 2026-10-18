/* p0bank.c — バンク切替テスト。全ページを 3 つの窓へ順に出し、見出しのページ番号を照合する。
 * ★コードバンクの窓を自分で切り替えるので、このファイルは必ずカーネル（固定窓）に置く。 */
#include "screen.h"
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

static u8 check_window(const volatile u8 *win, u8 page)
{
    return (u8)(win[0] == page && win[1] == 'T' && win[2] == 'G' && win[3] == 'P');
}

static void bank_draw_row(u8 row, const char *name, const volatile u8 *win, void (*sel)(u8), u8 restore)
{
    u8 ok = 0;
    u8 bad = 0xFF;
    u8 p;
    for (p = 1; p < ROM_PAGE_COUNT; p++) {
        sel(p);
        if (check_window(win, p)) ok++;
        else if (bad == 0xFF) bad = p;
    }
    sel(restore);
    font_puts(1, row, name, 7, 1);
    font_putdec(14, row, ok, 15, 1);
    font_puts(17, row, "/", 7, 1);
    font_putdec(18, row, ROM_PAGE_COUNT - 1, 15, 1);
    font_puts(22, row, bad == 0xFF ? "OK" : "NG", bad == 0xFF ? 3 : 8, 1);
    if (bad != 0xFF) font_putdec(25, row, bad, 8, 1);
}

static void bank_draw(void)
{
    ui_clear();
    ui_header("BANK SWITCH TEST");
    font_puts(1, 3, "WINDOW       PAGES", 7, 1);
    bank_draw_row(5, "CODE  6000H", (const volatile u8 *)WIN_CODE, bank_code, g_bank_code);
    bank_draw_row(6, "DATAA 8000H", (const volatile u8 *)WIN_DATAA, bank_dataA, g_bank_dataA);
    bank_draw_row(7, "DATAB A000H", (const volatile u8 *)WIN_DATAB, bank_dataB, g_bank_dataB);
    font_puts(1, 9, "MIRROR", 7, 1);
    font_putdec(8, 9, g_bank_code, 15, 1);
    font_putdec(11, 9, g_bank_dataA, 15, 1);
    font_putdec(14, 9, g_bank_dataB, 15, 1);
    ui_footer("ESC:BACK");
}

const Screen scr_banktest = { 0, back_on_cancel, bank_draw, 0 };

