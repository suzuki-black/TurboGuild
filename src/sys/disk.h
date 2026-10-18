/* disk.h — PHYDIO 経由の論理セクタ読み書き（仕様 §5.1）。ファイルシステムは使わない */
#ifndef DISK_H
#define DISK_H
#include "../types.h"
#define DISK_OK      0
#define DISK_WPROT   1
#define DISK_NODISK  2
#define DISK_CRC     3
#define DISK_ERROR   4
#define DISK_NOFDD   5
u8 disk_read(u16 sector, u8 count, u8 *buf);
u8 disk_write(u16 sector, u8 count, u8 *buf);
#endif
