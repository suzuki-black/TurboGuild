#!/usr/bin/env python3
"""data/*.csv を固定長レコードのバイナリと C ヘッダへ変換する（仕様 §4）。

    build/cards.bin   u16 レコード数 + CardRec × n
    build/text.dat    u16 文字列数 + u16 オフセット × n + Shift-JIS(NUL 終端)
    src/data/cards.h  CardRec の定義（生成物。手で編集しない）

CSV と data/text は UTF-8 で書き、ここで Shift-JIS (cp932) に落とす。
文字列は CSV に直接書き、text_id はこのスクリプトが振る。
"""
import csv
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PAGE_PAYLOAD = 8192 - 16

RARITY = {"N": 0, "R": 1, "SR": 2, "SSR": 3}
ELEMENT = {"炎": 0, "水": 1, "雷": 2, "光": 3, "闇": 4}
MAX_LV = {0: 20, 1: 30, 2: 40, 3: 50}

CARD_FMT = "<HHBBHHBBBHHB"
CARD_FIELDS = [
    ("u16", "id"), ("u16", "name_text"), ("u8", "rarity"), ("u8", "element"),
    ("u16", "atk"), ("u16", "def"), ("u8", "cost"), ("u8", "skill"),
    ("u8", "max_lv"), ("u16", "evo_to"), ("u16", "event_id"), ("u8", "img"),
]


class TextTable:
    def __init__(self):
        self.items = []
        self.index = {}

    def add(self, s):
        if s not in self.index:
            self.index[s] = len(self.items)
            self.items.append(s)
        return self.index[s]

    def pack(self):
        n = len(self.items)
        head = 2 + 2 * n
        blobs, offs = [], []
        pos = head
        for s in self.items:
            b = s.encode("cp932") + b"\0"
            offs.append(pos)
            blobs.append(b)
            pos += len(b)
        return struct.pack("<H", n) + struct.pack(f"<{n}H", *offs) + b"".join(blobs)


def die(msg):
    sys.exit(f"build_data: {msg}")


def load_cards(text):
    path = ROOT / "data" / "cards.csv"
    rows = []
    with open(path, encoding="utf-8") as f:
        for ln, r in enumerate(csv.DictReader(f), 2):
            try:
                rar = RARITY[r["rarity"]]
                elem = ELEMENT[r["element"]]
            except KeyError as e:
                die(f"{path.name}:{ln}: 不明な値 {e}")
            max_lv = int(r["max_lv"]) if r["max_lv"] else MAX_LV[rar]
            skill = int(r["skill"])
            if not 0 <= skill <= 4:
                die(f"{path.name}:{ln}: skill は 0..4")
            rows.append((int(r["id"]), text.add(r["name"]), rar, elem,
                         int(r["atk"]), int(r["def"]), int(r["cost"]), skill,
                         max_lv, int(r["evo_to"]), int(r["event_id"]), int(r["img"] or 0)))
    ids = [r[0] for r in rows]
    if ids != list(range(1, len(rows) + 1)):
        die(f"{path.name}: id は 1 から連番で並べる")
    for r in rows:
        if r[9] and r[9] not in ids:
            die(f"{path.name}: id {r[0]} の evo_to {r[9]} が存在しない")
    return rows


def write_cards_h(path, count):
    size = struct.calcsize(CARD_FMT)
    lines = [
        "/* cards.h — tools/build_data.py の生成物。手で編集しない */",
        "#ifndef DATA_CARDS_H", "#define DATA_CARDS_H", '#include "../types.h"', "",
        "typedef struct {",
    ]
    lines += [f"    {t} {n};" for t, n in CARD_FIELDS]
    lines += [
        "} CardRec;", "",
        f"#define CARD_REC_SIZE {size}",
        f"#define CARD_COUNT_BUILT {count}",
        "#define RARITY_N 0", "#define RARITY_R 1", "#define RARITY_SR 2", "#define RARITY_SSR 3",
        "#endif", "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")


def main():
    out = ROOT / "build"
    out.mkdir(exist_ok=True)
    (ROOT / "src" / "data").mkdir(exist_ok=True)
    text = TextTable()
    cards = load_cards(text)
    blob = struct.pack("<H", len(cards)) + b"".join(struct.pack(CARD_FMT, *r) for r in cards)
    tdat = text.pack()
    for name, b in (("cards.bin", blob), ("text.dat", tdat)):
        if len(b) > PAGE_PAYLOAD:
            die(f"{name} が {len(b)} B で 1 ページ ({PAGE_PAYLOAD} B) を超えた")
        (out / name).write_bytes(b)
    write_cards_h(ROOT / "src" / "data" / "cards.h", len(cards))
    print(f"build_data: cards {len(cards)} 枚 ({len(blob)} B), text {len(text.items)} 件 ({len(tdat)} B)")


if __name__ == "__main__":
    main()
