/* SPDX-License-Identifier: MIT */
/* savedisk.c — セーブはルートの TGSAVE.DAT（core/fat12.c。DECISIONS #82）。ファイルの 0 セクタ目が識別、
 * 1〜4 がスロット A、5〜8 がスロット B（以前の版がセクタ 16〜24 に直接書いていたのと同じ並び）。
 * 書くたびに A/B を交互にし、読むときは CRC が通るうち g_time の大きい方を採る。
 * セーブ用バッファは g_save 1 面だけなので、比較のために読み直すことがある。 */
#include "savedisk.h"
#include "../far.h"
#include "../ram.h"
#include "../sys/disk.h"
#include "../sys/rtc.h"
#include "../core/time.h"
#include "../core/fat12.h"

#define FSEC_ID     0          /* ファイルの中のセクタ番号 */
#define FSEC_SLOT_A 1
#define SLOT_SECS   4

static const char id_text[] = "TGSAVE01 TurboGuild save disk. Do not copy files to this disk.";

static u8 has_id(void)
{
    u8 i;
    for (i = 0; i < 8; i++) {
        if (g_sector[i] != (u8)id_text[i]) return 0;
    }
    return 1;
}

/* ★g_save へ直接セクタ単位で読み書きしない。構造体は 512 の倍数ではないので、
 *   後ろの RAM 変数まで保存・復元してしまう（メニュー位置がリセットを越えて残った）。
 *   1 セクタずつ g_sector を経由してコピーする。 */
static u8 slot_io(u8 write, u8 slot)
{
    u8 *p = (u8 *)&g_save;
    u16 left = sizeof(SaveData);
    u8 sec = (u8)(FSEC_SLOT_A + slot * SLOT_SECS);
    u8 e;
    while (left) {
        u16 n = left < 512 ? left : 512;
        u16 i;
        if (write) {
            for (i = 0; i < 512; i++) g_sector[i] = i < n ? p[i] : 0;
            e = disk_write(g_fat.sec[sec], 1, g_sector);
        } else {
            e = disk_read(g_fat.sec[sec], 1, g_sector);
            for (i = 0; i < n; i++) p[i] = g_sector[i];
        }
        if (e != DISK_OK) return e;
        p += n;
        left -= n;
        sec++;
    }
    return DISK_OK;
}

static u8 read_slot(u8 slot)
{
    if (slot_io(0, slot) != DISK_OK) return 0;
    if (!save_valid(&g_save)) return 0;
    save_upgrade(&g_save);                       /* 古い版のセーブは今の版へ */
    return 1;
}

static u8 load_best(void)
{
    u8 a = read_slot(0);
    u32 ta = g_save.g_time;
    u8 b = read_slot(1);
    if (!a && !b) return 0;
    if (a && (!b || ta >= g_save.g_time)) {
        read_slot(0);       /* g_save には B が入っているので A を読み直す */
        g_save_slot = 1;
    } else {
        g_save_slot = 0;
    }
    return 1;
}

FAR(void, savedisk_boot, (void))
{
    u32 now = rtc_now_secs();
    u8 e = fat_open();
    g_time_status = TIME_OK;
    g_time_added = 0;
    if (e == DISK_OK && (g_fat.state == FS_FILE || g_fat.state == FS_LEGACY)) e = disk_read(g_fat.sec[FSEC_ID], 1, g_sector);
    if (e != DISK_OK) {
        g_save_status = (e == DISK_NODISK || e == DISK_NOFDD) ? SD_NODISK : SD_ERROR;
    } else if (g_fat.state == FS_BAD) {
        g_save_status = SD_ERROR;               /* FAT12 として読めないディスク（保存しない） */
    } else if (g_fat.state == FS_NONE || !has_id()) {
        g_save_status = SD_NOID;
    } else if (load_best()) {
        u32 before = g_save.g_time;
        g_save.g_time = time_advance(before, g_save.real_time_last, now, &g_time_status);
        g_save.real_time_last = now;
        g_time_added = g_save.g_time - before;
        g_save_status = SD_LOADED;
        return;
    } else {
        g_save_status = SD_NEW;
    }
    save_new(&g_save, now);
}

static u8 write_id(void)
{
    u16 i;
    for (i = 0; i < 512; i++) g_sector[i] = 0;
    for (i = 0; id_text[i]; i++) g_sector[i] = (u8)id_text[i];
    return disk_write(g_fat.sec[FSEC_ID], 1, g_sector);
}

static u8 create_file(void)
{
    DateTime dt;
    u8 e;
    rtc_read(&dt);
    e = fat_create(&dt);
    return e != DISK_OK ? e : write_id();
}

/* ★書く前に毎回ディスクを読み直す。覚えているセクタへそのまま書くと、入れ替えられた別の FD を壊す。
 * セーブのファイルが無いディスクには、確かめるまで書かない（DECISIONS #83）。 */
static u8 check_disk(u8 allow_new)
{
    u8 e = fat_open();
    if (e != DISK_OK) return e;
    switch (g_fat.state) {
    case FS_FILE:
        e = disk_read(g_fat.sec[FSEC_ID], 1, g_sector);
        if (e != DISK_OK || has_id()) return e;
        if (!allow_new) return DISK_NEWFILE;    /* 同じ名前の別のファイルか、作りかけ */
        return write_id();
    case FS_LEGACY:
        return create_file();                   /* 以前の版のセーブディスク: その場所をファイルとして登録する */
    case FS_NONE:
        return allow_new ? create_file() : DISK_NEWFILE;
    default:
        return DISK_ERROR;
    }
}

FAR(u8, savedisk_save, (u8 allow_new))
{
    u8 e = check_disk(allow_new);
    if (e != DISK_OK) return e;
    if (g_save_status != SD_LOADED) g_save_status = SD_NEW;
    g_save.real_time_last = rtc_now_secs();
    g_save.save_count++;
    save_seal(&g_save);
    e = slot_io(1, g_save_slot);
    if (e == DISK_OK) g_save_slot ^= 1;
    return e;
}

/* RTC を読んでゲーム時間を進める（メインループから 1 秒に 1 回） */
FAR(void, time_sync, (void))
{
    u32 now = rtc_now_secs();
    u8 st;
    g_save.g_time = time_advance(g_save.g_time, g_save.real_time_last, now, &st);
    g_save.real_time_last = now;
}
