#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""openMSX 用の空のセーブディスク（2DD 720KB, FAT12 フォーマット済み）を作る（仕様 §5.1）。

    mkblank_dsk.py <out.dsk>

ブートセクタのブートコードは RET だけにして、DOS 起動を試みないようにする。
ファイルは無い。ゲームが初回の保存でルートに TGSAVE.DAT を作り、FAT にも登録する（DECISIONS #82）。
"""
import struct
import sys
from pathlib import Path

SECTORS = 1440
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


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    img = bytearray(SECTORS * BPS)
    img[0:BPS] = boot_sector()
    for fat in (1, 4):            # FAT は 1-3, 4-6
        img[fat * BPS:fat * BPS + 3] = bytes([MEDIA, 0xFF, 0xFF])
    Path(argv[1]).parent.mkdir(parents=True, exist_ok=True)
    Path(argv[1]).write_bytes(img)
    print(f"mkblank_dsk: {argv[1]} (720KB, media {MEDIA:02X}h)")


if __name__ == "__main__":
    main(sys.argv)
