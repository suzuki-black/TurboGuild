# SPDX-License-Identifier: MIT
"""カード絵の見本を 1 枚の PNG にまとめる（確認用）"""
import csv
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).parent))
import chibi  # noqa: E402
from palette import rgb8  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
RAR = {"N": 0, "R": 1, "SR": 2, "SSR": 3, "USR": 4, "XUSR": 5}
ELEM = {"炎": 0, "水": 1, "雷": 2, "光": 3, "闇": 4}


def load_specs():
    specs = {}
    with open(ROOT / "data" / "card_art.csv", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            specs[int(r["id"])] = {k: v for k, v in r.items() if v}
    return specs


def cards():
    specs = load_specs()
    with open(ROOT / "data" / "cards.csv", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            i = int(r["id"])
            yield i, chibi.card_image(specs[i], ELEM[r["element"]], RAR[r["rarity"]], i)


def to_rgb(img, scale):
    pal = []
    for i in range(16):
        pal += list(rgb8(i))
    img = img.copy()
    img.putpalette(pal + [0] * (768 - len(pal)))
    return img.convert("RGB").resize((img.width * scale, img.height * scale), Image.NEAREST)


if __name__ == "__main__":
    out = Path(sys.argv[1])
    imgs = list(cards())
    cols, scale = 10, 3
    sheet = Image.new("RGB", (cols * 58 * scale, ((len(imgs) + cols - 1) // cols) * 74 * scale), (40, 40, 40))
    for k, (_i, im) in enumerate(imgs):
        sheet.paste(to_rgb(im, scale), ((k % cols) * 58 * scale, (k // cols) * 74 * scale))
    sheet.save(out)
    print(out)
