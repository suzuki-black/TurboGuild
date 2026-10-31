#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""openMSX 用の空のセーブディスク（2DD 720KB, FAT12 フォーマット済み）を作る（仕様 §5.1）。

    mkblank_dsk.py [--empty] <out.dsk>

ブートセクタのブートコードは RET だけにして、DOS 起動を試みないようにする。
TGSAVE.DAT（識別だけ入れたもの）を用意するので、ゲームは確認なしで保存できる。
--empty ならファイルも無い空のディスクにする。ゲームは最初の保存で作ってよいか聞く（DECISIONS #83）。
"""
import struct
import sys
from pathlib import Path

SECTORS = 1440
FILE_SECS = 9                     # 識別 1 + スロット A 4 + スロット B 4（src/core/fat12.h）
DATA_START = 14                   # ブート 1 + FAT 3 × 2 + ルート 7
ID_TEXT = b"TGSAVE01 TurboGuild save disk. Do not copy files to this disk."
BPS = 512
MEDIA = 0xF9


def boot_sector():
    b = bytearray(BPS)
    b[0:3] = b"\xEB\xFE\x90"
    b[3:11] = b"TGSAVE  "
    struct.pack_into("<HBHBHHBHHHH", b, 11,
                     BPS,      # bytes/sector
                     2,        # sectors/cluster
                     1,        # reserved sectors
                     2,        # FATs
                     112,      # root entries
                     SECTORS,  # total sectors
                     MEDIA,
                     3,        # sectors/FAT
                     9,        # sectors/track
                     2,        # heads
                     0)        # hidden
    b[0x1E] = 0xC9            # ブートコード: 何もせず戻る
    return b


def put_fat(img, cluster, value):
    for fat in (1, 4):
        o = fat * BPS + cluster * 3 // 2
        if cluster & 1:
            img[o] = (img[o] & 0x0F) | (value << 4 & 0xF0)
            img[o + 1] = value >> 4 & 0xFF
        else:
            img[o] = value & 0xFF
            img[o + 1] = (img[o + 1] & 0xF0) | (value >> 8 & 0x0F)


def add_save_file(img):
    """クラスタ 2〜6 に TGSAVE.DAT を置き、先頭のセクタに識別を書く"""
    for c in range(2, 7):
        put_fat(img, c, 0xFFF if c == 6 else c + 1)
    entry = bytearray(32)
    entry[0:11] = b"TGSAVE  DAT"
    entry[11] = 0x20
    struct.pack_into("<H", entry, 24, 0x21)    # 1980-01-01
    struct.pack_into("<H", entry, 26, 2)
    struct.pack_into("<I", entry, 28, FILE_SECS * BPS)
    img[7 * BPS:7 * BPS + 32] = entry
    img[DATA_START * BPS:DATA_START * BPS + len(ID_TEXT)] = ID_TEXT


def main(argv):
    empty = "--empty" in argv
    args = [a for a in argv[1:] if a != "--empty"]
    if len(args) != 1:
        sys.exit(__doc__)
    img = bytearray(SECTORS * BPS)
    img[0:BPS] = boot_sector()
    for fat in (1, 4):            # FAT は 1-3, 4-6
        img[fat * BPS:fat * BPS + 3] = bytes([MEDIA, 0xFF, 0xFF])
    if not empty:
        add_save_file(img)
    Path(args[0]).parent.mkdir(parents=True, exist_ok=True)
    Path(args[0]).write_bytes(img)
    print(f"mkblank_dsk: {args[0]} (720KB, media {MEDIA:02X}h)")


if __name__ == "__main__":
    main(sys.argv)
