#!/usr/bin/env python3
"""docs/MEMORY_MAP.md の表どおりに ASCII8 メガROM を組み立てる（仕様 §2.2）。

    link_rom.py header <out.h>                    PAGE_<名前> の #define を生成
    link_rom.py rom <kernel.ihx> <out.rom>        ROM イメージを生成

ihx の 0x4000-0x5FFF はページ 0（カーネル）、0x6010-0x7FFF は表で `bank1` と書いたページへ置く。
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
BANK1_WIN, BANK1_BASE, BANK1_END = 0x6000, 0x6010, 0x8000
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


def cmd_header(out):
    rows = parse_map()
    lines = ["/* rompages.h — tools/link_rom.py が docs/MEMORY_MAP.md から生成。手で編集しない */",
             "#ifndef DATA_ROMPAGES_H", "#define DATA_ROMPAGES_H",
             f"#define ROM_PAGE_COUNT {page_count(rows)}"]
    for r in rows:
        if r["name"] not in ("-", "") and r["lo"] == r["hi"]:
            lines.append(f"#define PAGE_{r['name']} {r['lo']}")
    lines += ["#endif", ""]
    text = "\n".join(lines)
    p = Path(out)
    if not p.exists() or p.read_text(encoding="utf-8") != text:   # 変化が無ければ触らない（再コンパイル回避）
        p.write_text(text, encoding="utf-8")


def parse_ihx(path):
    mem = {}
    for ln, line in enumerate(Path(path).read_text().splitlines(), 1):
        if not line.startswith(":"):
            continue
        raw = bytes.fromhex(line[1:])
        n, addr, typ = raw[0], (raw[1] << 8) | raw[2], raw[3]
        if typ == 1:
            break
        if typ != 0:
            sys.exit(f"link_rom: {path}:{ln}: 未対応のレコード {typ}")
        for i in range(n):
            if addr + i in mem:
                sys.exit(f"link_rom: 0x{addr+i:04X} が二重定義")
            mem[addr + i] = raw[4 + i]
    return mem


def split_code(mem, rows):
    bad = [a for a in mem if not (KERNEL_BASE <= a < KERNEL_END or BANK1_BASE <= a < BANK1_END)]
    if bad:
        sys.exit(f"link_rom: 0x{min(bad):04X} にリンクされた"
                 "（カーネルは 0x4000-0x5FFF の 8KB、バンク1 は 0x6010-0x7FFF）")
    kern = {a: v for a, v in mem.items() if a < KERNEL_END}
    bank1 = {a: v for a, v in mem.items() if a >= BANK1_BASE}
    page1 = next((r["lo"] for r in rows if r["src"] == "bank1"), None)
    if bank1 and page1 is None:
        sys.exit("link_rom: BANK1 のコードがあるのに MEMORY_MAP に bank1 のページが無い")
    return kern, bank1, page1


def cmd_rom(ihx, out):
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

    kern, bank1, page1 = split_code(parse_ihx(ihx), rows)
    for a, v in kern.items():
        put(a - KERNEL_BASE, [v], "kernel")
    for a, v in bank1.items():
        put(page1 * PAGE + (a - BANK1_WIN), [v], "bank1")

    for r in rows:
        for p in range(r["lo"], r["hi"] + 1):
            if p == 0:
                continue
            put(p * PAGE, bytes([p, ord("T"), ord("G"), ord("P"), r["kind"]]) + b"\xFF" * 11, "header")
            if r["src"] not in ("-", "", "kernel", "bank1"):
                data = (ROOT / r["src"]).read_bytes()
                if len(data) > PAGE - HDR:
                    sys.exit(f"link_rom: {r['src']} が {len(data)} B で 1 ページに入らない")
                put(p * PAGE + HDR, data, r["src"])

    Path(out).parent.mkdir(parents=True, exist_ok=True)
    Path(out).write_bytes(rom)
    used0 = max(kern) - KERNEL_BASE + 1
    used1 = (max(bank1) - BANK1_WIN + 1) if bank1 else HDR
    print(f"link_rom: {out} {len(rom)//1024}KB ASCII8 ({npages} ページ)")
    print(f"  kernel 0x4000-0x{KERNEL_BASE+used0-1:04X}  {used0} / 8192 B (残り {8192-used0} B)")
    print(f"  bank1  0x6000-0x{BANK1_WIN+used1-1:04X}  {used1} / 8192 B (残り {8192-used1} B)")


def main(argv):
    if len(argv) == 3 and argv[1] == "header":
        cmd_header(argv[2])
    elif len(argv) == 4 and argv[1] == "rom":
        cmd_rom(argv[2], argv[3])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv)
