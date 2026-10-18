/* ram.h — RAM に置く変数はすべてここに宣言する（仕様 §11）。実体は ram.c。
 * 合計は make の最後に "RAM used" として表示される。 */
#ifndef RAM_H
#define RAM_H
#include "types.h"
#include "sys/vdp.h"
#include "sys/bios.h"
#include "core/save.h"
#include "ui/screen.h"

/* sys/ */
extern u8 g_cart_slot;
extern u8 g_bank_code, g_bank_dataA, g_bank_dataB;
extern VdpCmd g_vdpcmd;
extern u8 g_fontbuf[32];
extern u8 g_input_held, g_input_pressed;
extern u8 g_beep_frames;
extern DiskReq g_diskreq;
extern u8 g_sector[512];

/* セーブ */
extern SaveData g_save;
extern u8 g_save_slot;         /* 次に書くスロット 0=A 1=B */
extern u8 g_save_status;       /* 起動時ロードの結果 */
extern u8 g_time_status;       /* 時刻差分の結果 TIME_* */
extern u32 g_time_added;       /* 起動時に加算したゲーム秒 */

/* 画面 */
extern const Screen *g_screen_cur, *g_screen_next;
extern u8 g_dirty;
extern u16 g_boot_magic;      /* 起動完了の目印（自動テストが待つ） */
extern u8 g_cursor;
extern u8 g_menu_pos;         /* メニューへ戻ったときのカーソル位置 */
extern u8 g_ui_prev;         /* 画面ごとの前回値（再描画の判定用） */

/* crt0.s が置く静的 RAM の終端 */
extern const u16 ram_end;
#endif
