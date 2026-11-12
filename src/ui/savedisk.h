/* SPDX-License-Identifier: MIT */
/* savedisk.h — セーブディスクの識別・A/B スロットの読み書き（仕様 §5.1） */
#ifndef SAVEDISK_H
#define SAVEDISK_H
#include "../types.h"
#define SD_LOADED   0   /* 読めた */
#define SD_NEW      1   /* 識別はあるがセーブが無い／壊れている */
#define SD_NOID     2   /* セーブディスクではない（初期化が要る） */
#define SD_NODISK   3   /* ディスク無し・FDD 無し */
#define SD_ERROR    4

void savedisk_boot(void);     /* 起動時: ロードしてゲーム時間を進める。時計が怪しければ進めずに g_time_status = TIME_ASK_* */
void savedisk_clock_done(u8 skip);
void clock_title(void);       /* タイトルの最下行に本体の時計と [9] 時刻修正（ui/clock_ui.c） */   /* 時計の画面の後: 留守の間の時間を足す（skip = 1 なら足さない） */
u8   savedisk_save(u8 allow_new);   /* 戻り値 DISK_*。allow_new = 0 ならファイルを作らず DISK_NEWFILE を返す */
#endif
