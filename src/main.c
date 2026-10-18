/* main.c — 起動後の初期化と画面ループ。crt0 の boot_main から呼ばれ、戻らない */
#include "ram.h"
#include "config.h"
#include "sys/vdp.h"
#include "sys/psg.h"
#include "ui/screen.h"
#include "ui/savedisk.h"
#include "data/screens.h"

void main(void)
{
    vdp_init_screen5();
    psg_init();
    savedisk_boot();
    screen_change(&scr_title);
    g_boot_magic = BOOT_MAGIC;
    screen_run();
}
