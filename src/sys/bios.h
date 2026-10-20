/* SPDX-License-Identifier: MIT */
/* bios.h — BIOS ラッパ（bios_asm.s）。sys/ の外からは呼ばない */
#ifndef BIOS_H
#define BIOS_H
#include "../types.h"

typedef struct {
    u8  write;      /* 0=読み 1=書き */
    u8  drive;
    u8  count;      /* セクタ数 */
    u8  media;      /* メディア ID (2DD 720KB = 0xF9) */
    u16 sector;
    u8 *buf;
} DiskReq;

void bios_chgmod(u8 mode);
u8   bios_snsmat(u8 row);
u8   bios_redclk(u8 reg);
u8   bios_phydio(DiskReq *r);
u8   bios_getcpu(void);

#define BIOS_CGTABL   (*(const u8 * const *)0x0004)
#define BIOS_MSXVER   (*(const volatile u8 *)0x002D)
#define SYS_HIMEM     (*(volatile u16 *)0xFC4A)
#define SYS_JIFFY     (*(volatile u16 *)0xFC9E)
#define SYS_RG8SAV    (*(volatile u8 *)0xFFE7)
#define SYS_RG9SAV    (*(volatile u8 *)0xFFE8)
#endif
