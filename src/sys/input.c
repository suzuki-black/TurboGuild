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

/* キーマトリクス行 0 = 0〜7, 行 1 の bit0/1 = 8, 9 */
static u16 read_digits(void)
{
    return (u16)((u8)~bios_snsmat(0) | (((u8)~bios_snsmat(1) & 3) << 8));
}

void input_update(void)
{
    u16 d = read_digits();
    u16 edge = d & (u16)~g_digit_held;
    u8 i;
    g_digit_held = d;
    g_digit_pressed = NO_DIGIT;
    for (i = 0; i < 10; i++) {
        if (edge & (1u << i)) { g_digit_pressed = i; break; }
    }
    if (g_test_digit) {                    /* 自動試験の数字キー（数字 + 1。0 = なし） */
        g_digit_pressed = (u8)(g_test_digit - 1);
        g_test_digit = 0;
    }
    u8 now = read_keys() | read_joy() | g_test_keys;   /* g_test_keys: 自動テストが 1 フレームだけ押す */
    g_test_keys = 0;
    g_input_pressed = now & (u8)~g_input_held;
    g_input_held = now;
}

u8 input_held(void)    { return g_input_held; }
u8 input_pressed(void) { return g_input_pressed; }
u8 input_digit(void)   { return g_digit_pressed; }
