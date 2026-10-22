/* SPDX-License-Identifier: MIT */
/* ram.c — RAM 変数の実体。初期値は crt0 がゼロ化するので書かない */
#include "ram.h"

u8 g_cart_slot;
u8 g_bank_code, g_bank_dataA, g_bank_dataB;
VdpCmd g_vdpcmd;
u8 g_fontbuf[32];
u8 g_glyph[128];
const char *g_txt_rest;
u8 g_input_held, g_input_pressed;
u16 g_digit_held;
u8 g_digit_pressed;
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
u8 g_test_keys;
u8 g_test_digit;
u8 g_ui_state;
u8 g_frames;
u8 g_save_dirty;
u16 g_sel_area;
u16 g_sel_stage;
u16 g_sel_card;
u8 g_sel_gacha;
UiList g_list;
u8 g_deck_slot;
u8 g_replay;
u8 g_grid_remove;
u8 g_paid;
FuseReq g_fuse;
EventResult g_evres;
GvgResult g_gvgres;
RankList g_rank;
u8 g_kinds[6], g_kind_n;
u8 g_ticker, g_ticker_gap;
char g_ticker_buf[96];
u8 g_ticker_k;
u8 g_viewonly;
u16 g_gift;
u8  g_joined;
u16 g_cred_pos;
u8 g_from_title;
BgmCh g_bgm_ch[3];
u8 g_bgm_song, g_bgm_nch;
u16 g_bgm_base;
u16 g_bgm_ticks;
u16 g_bgm_clock, g_bgm_acc;
volatile u8 g_isr_safe;
u8 g_timi_stub[17];
u8 g_nosave_warned;
GachaReq g_gacha;
QuestResult g_qr;
LoginResult g_login;
u8 g_cursor;
u8 g_menu_pos;
u8 g_ui_prev;
