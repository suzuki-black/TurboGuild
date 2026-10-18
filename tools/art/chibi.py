"""カードの絵を手続きで描く（2 頭身のキャラクター・動物・魔物）。

1 枚 = 48x64 の絵（背景つき）。輪郭は黒で自動的に付ける。色はすべて palette.py の番号。
どのカードがどんな絵かは data/card_art.csv に書く（データ駆動。カードごとのコードは書かない）。
"""
from PIL import Image, ImageDraw

from palette import IDX

W, H = 48, 64
C = IDX            # 色名 → 番号


def new():
    return Image.new("P", (W, H), C["CLEAR"])


def outline(img):
    """塗った部分の外周に 1px の黒い輪郭を付ける"""
    px = img.load()
    add = []
    for y in range(H):
        for x in range(W):
            if px[x, y] != C["CLEAR"]:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < W and 0 <= ny < H and px[nx, ny] not in (C["CLEAR"], C["BLACK"]):
                    add.append((x, y))
                    break
    for x, y in add:
        px[x, y] = C["BLACK"]


# ---------------------------------------------------------------- 人型

def face(d, cx, cy, eye=C["BLACK"], mood="smile"):
    for ex in (cx - 5, cx + 4):
        d.rectangle([ex, cy, ex + 1, cy + 2], fill=eye)
        d.point((ex, cy), fill=C["WHITE"])
    if mood == "smile":
        d.line([cx - 1, cy + 5, cx + 1, cy + 5], fill=C["DRED"])
    elif mood == "fierce":
        d.line([cx - 6, cy - 2, cx - 3, cy - 1], fill=C["BLACK"])
        d.line([cx + 3, cy - 1, cx + 6, cy - 2], fill=C["BLACK"])
        d.line([cx - 1, cy + 5, cx + 1, cy + 5], fill=C["BLACK"])
    d.point((cx - 7, cy + 3), fill=C["ORANGE"])      # ほっぺ
    d.point((cx + 7, cy + 3), fill=C["ORANGE"])


def hair(d, style, col, cx=24, cy=20):
    if style == "none":
        return
    if style in ("short", "long", "twin", "cat", "spiky"):
        d.pieslice([cx - 12, cy - 12, cx + 12, cy + 8], 180, 360, fill=col)
        d.polygon([(cx - 12, cy - 2), (cx - 8, cy - 4), (cx - 5, cy + 1), (cx - 2, cy - 4),
                   (cx + 2, cy - 4), (cx + 5, cy + 1), (cx + 8, cy - 4), (cx + 12, cy - 2)], fill=col)
    if style == "long":
        d.rectangle([cx - 13, cy - 4, cx - 10, cy + 14], fill=col)
        d.rectangle([cx + 10, cy - 4, cx + 13, cy + 14], fill=col)
    if style == "twin":
        d.ellipse([cx - 19, cy - 6, cx - 11, cy + 10], fill=col)
        d.ellipse([cx + 11, cy - 6, cx + 19, cy + 10], fill=col)
    if style == "cat":
        d.polygon([(cx - 11, cy - 6), (cx - 9, cy - 16), (cx - 4, cy - 10)], fill=col)
        d.polygon([(cx + 11, cy - 6), (cx + 9, cy - 16), (cx + 4, cy - 10)], fill=col)
    if style == "spiky":
        for x in (-10, -4, 2, 8):
            d.polygon([(cx + x, cy - 9), (cx + x + 3, cy - 17), (cx + x + 6, cy - 9)], fill=col)


def headgear(d, kind, col, cx=24, cy=20):
    if kind == "none":
        return
    if kind == "helmet":
        d.pieslice([cx - 13, cy - 14, cx + 13, cy + 10], 180, 360, fill=col)
        d.rectangle([cx - 13, cy - 3, cx + 13, cy - 1], fill=C["DGRAY"])
        d.polygon([(cx - 2, cy - 14), (cx, cy - 20), (cx + 2, cy - 14)], fill=C["RED"])
    elif kind == "wizard":
        d.polygon([(cx - 15, cy - 6), (cx + 15, cy - 6), (cx + 4, cy - 26), (cx - 2, cy - 20)], fill=col)
        d.rectangle([cx - 15, cy - 7, cx + 15, cy - 5], fill=C["YELLOW"])
    elif kind == "hood":
        d.pieslice([cx - 14, cy - 14, cx + 14, cy + 16], 180, 360, fill=col)
        d.rectangle([cx - 14, cy - 1, cx - 11, cy + 12], fill=col)
        d.rectangle([cx + 11, cy - 1, cx + 14, cy + 12], fill=col)
    elif kind == "veil":
        d.pieslice([cx - 14, cy - 14, cx + 14, cy + 14], 180, 360, fill=col)
        d.rectangle([cx - 14, cy - 1, cx - 10, cy + 18], fill=col)
        d.rectangle([cx + 10, cy - 1, cx + 14, cy + 18], fill=col)
        d.rectangle([cx - 5, cy - 12, cx + 4, cy - 10], fill=C["YELLOW"])
    elif kind == "cap":
        d.pieslice([cx - 12, cy - 13, cx + 12, cy + 6], 180, 360, fill=col)
        d.rectangle([cx - 2, cy - 4, cx + 15, cy - 2], fill=col)
    elif kind == "crown":
        d.polygon([(cx - 9, cy - 9), (cx - 9, cy - 16), (cx - 5, cy - 12), (cx, cy - 18),
                   (cx + 5, cy - 12), (cx + 9, cy - 16), (cx + 9, cy - 9)], fill=C["YELLOW"])
        d.point((cx, cy - 13), fill=C["RED"])
    elif kind == "horns":
        d.polygon([(cx - 10, cy - 8), (cx - 16, cy - 20), (cx - 6, cy - 11)], fill=C["LGRAY"])
        d.polygon([(cx + 10, cy - 8), (cx + 16, cy - 20), (cx + 6, cy - 11)], fill=C["LGRAY"])
    elif kind == "band":
        d.rectangle([cx - 12, cy - 6, cx + 12, cy - 4], fill=col)


def body(d, outfit, col, cx=24):
    top, bot = 31, 50
    if outfit == "robe":
        d.polygon([(cx - 8, top), (cx + 8, top), (cx + 13, bot + 6), (cx - 13, bot + 6)], fill=col)
        d.line([cx, top + 2, cx, bot + 5], fill=C["YELLOW"])
    elif outfit == "dress":
        d.polygon([(cx - 7, top), (cx + 7, top), (cx + 15, bot + 4), (cx - 15, bot + 4)], fill=col)
        d.rectangle([cx - 7, top + 6, cx + 7, top + 7], fill=C["WHITE"])
    elif outfit == "armor":
        d.rectangle([cx - 10, top, cx + 10, bot], fill=C["LGRAY"])
        d.rectangle([cx - 10, top + 8, cx + 10, top + 10], fill=col)
        d.ellipse([cx - 14, top - 1, cx - 7, top + 6], fill=C["LGRAY"])
        d.ellipse([cx + 7, top - 1, cx + 14, top + 6], fill=C["LGRAY"])
    else:  # tunic
        d.rectangle([cx - 9, top, cx + 9, bot - 2], fill=col)
        d.rectangle([cx - 9, bot - 6, cx + 9, bot - 5], fill=C["DRED"])
    if outfit not in ("robe", "dress"):
        d.rectangle([cx - 7, bot - 1, cx - 2, bot + 7], fill=C["DGRAY"])
        d.rectangle([cx + 2, bot - 1, cx + 7, bot + 7], fill=C["DGRAY"])
    # 腕と手
    d.rectangle([cx - 13, top + 2, cx - 10, top + 12], fill=col if outfit != "armor" else C["LGRAY"])
    d.rectangle([cx + 10, top + 2, cx + 13, top + 12], fill=col if outfit != "armor" else C["LGRAY"])
    d.ellipse([cx - 14, top + 11, cx - 9, top + 15], fill=C["SKIN"])
    d.ellipse([cx + 9, top + 11, cx + 14, top + 15], fill=C["SKIN"])


def cape(d, col, cx=24):
    d.polygon([(cx - 10, 30), (cx + 10, 30), (cx + 17, 58), (cx - 17, 58)], fill=col)


def weapon(d, kind, col, cx=24):
    hx, hy = cx + 12, 44      # 右手
    lx, ly = cx - 12, 44      # 左手
    if kind == "sword":
        d.line([hx, hy, hx + 8, hy - 22], fill=C["WHITE"], width=2)
        d.line([hx - 3, hy - 2, hx + 3, hy + 1], fill=C["YELLOW"], width=2)
    elif kind == "greatsword":
        d.polygon([(hx - 1, hy), (hx + 3, hy), (hx + 10, hy - 30), (hx + 6, hy - 32)], fill=C["LGRAY"])
        d.line([hx - 4, hy - 1, hx + 5, hy + 2], fill=C["YELLOW"], width=2)
    elif kind == "twin":
        d.line([hx, hy, hx + 7, hy - 18], fill=C["WHITE"], width=2)
        d.line([lx, ly, lx - 7, ly - 18], fill=C["WHITE"], width=2)
    elif kind == "staff":
        d.line([hx + 2, hy + 12, hx + 2, hy - 26], fill=C["DRED"], width=2)
        d.ellipse([hx - 2, hy - 32, hx + 6, hy - 24], fill=col)
        d.point((hx + 1, hy - 30), fill=C["WHITE"])
    elif kind == "spear":
        d.line([hx + 2, hy + 14, hx + 2, hy - 30], fill=C["DRED"], width=2)
        d.polygon([(hx - 1, hy - 30), (hx + 2, hy - 38), (hx + 5, hy - 30)], fill=C["WHITE"])
    elif kind == "bow":
        d.arc([lx - 10, ly - 22, lx + 2, ly + 6], 90, 270, fill=C["ORANGE"], width=2)
        d.line([lx - 4, ly - 21, lx - 4, ly + 5], fill=C["WHITE"])
    elif kind == "shield":
        d.ellipse([lx - 9, ly - 14, lx + 5, ly + 4], fill=col)
        d.ellipse([lx - 5, ly - 10, lx + 1, ly], fill=C["YELLOW"])
        d.line([hx, hy, hx + 6, hy - 16], fill=C["WHITE"], width=2)
    elif kind == "lantern":
        d.line([hx + 2, hy - 2, hx + 2, hy + 2], fill=C["DGRAY"])
        d.rectangle([hx - 1, hy + 2, hx + 5, hy + 9], fill=C["YELLOW"])
        d.point((hx + 2, hy + 5), fill=C["WHITE"])
    elif kind == "bell":
        d.polygon([(hx - 2, hy + 6), (hx + 6, hy + 6), (hx + 4, hy - 2), (hx, hy - 2)], fill=C["YELLOW"])
    elif kind == "book":
        d.rectangle([lx - 6, ly - 6, lx + 4, ly + 2], fill=col)
        d.line([lx - 1, ly - 6, lx - 1, ly + 2], fill=C["WHITE"])
    elif kind == "rod":
        d.line([hx, hy + 4, hx + 12, hy - 28], fill=C["DRED"])
        d.line([hx + 12, hy - 28, hx + 14, hy - 4], fill=C["LGRAY"])
    elif kind == "bag":
        d.ellipse([lx - 8, ly - 4, lx + 2, ly + 8], fill=col)
    elif kind == "cards":
        for i in range(3):
            d.rectangle([hx - 2 + i * 3, hy - 6 - i, hx + 2 + i * 3, hy + 1 - i], fill=C["WHITE"])
    elif kind == "dagger":
        d.line([hx, hy, hx + 5, hy - 10], fill=C["WHITE"], width=2)
    elif kind == "puppet":
        d.line([hx + 2, hy - 20, hx + 2, hy - 6], fill=C["LGRAY"])
        d.ellipse([hx - 2, hy - 6, hx + 6, hy + 4], fill=col)
    elif kind == "flame":
        d.ellipse([hx - 3, hy - 14, hx + 7, hy - 2], fill=C["ORANGE"])
        d.ellipse([hx, hy - 10, hx + 4, hy - 4], fill=C["YELLOW"])
    elif kind == "letter":
        d.rectangle([hx - 3, hy - 4, hx + 6, hy + 2], fill=C["WHITE"])
        d.line([hx - 3, hy - 4, hx + 1, hy - 1, hx + 6, hy - 4], fill=C["DGRAY"])


def human(spec):
    img = new()
    d = ImageDraw.Draw(img)
    col = C[spec["outfit_col"]]
    if spec.get("extra") == "cape":
        cape(d, C[spec.get("extra_col", "RED")])
    if spec.get("extra") == "wings":
        wc = C[spec.get("extra_col", "WHITE")]
        d.ellipse([2, 22, 20, 46], fill=wc)
        d.ellipse([28, 22, 46, 46], fill=wc)
    if spec.get("extra") == "aura":
        ac = C[spec.get("extra_col", "YELLOW")]
        d.ellipse([3, 2, 45, 44], outline=ac, width=2)
    if spec["hair"] in ("long", "twin"):
        hair(d, spec["hair"], C[spec["hair_col"]])
    body(d, spec["outfit"], col)
    d.ellipse([13, 9, 35, 31], fill=C["SKIN"])
    hair(d, spec["hair"], C[spec["hair_col"]])
    headgear(d, spec.get("head", "none"), C[spec.get("head_col", spec["outfit_col"])])
    if spec.get("beard"):
        d.pieslice([16, 20, 32, 38], 0, 180, fill=C["WHITE"])
    face(d, 24, 20, mood=spec.get("mood", "smile"))
    if spec.get("beard"):
        d.ellipse([16, 25, 32, 37], fill=C["WHITE"])
    weapon(d, spec.get("weapon", "none"), C[spec.get("weapon_col", "CYAN")])
    outline(img)
    return img


# ---------------------------------------------------------------- 動物・魔物

def eyes(d, x1, x2, y, big=False):
    for ex in (x1, x2):
        if big:
            d.ellipse([ex - 2, y - 2, ex + 2, y + 2], fill=C["WHITE"])
            d.rectangle([ex, y - 1, ex + 1, y + 1], fill=C["BLACK"])
        else:
            d.rectangle([ex, y, ex + 1, y + 2], fill=C["BLACK"])
            d.point((ex, y), fill=C["WHITE"])


def creature(spec):
    img = new()
    d = ImageDraw.Draw(img)
    k = spec["kind"]
    col = C[spec["outfit_col"]]
    sub = C[spec.get("hair_col", "WHITE")]
    if k == "rabbit":
        d.ellipse([13, 4, 20, 26], fill=col); d.ellipse([28, 4, 35, 26], fill=col)
        d.ellipse([15, 7, 18, 22], fill=C["SKIN"]); d.ellipse([30, 7, 33, 22], fill=C["SKIN"])
        d.ellipse([9, 22, 39, 54], fill=col)
        d.ellipse([16, 40, 32, 56], fill=sub)
        eyes(d, 17, 30, 32); d.point((24, 37), fill=C["RED"])
    elif k == "lizard":
        d.polygon([(30, 44), (46, 40), (44, 52)], fill=col)
        d.ellipse([10, 28, 36, 56], fill=col)
        d.ellipse([8, 12, 34, 34], fill=col)
        d.ellipse([12, 38, 30, 54], fill=sub)
        eyes(d, 14, 26, 20, big=True)
        if spec.get("extra") == "flame":
            d.ellipse([30, 14, 44, 30], fill=C["ORANGE"]); d.ellipse([34, 18, 40, 26], fill=C["YELLOW"])
    elif k == "frog":
        d.ellipse([6, 26, 42, 56], fill=col)
        d.ellipse([8, 16, 20, 30], fill=col); d.ellipse([28, 16, 40, 30], fill=col)
        eyes(d, 13, 33, 21, big=True)
        d.arc([14, 32, 34, 44], 20, 160, fill=C["DRED"])
        d.ellipse([14, 40, 34, 54], fill=sub)
    elif k == "rat":
        d.ellipse([8, 12, 20, 24], fill=col); d.ellipse([28, 12, 40, 24], fill=col)
        d.ellipse([11, 15, 17, 21], fill=C["SKIN"]); d.ellipse([31, 15, 37, 21], fill=C["SKIN"])
        d.ellipse([10, 18, 38, 42], fill=col)
        d.ellipse([12, 34, 36, 58], fill=col)
        d.line([36, 52, 46, 44], fill=C["SKIN"], width=2)
        eyes(d, 17, 29, 27); d.point((24, 33), fill=C["RED"])
    elif k == "owl":
        d.ellipse([8, 10, 40, 58], fill=col)
        d.ellipse([12, 16, 24, 28], fill=sub); d.ellipse([24, 16, 36, 28], fill=sub)
        eyes(d, 17, 30, 21, big=True)
        d.polygon([(22, 27), (26, 27), (24, 31)], fill=C["ORANGE"])
        d.polygon([(10, 12), (14, 4), (18, 12)], fill=col); d.polygon([(30, 12), (34, 4), (38, 12)], fill=col)
        d.ellipse([16, 36, 32, 54], fill=C["SKIN"])
    elif k == "bee":
        d.ellipse([4, 12, 20, 28], fill=C["CYAN"]); d.ellipse([28, 12, 44, 28], fill=C["CYAN"])
        d.ellipse([10, 22, 38, 54], fill=C["YELLOW"])
        for y in (32, 40, 48):
            d.rectangle([11, y, 37, y + 2], fill=C["BLACK"])
        d.ellipse([14, 10, 34, 28], fill=C["YELLOW"])
        eyes(d, 19, 28, 17)
        d.line([18, 10, 14, 3], fill=C["BLACK"]); d.line([30, 10, 34, 3], fill=C["BLACK"])
    elif k == "golem":
        d.rectangle([8, 22, 40, 50], fill=col)
        d.rectangle([14, 8, 34, 24], fill=col)
        d.rectangle([2, 24, 9, 46], fill=col); d.rectangle([39, 24, 46, 46], fill=col)
        d.rectangle([12, 50, 20, 60], fill=col); d.rectangle([28, 50, 36, 60], fill=col)
        for x, y in ((14, 28), (30, 36), (20, 42), (34, 26)):
            d.point((x, y), fill=sub)
        d.rectangle([18, 14, 21, 17], fill=sub); d.rectangle([27, 14, 30, 17], fill=sub)
    elif k == "dragon":
        d.polygon([(4, 18), (20, 26), (8, 40)], fill=sub)
        d.polygon([(44, 18), (28, 26), (40, 40)], fill=sub)
        d.ellipse([12, 26, 36, 56], fill=col)
        d.ellipse([10, 6, 38, 30], fill=col)
        d.polygon([(12, 10), (8, 0), (18, 8)], fill=C["LGRAY"]); d.polygon([(36, 10), (40, 0), (30, 8)], fill=C["LGRAY"])
        eyes(d, 16, 30, 16, big=True)
        d.ellipse([18, 22, 30, 28], fill=C["SKIN"])
        d.ellipse([18, 36, 30, 52], fill=C["SKIN"])
        if spec.get("rider"):
            d.ellipse([18, 0, 30, 10], fill=C["SKIN"])
    elif k == "fairy":
        d.ellipse([4, 14, 22, 38], fill=C["CYAN"]); d.ellipse([26, 14, 44, 38], fill=C["CYAN"])
        d.ellipse([16, 30, 32, 50], fill=col)
        d.ellipse([14, 12, 34, 32], fill=C["SKIN"])
        hair(d, "short", C[spec.get("hair_col", "YELLOW")], 24, 22)
        face(d, 24, 22)
        d.ellipse([20, 48, 28, 56], fill=C["YELLOW"])
    elif k == "slime":
        d.ellipse([6, 24, 42, 58], fill=col)
        d.ellipse([12, 28, 20, 36], fill=C["WHITE"])
        eyes(d, 18, 29, 40)
    elif k == "beast":
        d.ellipse([8, 26, 40, 56], fill=col)
        d.ellipse([10, 10, 38, 36], fill=col)
        d.polygon([(10, 16), (12, 2), (20, 12)], fill=col); d.polygon([(38, 16), (36, 2), (28, 12)], fill=col)
        d.ellipse([18, 24, 30, 34], fill=sub)
        eyes(d, 16, 30, 20)
        d.point((24, 27), fill=C["BLACK"])
    outline(img)
    return img


# ---------------------------------------------------------------- 背景と枠

ELEM_BG = {
    0: ("DRED", "RED", "ORANGE"),     # 炎
    1: ("DBLUE", "BLUE", "CYAN"),     # 水
    2: ("DBLUE", "PURPLE", "YELLOW"), # 雷
    3: ("CYAN", "WHITE", "YELLOW"),   # 光
    4: ("BLACK", "DBLUE", "PURPLE"),  # 闇
}


def background(element, seed):
    """上が濃く下が明るい 3 段の帯。境目は市松のディザでぼかす。属性ごとの小物を散らす"""
    a, b, c = (C[n] for n in ELEM_BG[element])
    img = Image.new("P", (W, H), a)
    px = img.load()
    for y in range(H):
        for x in range(W):
            t = y * 3 // H
            band = (a, b, c)[t]
            edge = y % (H // 3)
            if edge < 4 and t > 0 and (x + y) % 2 == 0:
                band = (a, b, c)[t - 1]
            px[x, y] = band
    d = ImageDraw.Draw(img)
    r = seed * 7 + 3
    for i in range(6):
        r = (r * 1103515245 + 12345) & 0x7FFFFFFF
        x, y = r % W, (r >> 8) % H
        if element == 0:
            d.point((x, y), fill=C["YELLOW"])
        elif element == 1:
            d.ellipse([x, y, x + 3, y + 3], outline=C["WHITE"])
        elif element == 2:
            d.line([x, y, x + 3, y + 4, x + 1, y + 5, x + 4, y + 9], fill=C["YELLOW"])
        elif element == 3:
            d.point((x, y), fill=C["WHITE"]); d.point((x + 1, y), fill=C["YELLOW"])
        else:
            d.point((x, y), fill=C["PURPLE"])
    return img


FRAME_COLS = {
    0: ("DGRAY", "LGRAY"),
    1: ("DBLUE", "CYAN"),
    2: ("ORANGE", "YELLOW"),
    3: None,     # SSR は虹
}
RAINBOW = ["RED", "ORANGE", "YELLOW", "GREEN", "CYAN", "BLUE", "PURPLE"]


def framed(art, rarity, size=(56, 72), border=4):
    """絵（48x64）に枠を付けたカード 1 枚"""
    fw, fh = size
    card = Image.new("P", size, C["BLACK"])
    px = card.load()
    for y in range(fh):
        for x in range(fw):
            inner = border <= x < fw - border and border <= y < fh - border
            if inner:
                continue
            if rarity == 3:
                px[x, y] = C[RAINBOW[((x + y) // 3) % len(RAINBOW)]]
            else:
                dark, light = FRAME_COLS[rarity]
                ring = min(x, y, fw - 1 - x, fh - 1 - y)
                px[x, y] = C[light] if ring == 1 else C[dark]
    card.paste(art, (border, border))
    d = ImageDraw.Draw(card)
    if rarity >= 2:      # 四隅の宝石
        gem = C["RED"] if rarity == 2 else C["WHITE"]
        for x, y in ((1, 1), (fw - 4, 1), (1, fh - 4), (fw - 4, fh - 4)):
            d.rectangle([x, y, x + 2, y + 2], fill=gem)
    return card


def card_image(spec, element, rarity, seed):
    bg = background(element, seed)
    fg = human(spec) if spec["kind"] == "human" else creature(spec)
    bg.paste(fg, (0, 0), mask_of(fg))
    return framed(bg, rarity)


def mask_of(img):
    """透明（0）以外を 255 にした貼り付け用マスク"""
    m = Image.new("L", img.size, 0)
    src, dst = img.load(), m.load()
    for y in range(img.size[1]):
        for x in range(img.size[0]):
            if src[x, y] != C["CLEAR"]:
                dst[x, y] = 255
    return m
