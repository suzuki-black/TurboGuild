#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
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
import gen_npc  # noqa: E402
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
POOL = {"normal": 0, "normal_event": 1, "event2x": 2, "box": 3}   # box: n/r/sr/ssr は 100 個の箱の中身の個数
ITEM = {"gold": 0, "potion": 1, "ticket": 2, "gems": 3, "srticket": 4, "card": 5}
REWARD_KIND = {"point": 0, "rank": 1}

# (C の型, 名前)。struct の書式はこの表から作る
CARD_FIELDS = [("u16", "id"), ("u16", "name_text"), ("u8", "rarity"), ("u8", "element"),
               ("u16", "atk"), ("u16", "def"), ("u8", "cost"), ("u8", "skill"),
               ("u8", "max_lv"), ("u16", "evo_to"), ("u16", "event_id"), ("u8", "img"),
               ("u16", "release_day"), ("u16", "era_pct")]
TIMELINE_KIND = {"SERVICE_START": 0, "NEWS": 1, "EVENT": 2, "GACHA_RATEUP": 3, "INFLATION": 4,
                 "PLAYER_DROP": 5, "ANNOUNCE_END": 6, "SERVICE_END": 7, "GIFT": 8,
                 "GUILDWAR": 9}   # GUILDWAR: param = 第何回か。7 日間
TIMELINE_FIELDS = [("u16", "day"), ("u8", "kind"), ("u16", "param"), ("u16", "text")]
AREA_FIELDS = [("u16", "name_text"), ("u8", "element"), ("u16", "enemy_text"), ("u16", "boss_card")]
STAGE_FIELDS = [("u8", "ap"), ("u32", "hp"), ("u16", "def"), ("u16", "atk"), ("u16", "exp"),
                ("u16", "gold"), ("u8", "drop_rate"), ("u8", "drop_tbl")]
GACHA_FIELDS = [("u16", "name_text"), ("u8", "currency"), ("u16", "price"), ("u16", "price10"),
                ("u16", "rate_n"), ("u16", "rate_r"), ("u16", "rate_sr"), ("u16", "rate_ssr"),
                ("u16", "pity"), ("u8", "pool")]
LOGIN_FIELDS = [("u8", "item"), ("u16", "amount")]
COURSE_FIELDS = [("u16", "name_text"), ("u32", "start"), ("u32", "monthly"), ("u8", "infinite")]
PACK_FIELDS = [("u16", "gems"), ("u16", "price")]
EVENT_FIELDS = [("u16", "name_text"), ("u16", "story_text"), ("u16", "boss_card"), ("u16", "bonus_card"),
                ("u8", "bonus_mul"), ("u8", "evo_bonus_mul"), ("u8", "reward_tbl"), ("u8", "duration"),
                ("u8", "kind"), ("u16", "start_day"), ("u16", "era_pct")]
EVSTAGE_FIELDS = [("u8", "kind"), ("u8", "level"), ("u16", "name_text"), ("u8", "ap"), ("u32", "hp"),
                  ("u16", "def"), ("u16", "atk"), ("u16", "points"), ("u16", "exp"), ("u16", "gold")]
EVREWARD_FIELDS = [("u8", "tbl"), ("u8", "kind"), ("u32", "threshold"), ("u8", "item"), ("u16", "amount")]
NPC_FIELDS = [("u16", "name_id"), ("u8", "guild"), ("u8", "level"), ("u16", "power"),
              ("u32", "last_login"), ("u32", "event_pt"), ("u8", "activity"), ("u8", "flags")]
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


# ---- 言語（DECISIONS #77）: 日本語が元。ほかの言語は data/lang/<言語>.tsv で置き換える ----
#   「T_キー<TAB>訳」   UI の文（data/text/ui.txt）はキーで。同じ日本語でも画面ごとに訳し分けられる
#   「日本語<TAB>訳」   それ以外（カード名・イベント・お知らせ・NPC の名前など）は日本語の文で
#   3 列目に ! を書くと、日本語より長い訳も通す（置き場所に余裕があるのを画面で確かめたもの）

LANG_DIR = ROOT / "data" / "lang"


def width(s):
    """画面の桁数（全角 2・半角 1）。改行を含む文は一番長い行"""
    def w1(line):
        return sum(1 if ord(c) < 0x80 or 0xFF61 <= ord(c) <= 0xFF9F else 2 for c in line)
    return max(w1(line) for line in s.split("\n"))


def load_lang(lang):
    """訳の辞書 {日本語: (訳, 幅の検査を外すか)}。\\n は改行"""
    path = LANG_DIR / f"{lang}.tsv"
    if not path.exists():
        die(f"{path} が無い")
    d = {}
    for ln, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        cols = line.split("\t")
        if len(cols) < 2:
            die(f"{path.name}:{ln}: '日本語<TAB>訳' の形で書く")
        ja, tr = cols[0].replace("\\n", "\n"), cols[1].replace("\\n", "\n")
        if ja in d:
            die(f"{path.name}:{ln}: 「{ja}」が 2 回ある")
        d[ja] = (tr, len(cols) > 2 and cols[2].strip() == "!")
    return d


def translate(text, lang):
    """text.items を訳した並び。訳が無い・長すぎる・使えない字があれば、まとめて出して止める"""
    d = load_lang(lang)
    key_of = {i: k for k, i in text.keys.items() if k in text.ui}
    out, missing, errors = [], [], []
    used = set()
    for i, ja in enumerate(text.items):
        k = key_of.get(i)
        if k and k in d:                             # UI はキーの訳を優先
            hit = k
        elif ja in d:
            hit = ja
        elif all(ord(c) < 0x80 for c in ja):         # もともと半角だけの文（TurboGuild・URL など）はそのまま
            out.append(ja if ja else " ")
            continue
        else:
            missing.append(k if k else ja)
            out.append(ja)
            continue
        used.add(hit)
        tr, free = d[hit]
        limit = max(width(ja), text.maxw.get(i, 0))
        if not free and width(tr) > limit:
            errors.append(f"長すぎる（{width(tr)} 桁 > {limit} 桁）: {k or ja} -> {tr}")
        bad = [c for c in tr if not (c == "\n" or 0x20 <= ord(c) < 0x7F)]
        if bad:
            errors.append(f"半角の英数字と記号だけにする（{''.join(bad)}）: {tr}")
        out.append(tr if tr else " ")
    unused = [ja for ja in d if ja not in used]
    for u in unused:
        print(f"build_data: [{lang}] 使われていない訳: {u}")
    if missing:
        miss = LANG_DIR / f"{lang}.missing.tsv"
        seen = []
        for m in missing:
            if m not in seen:
                seen.append(m)
        miss.write_text("".join(f"{m.replace(chr(10), chr(92) + 'n')}\t\n" for m in seen), encoding="utf-8")
        errors.append(f"訳が無いものが {len(seen)} 件 → {miss.relative_to(ROOT)} に書き出した（訳を書いて {lang}.tsv へ足す）")
    if errors:
        die(f"[{lang}] " + "\n  ".join([""] + errors))
    return out


def lookup(table, key, where):
    if key not in table:
        die(f"{where}: 不明な値 '{key}'（使えるのは {', '.join(table)}）")
    return table[key]


class TextTable:
    def __init__(self):
        self.items, self.index, self.keys = [], {}, {}
        self.ui = set()      # data/text/ui.txt のキー（訳はキーで引く）
        self.maxw = {}       # 番号 → 訳してよい桁数（日本語の幅より大きいとき。置き場所の幅）

    def add(self, s, key=None, maxw=0, ui=False):
        if key:
            if key in self.keys:
                die(f"text キー {key} が重複")
            self.keys[key] = len(self.items)
            self.items.append(s)          # キー付きは同じ文でも別 id（画面ごとに書き換えられるように）
            if ui:
                self.ui.add(key)
            i = self.keys[key]
        else:
            if s not in self.index:
                self.index[s] = len(self.items)
                self.items.append(s)
            i = self.index[s]
        if maxw:
            self.maxw[i] = max(self.maxw.get(i, 0), maxw)
        return i

    def check_jis1(self):
        """漢字ROM は JIS 第1水準だけ。第2水準の字は出ないので先に止める。"""
        for s in self.items:
            for ch in s:
                b = ch.encode("cp932", errors="strict")
                if len(b) == 2:
                    j = ch.encode("iso2022_jp", errors="ignore")[3:5]
                    if len(j) != 2 or j[0] >= 0x50:
                        die(f"'{ch}'（「{s}」）は漢字ROM（JIS 第1水準）に無い")

    def pack(self, items=None):
        """文字列のページを作る。items を渡すとその並び（訳した文）で作る（番号は日本語と同じ）"""
        if items is None:
            self.check_jis1()
            items = self.items
        n = len(items)
        pages = [bytearray()]
        dir_size = 2 + 3 * n
        pos = dir_size
        entries = []
        for s in items:
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
        if path.name == "credits.txt":          # スタッフロールは load_credits が読む
            continue
        for ln, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if not line.strip() or line.startswith("#"):
                continue
            if "\t" not in line:
                die(f"{path.name}:{ln}: 'キー<TAB>文字列' の形で書く")
            key, s = line.split("\t", 1)
            if not key.startswith("T_"):
                die(f"{path.name}:{ln}: キーは T_ で始める")
            text.add(s.replace("\\n", "\n"), key, ui=True)


def load_timeline(text):
    rows = []
    last = -1
    for ln, r in read_csv("timeline.csv"):
        where = f"timeline.csv:{ln}"
        day = int(r["day"])
        if day < last:
            die(f"{where}: day は小さい順に並べる")
        last = day
        rows.append((day, lookup(TIMELINE_KIND, r["kind"], where), int(r["param"] or 0), text.add(r["text"])))
    if not rows or rows[0][1] != 0 or rows[-1][1] != TIMELINE_KIND["SERVICE_END"]:
        die("timeline.csv: SERVICE_START で始めて SERVICE_END で終える")
    return rows


def era_pct(timeline, day):
    """その日の時点のインフレ倍率（%）。INFLATION 行の param のうち、その日までで最後のもの"""
    pct = 100
    for d, kind, param, _t in timeline:
        if kind == TIMELINE_KIND["INFLATION"] and d <= day:
            pct = param
    return pct


def load_cards(text, timeline):
    rows = []
    for ln, r in read_csv("cards.csv"):
        where = f"cards.csv:{ln}"
        rar = lookup(RARITY, r["rarity"], where)
        skill = int(r["skill"])
        if not 0 <= skill <= 4:
            die(f"{where}: skill は 0..4（4 種で固定）")
        rows.append((int(r["id"]), text.add(r["name"], maxw=14), rar, lookup(ELEMENT, r["element"], where),
                     int(r["atk"]), int(r["def"]), int(r["cost"]), skill,
                     int(r["max_lv"]) if r["max_lv"] else MAX_LV[rar],
                     int(r["evo_to"]), int(r["event_id"]), int(r["img"] or 0),
                     int(r["release_day"] or 0), era_pct(timeline, int(r["release_day"] or 0))))
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
        rows.append((text.add(r["name"], maxw=16), lookup(ELEMENT, r["element"], where),
                     text.add(r["enemy"], maxw=14), boss))
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


def load_courses(text):
    rows = []
    for ln, r in read_csv("courses.csv"):
        if int(r["id"]) != len(rows) + 1:
            die(f"courses.csv:{ln}: id は 1 から連番")
        rows.append((text.add(r["name"]), int(r["start"]), int(r["monthly"]), int(r["infinite"])))
    return rows


def load_packs():
    rows = []
    for ln, r in read_csv("gem_packs.csv"):
        if int(r["id"]) != len(rows) + 1:
            die(f"gem_packs.csv:{ln}: id は 1 から連番")
        rows.append((int(r["gems"]), int(r["price"])))
    return rows


def load_events(text, ncards, timeline):
    starts = {param: day for day, kind, param, _t in timeline if kind == TIMELINE_KIND["EVENT"]}
    rows = []
    for ln, r in read_csv("events.csv"):
        where = f"events.csv:{ln}"
        if int(r["id"]) != len(rows) + 1:
            die(f"{where}: id は 1 から連番")
        for k in ("boss_card_id", "bonus_card_id"):
            if not 1 <= int(r[k]) <= ncards:
                die(f"{where}: {k} のカードが無い")
        eid = int(r["id"])
        if eid not in starts:
            die(f"{where}: timeline.csv に EVENT,{eid} の行が無い（開催日は年表で決める）")
        rows.append((text.add(r["name"], maxw=18), text.add(r["story"], maxw=48), int(r["boss_card_id"]), int(r["bonus_card_id"]),
                     int(r["bonus_mul"]), int(r["evo_bonus_mul"]), int(r["reward_tbl_id"]),
                     int(r["duration_days"]), int(r["kind"]), starts[eid], era_pct(timeline, starts[eid])))
    return rows


def load_event_stages(text):
    return [(int(r["kind"]), int(r["level"]), text.add(r["name"]), int(r["ap"]), int(r["hp"]), int(r["def"]),
             int(r["atk"]), int(r["points"]), int(r["exp"]), int(r["gold"])) for _ln, r in read_csv("event_stages.csv")]


def load_event_rewards(ncards):
    rows = []
    for ln, r in read_csv("event_rewards.csv"):
        where = f"event_rewards.csv:{ln}"
        item = lookup(ITEM, r["item"], where)
        if item == ITEM["card"] and not 1 <= int(r["amount"]) <= ncards:
            die(f"{where}: カード {r['amount']} が無い")
        rows.append((int(r["tbl"]), lookup(REWARD_KIND, r["kind"], where), int(r["threshold"]), item, int(r["amount"])))
    # 同じ表・同じ種類の中は閾値の昇順（C 側は前から見る）
    rows.sort(key=lambda x: (x[0], x[1], x[2]))
    return rows


def load_npc_texts(text):
    pre, suf = [], []
    for ln, r in read_csv("npc_names.csv"):
        (pre if r["part"] == "pre" else suf).append(r["text"])
    if len(pre) * len(suf) < gen_npc.COUNT:
        die("npc_names.csv: 前半 × 後半 が 1000 に足りない")
    first_pre = len(text.items)
    for i, s in enumerate(pre):
        text.add(s, f"T_NPC_PRE{i}")
    first_suf = len(text.items)
    for i, s in enumerate(suf):
        text.add(s, f"T_NPC_SUF{i}")
    guilds = [r["name"] for _ln, r in read_csv("guilds.csv")]
    first_guild = len(text.items)
    for i, s in enumerate(guilds):
        text.add(s, f"T_GUILD{i}", maxw=14)
    return first_pre, len(pre), first_suf, len(suf), first_guild, len(guilds)


def load_credits(text):
    """スタッフロール（data/text/credits.txt）。# の行は注釈。空行も 1 行として流す"""
    first = len(text.items)
    n = 0
    for ln, s in enumerate((ROOT / "data" / "text" / "credits.txt").read_text(encoding="utf-8").splitlines(), 1):
        if s.startswith("#"):
            continue
        if sum(2 if ord(c) > 0x7F else 1 for c in s) > 32:
            die(f"credits.txt:{ln}: 32 桁を超える")
        text.add(s or " ", f"T_CREDIT{n}", maxw=32)
        n += 1
    return first, n


def c_struct(name, fields):
    return ["typedef struct {"] + [f"    {t} {n};" for t, n in fields] + [f"}} {name};", ""]


def write_if_changed(path, text):
    if not path.exists() or path.read_text(encoding="utf-8") != text:
        path.write_text(text, encoding="utf-8")


def write_headers(cards, areas, stages, gacha, text, courses, packs, extra):
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
                         ("GachaRec", GACHA_FIELDS), ("LoginRec", LOGIN_FIELDS),
                         ("CourseRec", COURSE_FIELDS), ("PackRec", PACK_FIELDS),
                         ("EventRec", EVENT_FIELDS), ("EvStageRec", EVSTAGE_FIELDS),
                         ("EvRewardRec", EVREWARD_FIELDS), ("NpcRec", NPC_FIELDS),
                         ("TimelineRec", TIMELINE_FIELDS)):
        t += c_struct(name, fields)
    t += PACK_OFF
    t += [f"#define AREA_COUNT {len(areas)}", f"#define STAGE_COUNT {len(stages)}",
          f"#define STAGES_PER_AREA {gen_stages.STAGES_PER_AREA}",
          f"#define GACHA_COUNT {len(gacha)}", "#define LOGIN_DAYS 7",
          f"#define COURSE_COUNT {len(courses)}", f"#define PACK_COUNT {len(packs)}"]
    t += [f"#define {k} {v}" for k, v in extra.items()]
    t += [f"#define REWARD_{k.upper()} {v}" for k, v in REWARD_KIND.items()]
    t += [f"#define TL_{k} {v}" for k, v in TIMELINE_KIND.items()]
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
    lang = sys.argv[sys.argv.index("--lang") + 1] if "--lang" in sys.argv else "ja"
    OUT.mkdir(exist_ok=True)
    GEN.mkdir(exist_ok=True)
    text = TextTable()
    load_ui_text(text)
    timeline = load_timeline(text)
    cards = load_cards(text, timeline)
    areas = load_areas(text, len(cards))
    stages = gen_stages.generate(ROOT / "data" / "stages_rule.csv", areas)
    gacha = load_gacha(text)
    login = load_login()
    courses = load_courses(text)
    packs = load_packs()
    events = load_events(text, len(cards), timeline)
    evstages = load_event_stages(text)
    evrewards = load_event_rewards(len(cards))
    fp, np_, fs, ns, fg, ng = load_npc_texts(text)
    fc, nc = load_credits(text)
    npcs, by_power = gen_npc.generate(np_, ns, ng)
    npc_blob = gen_npc.pack(npcs)
    per = PAGE_PAYLOAD // 16
    extra = {"EVENT_COUNT": len(events), "EVSTAGE_COUNT": len(evstages), "EVREWARD_COUNT": len(evrewards),
             "NPC_COUNT": len(npcs), "NPC_PER_PAGE": per, "NPC_PRE_TEXT": fp, "NPC_PRE_COUNT": np_,
             "NPC_SUF_TEXT": fs, "NPC_SUF_COUNT": ns, "GUILD_TEXT": fg, "GUILD_COUNT": ng,
             "TIMELINE_COUNT": len(timeline), "SERVICE_END_DAY": timeline[-1][0],
             "CREDIT_TEXT": fc, "CREDIT_COUNT": nc}
    blobs = {
        "cards.bin": pack_table(CARD_FIELDS, cards),
        "areas.bin": pack_table(AREA_FIELDS, areas),
        "stages.bin": pack_table(STAGE_FIELDS, stages),
        "gacha.bin": pack_table(GACHA_FIELDS, gacha),
        "login.bin": pack_table(LOGIN_FIELDS, login),
        "courses.bin": pack_table(COURSE_FIELDS, courses),
        "packs.bin": pack_table(PACK_FIELDS, packs),
        "events.bin": pack_table(EVENT_FIELDS, events),
        "evstages.bin": pack_table(EVSTAGE_FIELDS, evstages),
        "evrewards.bin": pack_table(EVREWARD_FIELDS, evrewards),
        "npc0.bin": npc_blob[:per * 16],
        "npc1.bin": npc_blob[per * 16:],
        "npcpow.bin": struct.pack(f"<{len(by_power)}H", *by_power),
        "timeline.bin": pack_table(TIMELINE_FIELDS, timeline),
    }
    if lang != "ja":                 # ほかの言語は文字列のページだけ作る（番号も表もほかのページも日本語と同じ）
        pages = text.pack(translate(text, lang))
        out = OUT / lang
        out.mkdir(exist_ok=True)
        for i in range(TEXT_PAGES):
            (out / f"text{i}.dat").write_bytes(bytes(pages[i]) if i < len(pages) else b"")
        print(f"build_data: [{lang}] text {len(text.items)} 件")
        return
    pages = text.pack()
    for i in range(TEXT_PAGES):
        blobs[f"text{i}.dat"] = bytes(pages[i]) if i < len(pages) else b""
    for name, b in blobs.items():
        if len(b) > PAGE_PAYLOAD:
            die(f"{name} が {len(b)} B で 1 ページ ({PAGE_PAYLOAD} B) を超えた")
        (OUT / name).write_bytes(b)
    write_headers(cards, areas, stages, gacha, text, courses, packs, extra)
    print(f"build_data: cards {len(cards)}, areas {len(areas)}, stages {len(stages)} "
          f"({len(blobs['stages.bin'])} B), gacha {len(gacha)}, text {len(text.items)} 件")


if __name__ == "__main__":
    main()
