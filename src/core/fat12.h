/* SPDX-License-Identifier: MIT */
/* fat12.h — セーブディスクの FAT12（DECISIONS #82）。セーブはルートの TGSAVE.DAT（9 セクタ）に置き、
 * FAT とディレクトリにも正しく書くので、DOS でファイルをコピーしても壊れない（バックアップもできる）。
 * 読み書きは今までどおり PHYDIO の論理セクタ。ファイルの i 番目のセクタがディスクのどこかを g_fat.sec[i] に持つ。 */
#ifndef CORE_FAT12_H
#define CORE_FAT12_H
#include "../types.h"
#include "time.h"

#define FAT_FILE_SECS 9          /* 識別 1 + スロット A 4 + スロット B 4 */
#define FAT_LEGACY_SEC 16        /* 以前の版はセクタ 16 から直接書いていた */

#define FS_NONE   0              /* FAT12 のディスクだがセーブのファイルが無い（初回の保存で作る） */
#define FS_FILE   1              /* TGSAVE.DAT がある */
#define FS_LEGACY 2              /* ファイルは無いが、以前の版の形（セクタ 16〜24）で書かれている */
#define FS_BAD    3              /* FAT12 として読めない・ファイルが壊れている（保存できない） */

typedef struct {
    u8  state;
    u8  spc;                     /* 1 クラスタのセクタ数 */
    u8  nfat;
    u16 fat_start, spf;          /* FAT の先頭と 1 枚のセクタ数 */
    u16 root_start, root_secs;
    u16 data_start, nclus;       /* データ領域の先頭とクラスタ数（クラスタ番号は 2 から） */
    u16 sec[FAT_FILE_SECS];      /* ファイルの i 番目のセクタ → ディスクの論理セクタ */
    u16 cache;                   /* g_sector に入っているセクタ（無ければ FFFFh） */
    u8  dirty;                   /* g_sector を書き戻す必要がある */
} FatInfo;

extern FatInfo g_fat;
extern u8 g_sector[512];

u8 fat_open(void);                     /* ブートセクタとルートを読んで g_fat を作る。戻り値 DISK_* */
u8 fat_create(const DateTime *dt);     /* TGSAVE.DAT を作る（以前の版の形なら中身を移す）。戻り値 DISK_* */
#endif
