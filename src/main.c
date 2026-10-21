/* SPDX-License-Identifier: MIT */
/* main.c — 起動後の初期化と画面ループ。crt0 の boot_main から呼ばれ、戻らない */
#include "ram.h"
#include "config.h"
#include "data/rompages.h"
#include "sys/bank.h"
#include "sys/vdp.h"
#include "sys/bgmdrv.h"
#include "sys/psg.h"
#include "sys/gfx.h"
#include "ui/screen.h"
#include "ui/savedisk.h"
#include "data/screens.h"

/* ページ HOTCODE を RAM の HOT_ADDR へ写す（MEMORY_MAP の hot: 行, DECISIONS #72）。窓 A だけを使う */
static void hot_load(void)
{
    const u8 *src = (const u8 *)0x8010;
    u8 *dst = (u8 *)HOT_ADDR;
    u16 i;
    bank_dataA(PAGE_HOTCODE);
    for (i = 0; i < HOT_CAP; i++) dst[i] = src[i];
}

void main(void)
{
    hot_load();              /* RAM で実行するコード（VDP・文字・BGM）を写す（★それらを使う何よりも先に） */
    vdp_init_screen5();
    bgm_install();           /* BGM を VBLANK 割り込みで進める */
    gfx_load_atlas();        /* 画面部品を VRAM ページ 1 へ（★忘れると部品が全部ただの四角になる） */
    psg_init();
    savedisk_boot();
    screen_change(&scr_title);
    g_boot_magic = BOOT_MAGIC;
    screen_run();
}
