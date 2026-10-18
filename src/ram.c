/* ram.c — RAM 変数の実体。初期値は crt0 がゼロ化するので書かない */
#include "ram.h"

u8 g_cart_slot;
u8 g_bank_code, g_bank_dataA, g_bank_dataB;
VdpCmd g_vdpcmd;
u8 g_fontbuf[32];
u8 g_input_held, g_input_pressed;
u8 g_beep_frames;
DiskReq g_diskreq;
u8 g_sector[512];

SaveData g_save;
u8 g_save_slot;
u8 g_save_status;
u8 g_time_status;
u32 g_time_added;

const Screen *g_screen_cur, *g_screen_next;
u8 g_dirty;
u16 g_boot_magic;
u8 g_cursor;
u8 g_menu_pos;
u8 g_ui_prev;
