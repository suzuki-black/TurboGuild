/* savedisk.h — セーブディスクの識別・A/B スロットの読み書き（仕様 §5.1） */
#ifndef SAVEDISK_H
#define SAVEDISK_H
#include "../types.h"
#define SD_LOADED   0   /* 読めた */
#define SD_NEW      1   /* 識別はあるがセーブが無い／壊れている */
#define SD_NOID     2   /* セーブディスクではない（初期化が要る） */
#define SD_NODISK   3   /* ディスク無し・FDD 無し */
#define SD_ERROR    4

void savedisk_boot(void);     /* 起動時: ロードしてゲーム時間を進める */
u8   savedisk_save(void);     /* 戻り値 DISK_* */
#endif
