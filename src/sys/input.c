/* input.c — キーマトリクス行 7/8 とジョイスティック 1 を読む。
 * ジョイスティックは PSG R#14 を直接読む（BIOS GTSTCK はキーと混ざるため）。 */
#include "input.h"
#include "bios.h"
#include "psg.h"
#include "../ram.h"

static u8 read_keys(void)
{
    u8 r8 = (u8)~bios_snsmat(8);
    u8 r7 = (u8)~bios_snsmat(7);
    u8 b = 0;
    if (r8 & 0x20) b |= BTN_UP;
    if (r8 & 0x40) b |= BTN_DOWN;
    if (r8 & 0x10) b |= BTN_LEFT;
    if (r8 & 0x80) b |= BTN_RIGHT;
    if ((r8 & 0x01) || (r7 & 0x80)) b |= BTN_OK;
    if (r7 & 0x04) b |= BTN_CANCEL;
    return b;
}

static u8 read_joy(void)
{
    u8 j = (u8)~psg_read_joy(0);
    u8 b = 0;
    if (j & 0x01) b |= BTN_UP;
    if (j & 0x02) b |= BTN_DOWN;
    if (j & 0x04) b |= BTN_LEFT;
    if (j & 0x08) b |= BTN_RIGHT;
    if (j & 0x10) b |= BTN_OK;
    if (j & 0x20) b |= BTN_CANCEL;
    return b;
}

void input_update(void)
{
    u8 now = read_keys() | read_joy();
    g_input_pressed = now & (u8)~g_input_held;
    g_input_held = now;
}

u8 input_held(void)    { return g_input_held; }
u8 input_pressed(void) { return g_input_pressed; }
