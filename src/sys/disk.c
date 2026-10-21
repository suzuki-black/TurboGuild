/* SPDX-License-Identifier: MIT */
/* disk.c — 失敗は 3 回まで再試行し、エラーの種類を戻り値で返す。
 * ディスクROM が居ないと H.PHYD が空（C9h）なので、そのときは呼ばずに NOFDD を返す。 */
#include "disk.h"
#include "bios.h"
#include "bgmdrv.h"
#include "../ram.h"

#define H_PHYD   (*(volatile u8 *)0xFFA7)
#define MEDIA_2DD 0xF9
#define RETRY     3

static u8 map_error(u8 e)
{
    switch (e) {
    case 1:  return DISK_WPROT;    /* PHYDIO 0: 書込禁止 */
    case 3:  return DISK_NODISK;   /* 2: ドライブ準備できず */
    case 5:  return DISK_CRC;      /* 4: データ(CRC)エラー */
    default: return DISK_ERROR;
    }
}

static u8 transfer(u8 write, u16 sector, u8 count, u8 *buf)
{
    DiskReq *r = &g_diskreq;
    u8 e = 0;
    u8 i;
    if (H_PHYD == 0xC9) return DISK_NOFDD;
    r->write = write;
    r->drive = 0;
    r->count = count;
    r->media = MEDIA_2DD;
    r->sector = sector;
    r->buf = buf;
    for (i = 0; i < RETRY; i++) {
        bgm_hold();                    /* ディスク ROM が 4000h に出る間は BGM を止める */
        e = bios_phydio(r);
        bgm_release();
        if (e == 0) return DISK_OK;
        if (e == 1 || e == 3) break;   /* 書込禁止・未挿入は再試行しても同じ */
    }
    return map_error(e);
}

u8 disk_read(u16 sector, u8 count, u8 *buf)  { return transfer(0, sector, count, buf); }
u8 disk_write(u16 sector, u8 count, u8 *buf) { return transfer(1, sector, count, buf); }
