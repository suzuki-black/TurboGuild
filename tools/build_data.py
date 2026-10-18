#!/usr/bin/env python3
"""data/* を固定長レコードのバイナリと C ヘッダへ変換する（仕様 §4）。

    build/cards.bin  build/areas.bin  build/stages.bin  build/gacha.bin  build/login.bin
        どれも u16 レコード数 + 固定長レコード × n
    build/text0.dat, text1.dat …
        text0 の先頭に u16 文字列数 + 目次 {u8 ページ, u16 オフセット} × n。
        文字列は Shift-JIS(cp932) の NUL 終端で、入りきらなければ次のページへ溢れる。
    src/data/cards.h  tables.h  text_ids.h   （生成物。手で編集しない）

元データ（CSV・data/text/*.txt）は UTF-8。文字列は CSV に直接書き、text_id はここで振る。
"""
import csv
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_stages  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build"
GEN = ROOT / "src" / "data"
PAGE_PAYLOAD = 8192 - 16
TEXT_PAGES = 4                     # docs/MEMORY_MAP.md の TEXT0..TEXT3 と一致させる

RARITY = {"N": 0, "R": 1, "SR": 2, "SSR": 3}
ELEMENT = {"炎": 0, "水": 1, "雷": 2, "光": 3, "闇": 4}
MAX_LV = {0: 20, 1: 30, 2: 40, 3: 50}
CURRENCY = {"free": 0, "gold": 1, "gems": 2, "ticket": 3, "srticket": 4}
POOL = {"normal": 0, "normal_event": 1, "event2x": 2}
ITEM = {"gold": 0, "potion": 1, "ticket": 2, "gems": 3, "srticket": 4}

# (C の型, 名前)。struct の書式はこの表から作る
CARD_FIELDS = [("u16", "id"), ("u16", "name_text"), ("u8", "rarity"), ("u8", "element"),
               ("u16", "atk"), ("u16", "def"), ("u8", "cost"), ("u8", "skill"),
               ("u8", "max_lv"), ("u16", "evo_to"), ("u16", "event_id"), ("u8", "img")]
AREA_FIELDS = [("u16", "name_text"), ("u8", "element"), ("u16", "enemy_text"), ("u16", "boss_card")]
STAGE_FIELDS = [("u8", "ap"), ("u32", "hp"), ("u16", "def"), ("u16", "atk"), ("u16", "exp"),
                ("u16", "gold"), ("u8", "drop_rate"), ("u8", "drop_tbl")]
GACHA_FIELDS = [("u16", "name_text"), ("u8", "currency"), ("u16", "price"), ("u16", "price10"),
                ("u16", "rate_n"), ("u16", "rate_r"), ("u16", "rate_sr"), ("u16", "rate_ssr"),
                ("u16", "pity"), ("u8", "pool")]
LOGIN_FIELDS = [("u8", "item"), ("u16", "amount")]
FMT = {"u8": "B", "u16": "H", "u32": "I"}


def die(msg):
    sys.exit(f"build_data: {msg}")


def fmt_of(fields):
    return "<" + "".join(FMT[t] for t, _ in fields)


def pack_table(fields, rows):
    f = fmt_of(fields)
    try:
        return struct.pack("<H", len(rows)) + b"".join(struct.pack(f, *r) for r in rows)
    except struct.error as e:
        die(f"値が型に入らない: {e}")


def read_csv(name):
    path = ROOT / "data" / name
    with open(path, encoding="utf-8") as f:
        return [(ln, r) for ln, r in enumerate(csv.DictReader(f), 2)]


def lookup(table, key, where):
    if key not in table:
        die(f"{where}: 不明な値 '{key}'（使えるのは {', '.join(table)}）")
    return table[key]


class TextTable:
    def __init__(self):
        self.items, self.index, self.keys = [], {}, {}

    def add(self, s, key=None):
        if key:
            if key in self.keys:
                die(f"text キー {key} が重複")
            self.keys[key] = len(self.items)
            self.items.append(s)          # キー付きは同じ文でも別 id（画面ごとに書き換えられるように）
            return self.keys[key]
        if s not in self.index:
            self.index[s] = len(self.items)
            self.items.append(s)
        return self.index[s]

    def check_jis1(self):
        """漢字ROM は JIS 第1水準だけ。第2水準の字は出ないので先に止める。"""
        for s in self.items:
            for ch in s:
                b = ch.encode("cp932", errors="strict")
                if len(b) == 2:
                    j = ch.encode("iso2022_jp", errors="ignore")[3:5]
                    if len(j) != 2 or j[0] >= 0x50:
                        die(f"'{ch}'（「{s}」）は漢字ROM（JIS 第1水準）に無い")

    def pack(self):
        self.check_jis1()
        n = len(self.items)
        pages = [bytearray()]
        dir_size = 2 + 3 * n
        pos = dir_size
        entries = []
        for s in self.items:
            b = s.encode("cp932") + b"\0"
            if pos + len(b) > PAGE_PAYLOAD:
                pages.append(bytearray())
                pos = 0
            entries.append((len(pages) - 1, pos))
            pages[-1] += b
            pos += len(b)
        if dir_size > PAGE_PAYLOAD or len(pages) > TEXT_PAGES:
            die(f"文字列が {TEXT_PAGES} ページに入らない")
        head = struct.pack("<H", n) + b"".join(struct.pack("<BH", p, o) for p, o in entries)
        pages[0] = bytearray(head) + pages[0]
        return pages


def load_ui_text(text):
    for path in sorted((ROOT / "data" / "text").glob("*.txt")):
        for ln, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if not line.strip() or line.startswith("#"):
                continue
            if "\t" not in line:
                die(f"{path.name}:{ln}: 'キー<TAB>文字列' の形で書く")
            key, s = line.split("\t", 1)
            if not key.startswith("T_"):
                die(f"{path.name}:{ln}: キーは T_ で始める")
            text.add(s.replace("\\n", "\n"), key)


def load_cards(text):
    rows = []
    for ln, r in read_csv("cards.csv"):
        where = f"cards.csv:{ln}"
        rar = lookup(RARITY, r["rarity"], where)
        skill = int(r["skill"])
        if not 0 <= skill <= 4:
            die(f"{where}: skill は 0..4（4 種で固定）")
        rows.append((int(r["id"]), text.add(r["name"]), rar, lookup(ELEMENT, r["element"], where),
                     int(r["atk"]), int(r["def"]), int(r["cost"]), skill,
                     int(r["max_lv"]) if r["max_lv"] else MAX_LV[rar],
                     int(r["evo_to"]), int(r["event_id"]), int(r["img"] or 0)))
    ids = [r[0] for r in rows]
    if ids != list(range(1, len(rows) + 1)):
        die("cards.csv: id は 1 から連番で並べる")
    for r in rows:
        if r[9] and r[9] not in ids:
            die(f"cards.csv: id {r[0]} の evo_to {r[9]} が存在しない")
    return rows


def load_areas(text, ncards):
    rows = []
    for ln, r in read_csv("areas.csv"):
        where = f"areas.csv:{ln}"
        if int(r["id"]) != len(rows) + 1:
            die(f"{where}: id は 1 から連番")
        boss = int(r["boss_card_id"])
        if not 1 <= boss <= ncards:
            die(f"{where}: boss_card_id {boss} が存在しない")
        rows.append((text.add(r["name"]), lookup(ELEMENT, r["element"], where),
                     text.add(r["enemy"]), boss))
    return rows


def load_gacha(text):
    rows = []
    for ln, r in read_csv("gacha.csv"):
        where = f"gacha.csv:{ln}"
        rates = [int(r[k]) for k in ("n", "r", "sr", "ssr")]
        if sum(rates) != 100:
            die(f"{where}: N+R+SR+SSR が 100 にならない")
        if int(r["id"]) != len(rows) + 1:
            die(f"{where}: id は 1 から連番")
        rows.append((text.add(r["name"]), lookup(CURRENCY, r["currency"], where),
                     int(r["price"]), int(r["price10"]), *[x * 100 for x in rates],
                     int(r["pity"]), lookup(POOL, r["pool"], where)))
    return rows


def load_login():
    rows = []
    for ln, r in read_csv("login_bonus.csv"):
        if int(r["day"]) != len(rows) + 1:
            die(f"login_bonus.csv:{ln}: day は 1 から連番")
        rows.append((lookup(ITEM, r["item"], f"login_bonus.csv:{ln}"), int(r["amount"])))
    if len(rows) != 7:
        die("login_bonus.csv: 7 日ぶん書く")
    return rows


# SDCC(Z80) は詰めて並べる。PC のテストで同じバイナリを読めるよう gcc でも詰めさせる
PACK_ON = ["#ifndef __SDCC", "#pragma pack(push, 1)", "#endif", ""]
PACK_OFF = ["#ifndef __SDCC", "#pragma pack(pop)", "#endif", ""]


def c_struct(name, fields):
    return ["typedef struct {"] + [f"    {t} {n};" for t, n in fields] + [f"}} {name};", ""]


def write_if_changed(path, text):
    if not path.exists() or path.read_text(encoding="utf-8") != text:
        path.write_text(text, encoding="utf-8")


def write_headers(cards, areas, stages, gacha, text):
    h = ["/* cards.h — tools/build_data.py の生成物。手で編集しない */",
         "#ifndef DATA_CARDS_H", "#define DATA_CARDS_H", '#include "../types.h"', ""] + PACK_ON
    h += c_struct("CardRec", CARD_FIELDS) + PACK_OFF
    h += [f"#define CARD_COUNT_BUILT {len(cards)}",
          "#define RARITY_N 0", "#define RARITY_R 1", "#define RARITY_SR 2", "#define RARITY_SSR 3",
          "#define ELEMENTS 5", "#endif", ""]
    write_if_changed(GEN / "cards.h", "\n".join(h))

    t = ["/* tables.h — tools/build_data.py の生成物。手で編集しない */",
         "#ifndef DATA_TABLES_H", "#define DATA_TABLES_H", '#include "../types.h"', ""] + PACK_ON
    for name, fields in (("AreaRec", AREA_FIELDS), ("StageRec", STAGE_FIELDS),
                         ("GachaRec", GACHA_FIELDS), ("LoginRec", LOGIN_FIELDS)):
        t += c_struct(name, fields)
    t += PACK_OFF
    t += [f"#define AREA_COUNT {len(areas)}", f"#define STAGE_COUNT {len(stages)}",
          f"#define STAGES_PER_AREA {gen_stages.STAGES_PER_AREA}",
          f"#define GACHA_COUNT {len(gacha)}", "#define LOGIN_DAYS 7"]
    t += [f"#define CUR_{k.upper()} {v}" for k, v in CURRENCY.items()]
    t += [f"#define POOL_{k.upper()} {v}" for k, v in POOL.items()]
    t += [f"#define ITEM_{k.upper()} {v}" for k, v in ITEM.items()]
    t += ["#define DROP_RARITY(t) ((u8)((t) >> 4))", "#define DROP_ELEMENT(t) ((u8)((t) & 15))",
          "#endif", ""]
    write_if_changed(GEN / "tables.h", "\n".join(t))

    x = ["/* text_ids.h — tools/build_data.py の生成物。手で編集しない */",
         "#ifndef DATA_TEXT_IDS_H", "#define DATA_TEXT_IDS_H",
         f"#define TEXT_COUNT {len(text.items)}"]
    x += [f"#define {k} {v}" for k, v in text.keys.items()]
    x += ["#endif", ""]
    write_if_changed(GEN / "text_ids.h", "\n".join(x))


def main():
    OUT.mkdir(exist_ok=True)
    GEN.mkdir(exist_ok=True)
    text = TextTable()
    load_ui_text(text)
    cards = load_cards(text)
    areas = load_areas(text, len(cards))
    stages = gen_stages.generate(ROOT / "data" / "stages_rule.csv", areas)
    gacha = load_gacha(text)
    login = load_login()
    blobs = {
        "cards.bin": pack_table(CARD_FIELDS, cards),
        "areas.bin": pack_table(AREA_FIELDS, areas),
        "stages.bin": pack_table(STAGE_FIELDS, stages),
        "gacha.bin": pack_table(GACHA_FIELDS, gacha),
        "login.bin": pack_table(LOGIN_FIELDS, login),
    }
    pages = text.pack()
    for i in range(TEXT_PAGES):
        blobs[f"text{i}.dat"] = bytes(pages[i]) if i < len(pages) else b""
    for name, b in blobs.items():
        if len(b) > PAGE_PAYLOAD:
            die(f"{name} が {len(b)} B で 1 ページ ({PAGE_PAYLOAD} B) を超えた")
        (OUT / name).write_bytes(b)
    write_headers(cards, areas, stages, gacha, text)
    print(f"build_data: cards {len(cards)}, areas {len(areas)}, stages {len(stages)} "
          f"({len(blobs['stages.bin'])} B), gacha {len(gacha)}, text {len(text.items)} 件")


if __name__ == "__main__":
    main()
