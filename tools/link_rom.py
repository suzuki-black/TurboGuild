#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""docs/MEMORY_MAP.md の表どおりに ASCII8 メガROM を組み立てる（仕様 §2.2）。

    link_rom.py header <out.h> <banks.mk>         PAGE_<名前> の #define と、ソース→バンク表を生成
    link_rom.py rom <kernel.ihx> <out.rom> [--text-dir DIR]
                                                  ROM イメージを生成。--text-dir は文字列のページ
                                                  （build/textN.dat）を DIR/textN.dat に置き換える（英語版など）

ihx の 0x4000-0x5FFF はページ 0（カーネル）。コードバンク n の領域 _BANKn は 0xn6010 に
リンクされ（24bit 番地, ihx の型 04 レコード）、ページ n の 6000h 窓の位置に置く。
ページ 0 以外は先頭 16 バイトに見出し [page,'T','G','P',種別,FF..] を置き、中身はオフセット 16 から。
同じ番地への二重書き込みはエラーにする（黙って後勝ちにしない）。
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAP = ROOT / "docs" / "MEMORY_MAP.md"
PAGE = 0x2000
HDR = 16
KERNEL_BASE, KERNEL_END = 0x4000, 0x6000
CODE_WIN, CODE_BASE, CODE_END = 0x6000, 0x6010, 0x8000
HOT_ADDR, HOT_CAP = 0xCC00, 0xE00     # RAM で実行するコード（MEMORY_MAP の hot: 行）。CC00h-D9FFh
KIND = {"コード": 1, "データA": 2, "データB": 3}


def parse_map():
    rows = []
    for line in MAP.read_text(encoding="utf-8").splitlines():
        cols = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cols) < 4 or not re.fullmatch(r"\d+(-\d+)?", cols[0]):
            continue
        a, _, b = cols[0].partition("-")
        lo, hi = int(a), int(b or a)
        kind = next((v for k, v in KIND.items() if k in cols[1]), 0)
        rows.append({"lo": lo, "hi": hi, "kind": kind, "name": cols[2], "src": cols[3]})
    if not rows:
        sys.exit("link_rom: MEMORY_MAP.md に表がない")
    return rows


def page_count(rows):
    return max(r["hi"] for r in rows) + 1


def code_files(r):
    return r["src"][5:].split() if r["src"].startswith("code:") else []


def hot_files(r):
    return r["src"][4:].split() if r["src"].startswith("hot:") else []


def cmd_header(out, banks_mk):
    rows = parse_map()
    mk = ["# banks.mk — tools/link_rom.py が docs/MEMORY_MAP.md から生成"]
    pages = []
    for r in rows:
        for f in code_files(r):
            if not (ROOT / f).exists():
                sys.exit(f"link_rom: MEMORY_MAP のページ {r['lo']} に載っている {f} が無い")
            mk.append(f"BANK_OF_{f} := {r['lo']}")
        if code_files(r):
            pages.append(str(r["lo"]))
        for f in hot_files(r):
            if not (ROOT / f).exists():
                sys.exit(f"link_rom: MEMORY_MAP のページ {r['lo']} に載っている {f} が無い")
            mk.append(f"HOT_OF_{f} := 1")
    mk.append(f"CODE_BANKS := {' '.join(pages)}")
    mk.append(f"HOT_ADDR := 0x{HOT_ADDR:04X}")
    write_if_changed(banks_mk, "\n".join(mk) + "\n")
    lines = ["/* rompages.h — tools/link_rom.py が docs/MEMORY_MAP.md から生成。手で編集しない */",
             "#ifndef DATA_ROMPAGES_H", "#define DATA_ROMPAGES_H",
             f"#define ROM_PAGE_COUNT {page_count(rows)}",
             f"#define HOT_ADDR 0x{HOT_ADDR:04X}", f"#define HOT_CAP {HOT_CAP}"]
    for r in rows:
        if r["name"] not in ("-", "") and r["lo"] == r["hi"]:
            lines.append(f"#define PAGE_{r['name']} {r['lo']}")
    lines += ["#endif", ""]
    write_if_changed(out, "\n".join(lines))


def write_if_changed(path, text):
    p = Path(path)    # 変化が無ければ触らない（再コンパイル回避）
    p.parent.mkdir(parents=True, exist_ok=True)
    if not p.exists() or p.read_text(encoding="utf-8") != text:
        p.write_text(text, encoding="utf-8")


def parse_ihx(path):
    mem = {}
    upper = 0
    for ln, line in enumerate(Path(path).read_text().splitlines(), 1):
        if not line.startswith(":"):
            continue
        raw = bytes.fromhex(line[1:])
        n, typ = raw[0], raw[3]
        addr = upper | (raw[1] << 8) | raw[2]
        if typ == 1:
            break
        if typ == 4:
            upper = ((raw[4] << 8) | raw[5]) << 16
            continue
        if typ != 0:
            sys.exit(f"link_rom: {path}:{ln}: 未対応のレコード {typ}")
        for i in range(n):
            if addr + i in mem:
                sys.exit(f"link_rom: 0x{addr+i:04X} が二重定義")
            mem[addr + i] = raw[4 + i]
    return mem


def split_code(mem, rows):
    """カーネル {addr: v} と、コードバンク {page: {addr16: v}} に分ける。"""
    code_pages = {r["lo"] for r in rows if code_files(r)}
    kern, banks, hot = {}, {}, {}
    for a, v in mem.items():
        page, a16 = a >> 16, a & 0xFFFF
        if page == 0 and KERNEL_BASE <= a16 < KERNEL_END:
            kern[a16] = v
        elif page == 0 and HOT_ADDR <= a16 < HOT_ADDR + HOT_CAP:
            hot[a16] = v
        elif page in code_pages and CODE_BASE <= a16 < CODE_END:
            banks.setdefault(page, {})[a16] = v
        else:
            sys.exit(f"link_rom: 0x{a:06X} にリンクされた（カーネルは 0x4000-0x5FFF の 8KB、"
                     f"コードバンク n は 0xn6010-0xn7FFF、RAM 実行は 0x{HOT_ADDR:04X} から {HOT_CAP} B。溢れていないか）")
    return kern, banks, hot


def cmd_rom(ihx, out, text_dir=None):
    rows = parse_map()
    npages = page_count(rows)
    rom = bytearray(b"\xFF" * (npages * PAGE))
    owner = {}

    def put(off, data, what):
        for i, b in enumerate(data):
            o = off + i
            if o in owner:
                sys.exit(f"link_rom: ROM 0x{o:05X} に {what} と {owner[o]} が重なった")
            owner[o] = what
            rom[o] = b

    kern, banks, hot = split_code(parse_ihx(ihx), rows)
    hot_page = next((r["lo"] for r in rows if hot_files(r)), None)
    if hot and hot_page is None:
        sys.exit("link_rom: RAM 実行のコードがあるのに MEMORY_MAP に hot: の行が無い")
    for a, v in hot.items():
        put(hot_page * PAGE + HDR + (a - HOT_ADDR), [v], "hot")
    for a, v in kern.items():
        put(a - KERNEL_BASE, [v], "kernel")
    for page, mem in banks.items():
        for a, v in mem.items():
            put(page * PAGE + (a - CODE_WIN), [v], f"bank{page}")

    for r in rows:
        for p in range(r["lo"], r["hi"] + 1):
            if p == 0:
                continue
            put(p * PAGE, bytes([p, ord("T"), ord("G"), ord("P"), r["kind"]]) + b"\xFF" * 11, "header")
            if r["src"] not in ("-", "", "kernel") and not code_files(r) and not hot_files(r):
                src = r["src"]
                if text_dir and re.fullmatch(r"build/text\d+\.dat", src):
                    src = f"{text_dir}/{Path(src).name}"      # 言語ごとの文字列のページ
                data = (ROOT / src).read_bytes()
                if len(data) > PAGE - HDR:
                    sys.exit(f"link_rom: {r['src']} が {len(data)} B で 1 ページに入らない")
                put(p * PAGE + HDR, data, r["src"])

    Path(out).parent.mkdir(parents=True, exist_ok=True)
    Path(out).write_bytes(rom)
    used0 = max(kern) - KERNEL_BASE + 1
    print(f"link_rom: {out} {len(rom)//1024}KB ASCII8 ({npages} ページ)")
    print(f"  kernel 0x4000-0x{KERNEL_BASE+used0-1:04X}  {used0:5d} / 8192 B (残り {8192-used0} B)")
    if hot:
        used = max(hot) - HOT_ADDR + 1
        print(f"  hot   0x{HOT_ADDR:04X}-0x{HOT_ADDR+used-1:04X}  {used:5d} / {HOT_CAP} B（RAM で実行。残り {HOT_CAP-used} B）")
    for page in sorted(banks):
        used = max(banks[page]) - CODE_WIN + 1
        print(f"  bank{page:<2d} 0x6000-0x{CODE_WIN+used-1:04X}  {used:5d} / 8192 B (残り {8192-used} B)")


def main(argv):
    if len(argv) == 4 and argv[1] == "header":
        cmd_header(argv[2], argv[3])
    elif len(argv) == 4 and argv[1] == "rom":
        cmd_rom(argv[2], argv[3])
    elif len(argv) == 6 and argv[1] == "rom" and argv[4] == "--text-dir":
        cmd_rom(argv[2], argv[3], argv[5])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv)
