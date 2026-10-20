#!/usr/bin/env python3
"""絵の素材を作る（DECISIONS #31）。

    build/atlas0..N.bin     画面部品のアトラス 256x256（起動時に VRAM ページ 1 へ読み込む）
    build/cardart0..N.bin   カード 1 枚 56x72（枠つき）。1 ページに 4 枚
    build/thumb0..N.bin     カードの小さい絵 28x36（枠つき）。1 ページに 16 枚
    src/data/art.h          アトラスの位置（ATL_*）・枚数・ページ割り
    src/data/palette.h      16 色のパレット
    build/art/*.png         確認用の見本（--preview のとき）

絵はすべて手続きで描く（tools/art/）。カードごとの描き分けは data/card_art.csv。
"""
import csv
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "art"))
import chibi  # noqa: E402
import palette  # noqa: E402
import ui  # noqa: E402

PAGE_PAYLOAD = 8192 - 16
CARD_W, CARD_H = 56, 72
THUMB_W, THUMB_H = 28, 36
CARDS_PER_PAGE = PAGE_PAYLOAD // (CARD_W * CARD_H // 2)        # 4
THUMBS_PER_PAGE = PAGE_PAYLOAD // (THUMB_W * THUMB_H // 2)     # 16
# docs/MEMORY_MAP.md に並べたページ数と一致させる
ATLAS_PAGES, CARDART_PAGES, THUMB_PAGES = 5, 30, 8
RAR = {"N": 0, "R": 1, "SR": 2, "SSR": 3}
ELEM = {"炎": 0, "水": 1, "雷": 2, "光": 3, "闇": 4}


def die(msg):
    sys.exit(f"gen_art: {msg}")


def pack4(img):
    """4bpp: 1 バイト = 左の画素が上位 4bit"""
    w, h = img.size
    if w % 2:
        die("幅は偶数")
    px = img.load()
    out = bytearray()
    for y in range(h):
        for x in range(0, w, 2):
            out.append((px[x, y] << 4) | px[x + 1, y])
    return bytes(out)


def shrink(img):
    """2x2 を 1 画素に。いちばん多い色、同数なら暗い輪郭を優先"""
    w, h = img.size
    src = img.load()
    out = Image.new("P", (w // 2, h // 2))
    dst = out.load()
    for y in range(h // 2):
        for x in range(w // 2):
            cs = [src[2 * x, 2 * y], src[2 * x + 1, 2 * y], src[2 * x, 2 * y + 1], src[2 * x + 1, 2 * y + 1]]
            best = max(set(cs), key=lambda c: (cs.count(c), c == palette.IDX["BLACK"]))
            dst[x, y] = best
    return out


def card_specs():
    specs = {}
    with open(ROOT / "data" / "card_art.csv", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            specs[int(r["id"])] = {k: v for k, v in r.items() if v}
    cards = []
    with open(ROOT / "data" / "cards.csv", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            i = int(r["id"])
            if i not in specs:
                die(f"card_art.csv に id {i} の絵の指定が無い")
            cards.append((i, specs[i], ELEM[r["element"]], RAR[r["rarity"]]))
    return cards


def make_cards():
    full, thumbs = [], []
    for i, spec, elem, rar in card_specs():
        bg = chibi.background(elem, i)
        fg = chibi.human(spec) if spec["kind"] == "human" else chibi.creature(spec)
        bg.paste(fg, (0, 0), chibi.mask_of(fg))
        full.append(chibi.framed(bg, rar, (CARD_W, CARD_H), 4))
        thumbs.append(chibi.framed(shrink(bg), rar, (THUMB_W, THUMB_H), 2))
    return full, thumbs


def shelf_pack(parts, width=256, height=256):
    """背の高い順に左から詰める。戻り値 {名前: (x, y, w, h)} とアトラス画像"""
    order = sorted(parts, key=lambda p: (-p[1].size[1], -p[1].size[0]))
    atlas = Image.new("P", (width, height), palette.IDX["CLEAR"])
    rects = {}
    x = y = shelf = 0
    for name, img in order:
        w, h = img.size
        w2 = w + (w & 1)                 # HMMM はバイト単位なので x と幅を偶数に揃える
        if x + w2 > width:
            x, y, shelf = 0, y + shelf, 0
        if y + h > height:
            die(f"アトラスが 256x256 に入らない（{name}）")
        atlas.paste(img, (x, y))
        rects[name] = (x, y, w2, h)
        x += w2
        shelf = max(shelf, h)
    return rects, atlas


def split_pages(blob, n_pages, what):
    pages = [blob[i:i + PAGE_PAYLOAD] for i in range(0, len(blob), PAGE_PAYLOAD)]
    if len(pages) > n_pages:
        die(f"{what} が {len(pages)} ページ要る（MEMORY_MAP の割り当ては {n_pages}）")
    return pages + [b""] * (n_pages - len(pages))


def per_page(images, per, n_pages, what):
    if len(images) > per * n_pages:
        die(f"{what} {len(images)} 枚は {n_pages} ページ × {per} 枚に入らない")
    pages = []
    for p in range(n_pages):
        pages.append(b"".join(pack4(im) for im in images[p * per:(p + 1) * per]))
    return pages


def write_if_changed(path, text):
    if not path.exists() or path.read_text(encoding="utf-8") != text:
        path.write_text(text, encoding="utf-8")


def write_headers(rects):
    gen = ROOT / "src" / "data"
    h = ["/* art.h — tools/gen_art.py の生成物。手で編集しない */", "#ifndef DATA_ART_H", "#define DATA_ART_H",
         f"#define ART_CARD_W {CARD_W}", f"#define ART_CARD_H {CARD_H}",
         f"#define ART_CARD_BYTES {CARD_W * CARD_H // 2}", f"#define ART_CARDS_PER_PAGE {CARDS_PER_PAGE}",
         f"#define ART_THUMB_W {THUMB_W}", f"#define ART_THUMB_H {THUMB_H}",
         f"#define ART_THUMB_BYTES {THUMB_W * THUMB_H // 2}", f"#define ART_THUMBS_PER_PAGE {THUMBS_PER_PAGE}",
         f"#define ATLAS_PAGES {ATLAS_PAGES}", "#define ATLAS_VY 256   /* アトラスは VRAM ページ 1 (y=256..511) */", ""]
    for name, (x, y, w, hh) in sorted(rects.items()):
        h.append(f"#define ATL_{name} {x}, {y}, {w}, {hh}")
        h.append(f"#define ATL_{name}_X {x}")
        h.append(f"#define ATL_{name}_Y {y}")
        h.append(f"#define ATL_{name}_W {w}")
        h.append(f"#define ATL_{name}_H {hh}")
    h += ["#endif", ""]
    write_if_changed(gen / "art.h", "\n".join(h))
    pal = ", ".join(f"0x{b:02X}" for b in palette.msx_bytes())
    p = ["/* palette.h — tools/gen_art.py の生成物（tools/art/palette.py）。手で編集しない */",
         "#ifndef DATA_PALETTE_H", "#define DATA_PALETTE_H",
         f"static const u8 art_palette[32] = {{ {pal} }};", "#endif", ""]
    write_if_changed(gen / "palette.h", "\n".join(p))


def to_rgb(img, scale=1):
    pal = []
    for i in range(16):
        pal += list(palette.rgb8(i))
    im = img.copy()
    im.putpalette(pal + [0] * (768 - len(pal)))
    return im.convert("RGB").resize((im.width * scale, im.height * scale), Image.NEAREST)


def main(preview):
    out = ROOT / "build"
    out.mkdir(exist_ok=True)
    rects, atlas = shelf_pack(ui.all_parts())
    for i, blob in enumerate(split_pages(pack4(atlas), ATLAS_PAGES, "アトラス")):
        (out / f"atlas{i}.bin").write_bytes(blob)
    full, thumbs = make_cards()
    for i, blob in enumerate(per_page(full, CARDS_PER_PAGE, CARDART_PAGES, "カード絵")):
        (out / f"cardart{i}.bin").write_bytes(blob)
    for i, blob in enumerate(per_page(thumbs, THUMBS_PER_PAGE, THUMB_PAGES, "小さい絵")):
        (out / f"thumb{i}.bin").write_bytes(blob)
    write_headers(rects)
    if preview:
        (out / "art").mkdir(exist_ok=True)
        to_rgb(atlas, 3).save(out / "art" / "atlas.png")
    used = max(y + h for _x, y, _w, h in rects.values())
    print(f"gen_art: 部品 {len(rects)} 個（アトラス {used}/256 行）, カード絵 {len(full)} 枚")


if __name__ == "__main__":
    main("--preview" in sys.argv)
