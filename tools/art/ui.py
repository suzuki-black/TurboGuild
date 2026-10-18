"""画面部品の絵（アイコン・数字キー・アイテム・敵・ガチャの小道具・演出）を描く。

どれも透明色 0 の上に描き、gen_art.py が 256x256 のアトラス（VRAM ページ 1）に詰める。
"""
from PIL import Image, ImageDraw

import chibi
from palette import IDX

C = IDX


def canvas(w, h):
    img = Image.new("P", (w, h), C["CLEAR"])
    return img, ImageDraw.Draw(img)


def outline(img):
    """chibi.outline の任意サイズ版"""
    w, h = img.size
    px = img.load()
    add = []
    for y in range(h):
        for x in range(w):
            if px[x, y] != C["CLEAR"]:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and px[nx, ny] not in (C["CLEAR"], C["BLACK"]):
                    add.append((x, y))
                    break
    for x, y in add:
        px[x, y] = C["BLACK"]
    return img


# 3x5 の数字（数字キーの印と、小さな数に使う）
DIG3 = {
    "0": ["###", "#.#", "#.#", "#.#", "###"], "1": [".#.", "##.", ".#.", ".#.", "###"],
    "2": ["###", "..#", "###", "#..", "###"], "3": ["###", "..#", "###", "..#", "###"],
    "4": ["#.#", "#.#", "###", "..#", "..#"], "5": ["###", "#..", "###", "..#", "###"],
    "6": ["###", "#..", "###", "#.#", "###"], "7": ["###", "..#", ".#.", ".#.", ".#."],
    "8": ["###", "#.#", "###", "#.#", "###"], "9": ["###", "#.#", "###", "..#", "###"],
    "#": ["#.#", "###", "#.#", "###", "#.#"], "*": ["#.#", ".#.", "###", ".#.", "#.#"],
}

# 5x7 の太い数字（ダメージ・ゲージの数値）
DIG5 = {
    "0": ["#####", "##..#", "##..#", "##..#", "##..#", "##..#", "#####"],
    "1": ["..##.", ".###.", "..##.", "..##.", "..##.", "..##.", ".####"],
    "2": ["#####", "....#", "....#", "#####", "##...", "##...", "#####"],
    "3": ["#####", "....#", "....#", ".####", "....#", "....#", "#####"],
    "4": ["##..#", "##..#", "##..#", "#####", "....#", "....#", "....#"],
    "5": ["#####", "##...", "##...", "#####", "....#", "....#", "#####"],
    "6": ["#####", "##...", "##...", "#####", "##..#", "##..#", "#####"],
    "7": ["#####", "....#", "...##", "..##.", "..##.", "..##.", "..##."],
    "8": ["#####", "##..#", "##..#", "#####", "##..#", "##..#", "#####"],
    "9": ["#####", "##..#", "##..#", "#####", "....#", "....#", "#####"],
}


def glyph(d, x, y, rows, col, scale=1):
    for j, r in enumerate(rows):
        for i, ch in enumerate(r):
            if ch == "#":
                d.rectangle([x + i * scale, y + j * scale, x + i * scale + scale - 1, y + j * scale + scale - 1], fill=col)


def key_badge(ch):
    """携帯の数字キーの形（角丸の灰色キーに黒い数字）"""
    img, d = canvas(11, 11)
    d.rounded_rectangle([0, 0, 10, 10], radius=3, fill=C["LGRAY"], outline=C["DGRAY"])
    d.line([2, 9, 8, 9], fill=C["WHITE"])
    glyph(d, 4, 3, DIG3[ch], C["BLACK"])
    return img


def big_digit(ch, col="YELLOW"):
    img, d = canvas(8, 11)
    glyph(d, 1, 2, DIG5[ch], C[col])
    return outline(img)


# ---------------------------------------------------------------- アイコン 24x24

def icon(kind):
    img, d = canvas(24, 24)
    if kind == "quest":         # 交差した剣
        d.line([3, 20, 18, 5], fill=C["WHITE"], width=3)
        d.line([20, 20, 5, 5], fill=C["WHITE"], width=3)
        d.line([2, 15, 8, 21], fill=C["YELLOW"], width=2)
        d.line([21, 15, 15, 21], fill=C["YELLOW"], width=2)
    elif kind == "gacha":       # カプセルトイの機械
        d.ellipse([3, 1, 20, 15], fill=C["CYAN"])
        d.ellipse([6, 5, 10, 9], fill=C["RED"]); d.ellipse([12, 8, 16, 12], fill=C["YELLOW"])
        d.rectangle([4, 13, 19, 22], fill=C["RED"])
        d.ellipse([9, 15, 14, 20], fill=C["WHITE"])
    elif kind == "cards":
        d.rectangle([9, 2, 21, 18], fill=C["BLUE"])
        d.rectangle([5, 5, 17, 21], fill=C["YELLOW"])
        d.rectangle([8, 9, 14, 17], fill=C["ORANGE"])
    elif kind == "deck":
        for i, col in enumerate(("RED", "BLUE", "GREEN", "YELLOW", "PURPLE")):
            d.rectangle([1 + i * 4, 4 + (2 - abs(2 - i)) * -1 + 2, 8 + i * 4, 18], fill=C[col])
    elif kind == "shop":        # 回復薬の瓶
        d.rectangle([9, 2, 14, 7], fill=C["DRED"])
        d.ellipse([4, 6, 19, 22], fill=C["GREEN"])
        d.ellipse([7, 9, 10, 13], fill=C["WHITE"])
    elif kind == "save":        # フロッピー
        d.rectangle([3, 2, 20, 21], fill=C["DBLUE"])
        d.rectangle([7, 2, 16, 8], fill=C["LGRAY"])
        d.rectangle([6, 12, 17, 21], fill=C["WHITE"])
    elif kind == "login":       # カレンダー
        d.rectangle([3, 4, 20, 21], fill=C["WHITE"])
        d.rectangle([3, 4, 20, 8], fill=C["RED"])
        glyph(d, 10, 11, DIG3["7"], C["BLACK"])
    elif kind == "title":       # 家
        d.polygon([(2, 11), (12, 2), (22, 11)], fill=C["RED"])
        d.rectangle([5, 11, 19, 21], fill=C["ORANGE"])
        d.rectangle([10, 14, 14, 21], fill=C["DRED"])
    elif kind == "news":        # ベル
        d.pieslice([4, 3, 20, 25], 180, 360, fill=C["YELLOW"])
        d.rectangle([4, 14, 20, 17], fill=C["YELLOW"])
        d.ellipse([10, 17, 14, 21], fill=C["ORANGE"])
    return outline(img)


def item(kind):
    img, d = canvas(14, 14)
    if kind == "gold":
        d.ellipse([1, 1, 12, 12], fill=C["YELLOW"]); d.ellipse([4, 4, 9, 9], outline=C["ORANGE"])
    elif kind == "gems":
        d.polygon([(6, 1), (12, 6), (6, 12), (0, 6)], fill=C["CYAN"])
        d.polygon([(6, 1), (9, 6), (6, 12)], fill=C["BLUE"]); d.point((4, 4), fill=C["WHITE"])
    elif kind == "potion":
        d.rectangle([5, 0, 8, 3], fill=C["DRED"]); d.ellipse([1, 3, 12, 13], fill=C["GREEN"])
        d.point((4, 6), fill=C["WHITE"])
    elif kind == "ticket":
        d.rectangle([0, 3, 13, 10], fill=C["ORANGE"]); d.line([4, 3, 4, 10], fill=C["DRED"])
    elif kind == "srticket":
        d.rectangle([0, 3, 13, 10], fill=C["YELLOW"]); d.line([4, 3, 4, 10], fill=C["ORANGE"])
        d.point((8, 6), fill=C["RED"]); d.point((9, 6), fill=C["RED"])
    elif kind == "ap":
        d.polygon([(8, 0), (2, 7), (6, 7), (4, 13), (11, 5), (7, 5)], fill=C["YELLOW"])
    elif kind == "exp":
        d.polygon([(6, 0), (8, 5), (13, 5), (9, 8), (11, 13), (6, 10), (1, 13), (3, 8), (0, 5), (5, 5)], fill=C["CYAN"])
    return outline(img)


# ---------------------------------------------------------------- カード裏・ガチャの小道具

def card_back(w=56, h=72):
    img, d = canvas(w, h)
    d.rectangle([0, 0, w - 1, h - 1], fill=C["YELLOW"])
    d.rectangle([2, 2, w - 3, h - 3], fill=C["DBLUE"])
    for y in range(4, h - 4, 4):
        for x in range(4 + (y // 4 % 2) * 2, w - 4, 4):
            d.point((x, y), fill=C["BLUE"])
    cx, cy = w // 2, h // 2
    s = min(w, h) // 4
    d.polygon([(cx, cy - s), (cx + s, cy), (cx, cy + s), (cx - s, cy)], fill=C["YELLOW"])
    d.polygon([(cx, cy - s // 2), (cx + s // 2, cy), (cx, cy + s // 2), (cx - s // 2, cy)], fill=C["RED"])
    return img


def capsule_machine():
    img, d = canvas(48, 64)
    d.ellipse([4, 0, 43, 36], fill=C["CYAN"])
    for i, (x, y, col) in enumerate(((10, 14, "RED"), (22, 8, "YELLOW"), (30, 18, "BLUE"),
                                     (16, 22, "GREEN"), (26, 25, "PURPLE"))):
        d.ellipse([x, y, x + 8, y + 8], fill=C[col])
    d.point((12, 6), fill=C["WHITE"]); d.point((13, 5), fill=C["WHITE"])
    d.rectangle([6, 32, 41, 63], fill=C["RED"])
    d.rectangle([6, 32, 41, 35], fill=C["DRED"])
    d.ellipse([16, 38, 31, 53], fill=C["LGRAY"])
    d.rectangle([22, 40, 25, 51], fill=C["DGRAY"])
    d.rectangle([28, 56, 39, 62], fill=C["BLACK"])
    return outline(img)


def capsule(rarity):
    img, d = canvas(16, 16)
    top = {0: "LGRAY", 1: "BLUE", 2: "YELLOW", 3: "PURPLE"}[rarity]
    d.pieslice([1, 1, 14, 14], 180, 360, fill=C[top])
    d.pieslice([1, 1, 14, 14], 0, 180, fill=C["WHITE"])
    d.line([1, 7, 14, 7], fill=C["DGRAY"])
    if rarity == 3:
        for x, col in ((4, "RED"), (7, "YELLOW"), (10, "CYAN")):
            d.point((x, 4), fill=C[col])
    return outline(img)


def chest(opened):
    img, d = canvas(40, 36)
    d.rectangle([2, 16, 37, 34], fill=C["DRED"])
    d.rectangle([2, 22, 37, 24], fill=C["YELLOW"])
    if opened:
        d.polygon([(2, 16), (37, 16), (33, 2), (6, 2)], fill=C["DRED"])
        d.rectangle([5, 14, 34, 17], fill=C["YELLOW"])
        d.rectangle([8, 17, 31, 21], fill=C["BLACK"])
    else:
        d.pieslice([2, 6, 37, 26], 180, 360, fill=C["DRED"])
        d.rectangle([17, 18, 22, 26], fill=C["YELLOW"])
    d.rectangle([17, 20, 22, 26] if not opened else [17, 22, 22, 28], fill=C["YELLOW"])
    return outline(img)


def magic_circle():
    img, d = canvas(64, 32)
    d.ellipse([0, 0, 63, 31], outline=C["CYAN"], width=2)
    d.ellipse([8, 4, 55, 27], outline=C["BLUE"])
    d.polygon([(32, 2), (52, 26), (12, 10), (52, 10), (12, 26)], outline=C["CYAN"])
    return img


def door(opened):
    img, d = canvas(40, 56)
    d.rectangle([0, 0, 39, 55], fill=C["YELLOW"])
    d.pieslice([0, 0, 39, 30], 180, 360, fill=C["YELLOW"])
    if opened:
        d.rectangle([4, 10, 35, 55], fill=C["WHITE"])
    else:
        d.rectangle([4, 10, 19, 55], fill=C["ORANGE"]); d.rectangle([20, 10, 35, 55], fill=C["ORANGE"])
        d.point((17, 32), fill=C["DRED"]); d.point((22, 32), fill=C["DRED"])
    return outline(img)


def scroll():
    img, d = canvas(40, 24)
    d.rectangle([4, 4, 35, 19], fill=C["SKIN"])
    d.ellipse([0, 2, 7, 21], fill=C["ORANGE"]); d.ellipse([32, 2, 39, 21], fill=C["ORANGE"])
    for y in (8, 12, 16):
        d.line([10, y, 29, y], fill=C["DRED"])
    return outline(img)


# ---------------------------------------------------------------- クエストの風景・敵・演出

def walker(frame):
    img, d = canvas(24, 32)
    d.ellipse([5, 2, 18, 15], fill=C["SKIN"])
    d.pieslice([4, 0, 19, 12], 180, 360, fill=C["ORANGE"])
    d.rectangle([7, 15, 16, 25], fill=C["BLUE"])
    if frame == 0:
        d.rectangle([7, 25, 10, 30], fill=C["DGRAY"]); d.rectangle([13, 25, 16, 30], fill=C["DGRAY"])
    else:
        d.rectangle([5, 25, 8, 30], fill=C["DGRAY"]); d.rectangle([15, 25, 18, 30], fill=C["DGRAY"])
    d.point((13, 8), fill=C["BLACK"]); d.point((13, 9), fill=C["BLACK"])
    d.line([17, 22, 22, 10], fill=C["WHITE"], width=2)
    return outline(img)


def decor(kind):
    if kind == "tree":
        img, d = canvas(24, 32)
        d.rectangle([10, 20, 13, 31], fill=C["DRED"])
        d.ellipse([1, 0, 22, 24], fill=C["GREEN"]); d.ellipse([5, 4, 11, 10], fill=C["WHITE"])
    elif kind == "rock":
        img, d = canvas(16, 12)
        d.ellipse([0, 2, 15, 11], fill=C["DGRAY"]); d.point((4, 5), fill=C["LGRAY"])
    elif kind == "pillar":
        img, d = canvas(16, 32)
        d.rectangle([3, 3, 12, 31], fill=C["LGRAY"]); d.rectangle([0, 0, 15, 4], fill=C["WHITE"])
        d.line([7, 6, 7, 30], fill=C["DGRAY"])
    elif kind == "crystal":
        img, d = canvas(16, 24)
        d.polygon([(8, 0), (14, 10), (11, 23), (5, 23), (2, 10)], fill=C["CYAN"])
        d.line([8, 2, 8, 22], fill=C["WHITE"])
    elif kind == "cloud":
        img, d = canvas(32, 14)
        d.ellipse([0, 4, 16, 13], fill=C["WHITE"]); d.ellipse([10, 0, 26, 13], fill=C["WHITE"])
        d.ellipse([18, 5, 31, 13], fill=C["WHITE"])
        return img
    elif kind == "flame":
        img, d = canvas(16, 20)
        d.ellipse([1, 4, 14, 19], fill=C["RED"]); d.polygon([(3, 8), (8, 0), (13, 8)], fill=C["RED"])
        d.ellipse([4, 9, 11, 18], fill=C["ORANGE"]); d.ellipse([6, 12, 9, 17], fill=C["YELLOW"])
    return outline(img)


# 通常の敵（属性ごと）: chibi の生き物を流用し、透明背景のまま使う
ENEMY = {
    0: {"kind": "lizard", "outfit_col": "ORANGE", "hair_col": "YELLOW", "extra": "flame"},
    1: {"kind": "slime", "outfit_col": "BLUE"},
    2: {"kind": "bee", "outfit_col": "YELLOW"},
    3: {"kind": "golem", "outfit_col": "LGRAY", "hair_col": "CYAN"},
    4: {"kind": "beast", "outfit_col": "PURPLE", "hair_col": "DGRAY"},
}


def enemy(element):
    return chibi.creature(ENEMY[element])


def slash():
    img, d = canvas(32, 32)
    d.line([2, 29, 29, 2], fill=C["WHITE"], width=3)
    d.line([6, 29, 29, 6], fill=C["YELLOW"])
    d.line([2, 25, 25, 2], fill=C["YELLOW"])
    return img


def hit():
    img, d = canvas(16, 16)
    d.polygon([(8, 0), (10, 6), (16, 8), (10, 10), (8, 16), (6, 10), (0, 8), (6, 6)], fill=C["YELLOW"])
    d.ellipse([6, 6, 10, 10], fill=C["WHITE"])
    return img


def sparkle(big):
    n = 9 if big else 5
    img, d = canvas(n, n)
    m = n // 2
    d.line([m, 0, m, n - 1], fill=C["WHITE"]); d.line([0, m, n - 1, m], fill=C["WHITE"])
    if big:
        d.point((m, m), fill=C["YELLOW"])
    return img


def stamp():
    img, d = canvas(24, 24)
    d.ellipse([0, 0, 23, 23], outline=C["RED"], width=3)
    d.line([6, 12, 10, 17, 18, 6], fill=C["RED"], width=3)
    return img


def all_parts():
    """(名前, 画像) の列。名前は C の ATL_<名前> になる"""
    parts = [("CARDBACK", card_back()), ("CARDBACK_S", card_back(28, 36)),
             ("MACHINE", capsule_machine()), ("CHEST", chest(False)), ("CHEST_OPEN", chest(True)),
             ("DOOR", door(False)), ("DOOR_OPEN", door(True)), ("CIRCLE", magic_circle()),
             ("SCROLL", scroll()), ("SLASH", slash())]
    parts += [(f"ENEMY{e}", enemy(e)) for e in range(5)]
    parts += [(f"ICON_{k.upper()}", icon(k)) for k in
              ("quest", "gacha", "cards", "deck", "shop", "save", "login", "title", "news")]
    parts += [(f"CAPSULE{r}", capsule(r)) for r in range(4)]
    parts += [("WALK0", walker(0)), ("WALK1", walker(1))]
    parts += [(f"DECOR_{k.upper()}", decor(k)) for k in ("tree", "rock", "pillar", "crystal", "cloud", "flame")]
    parts += [("HIT", hit()), ("STAMP", stamp()), ("SPARK", sparkle(True)), ("SPARK_S", sparkle(False))]
    parts += [(f"ITEM_{k.upper()}", item(k)) for k in ("gold", "gems", "potion", "ticket", "srticket", "ap", "exp")]
    # 数字は番号で横に並べた 1 本の帯にする（C 側は 先頭 + 番号 × 幅 で引く）
    parts += [("KEYS", strip([key_badge(k) for k in "0123456789#*"], 12))]
    parts += [("DIGITS", strip([big_digit(k) for k in "0123456789"], 8))]
    return parts


def strip(imgs, cell):
    img, _d = canvas(cell * len(imgs), max(i.size[1] for i in imgs))
    for k, im in enumerate(imgs):
        img.paste(im, (k * cell, 0))
    return img
