# SPDX-License-Identifier: MIT
"""TurboGuild の 16 色パレット（SCREEN5）。絵の生成と MSX 側（sys/vdp.c）で同じものを使う。

各色は MSX の 3bit RGB (0..7)。番号は C の COL_* と一致させる（src/config.h）。
"""

# (名前, R, G, B)  0..7
PALETTE = [
    ("CLEAR",   0, 0, 0),   # 0 透明（LMMM で抜く色）。表示上は黒
    ("BLACK",   0, 0, 0),   # 1 輪郭・背景
    ("DGRAY",   2, 2, 3),   # 2
    ("LGRAY",   5, 5, 6),   # 3
    ("WHITE",   7, 7, 7),   # 4
    ("SKIN",    7, 5, 4),   # 5
    ("DRED",    4, 0, 1),   # 6 暗い赤・茶の代わり
    ("RED",     7, 1, 1),   # 7
    ("ORANGE",  7, 4, 0),   # 8
    ("YELLOW",  7, 7, 2),   # 9
    ("DGREEN",  0, 3, 1),   # 10
    ("GREEN",   2, 6, 1),   # 11
    ("DBLUE",   0, 1, 4),   # 12
    ("BLUE",    1, 3, 7),   # 13
    ("CYAN",    3, 6, 7),   # 14
    ("PURPLE",  5, 1, 6),   # 15
]

IDX = {name: i for i, (name, *_rgb) in enumerate(PALETTE)}


def rgb8(i):
    """プレビュー用の 8bit RGB"""
    _n, r, g, b = PALETTE[i]
    return (r * 255 // 7, g * 255 // 7, b * 255 // 7)


def msx_bytes():
    """V9938 パレットレジスタ形式: 0RRR0BBB, 00000GGG を 16 色ぶん"""
    out = bytearray()
    for _n, r, g, b in PALETTE:
        out += bytes([(r << 4) | b, g])
    return bytes(out)
