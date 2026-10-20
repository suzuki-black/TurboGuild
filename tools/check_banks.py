#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""別バンクの関数・定数を直接参照していないかをリンク後に調べる（DECISIONS #14）。

    check_banks.py <kernel.map> <banks.mk> <objdir>

SDCC は 24bit 番地の下位 16bit で call/jp を作るので、別バンクの関数を直接呼んでも
リンクは通り、実行すると今の窓の中の別物に飛ぶ。ここで止める。
許すもの: カーネル・RAM への参照、自分のバンクへの参照、生成された中継（far_stubs）からの参照。
"""
import re
import sys
from pathlib import Path

SYM_RE = re.compile(r"^\s+([0-9A-Fa-f]{8})\s+(\w+)\s+(\S+)?")
REF_RE = re.compile(r"(?<![\w.])(_\w+)")
INSN_RE = re.compile(r"^\s+(call|jp|jr|ld|\.dw|\.db|push|ex|cp|add|sub|and|or|xor)\b(.*)$")


def load_syms(mapf):
    syms = {}
    for line in Path(mapf).read_text(encoding="utf-8", errors="ignore").splitlines():
        m = SYM_RE.match(line)
        if m and m.group(2).startswith("_") and not m.group(2).startswith("__"):
            syms[m.group(2)] = int(m.group(1), 16)
    return syms


def load_banks(mk):
    banks = {}
    for line in Path(mk).read_text().splitlines():
        m = re.match(r"BANK_OF_(\S+)\s*:=\s*(\d+)", line)
        if m:
            banks[Path(m.group(1)).with_suffix("").as_posix().removeprefix("src/")] = int(m.group(2))
    return banks


def region(addr):
    page, a16 = addr >> 16, addr & 0xFFFF
    if page:
        return page
    return 0   # カーネル・RAM・定数番地


def main(mapf, mk, objdir):
    syms = load_syms(mapf)
    banks = load_banks(mk)
    errors = []
    for asm in sorted(Path(objdir).rglob("*.asm")):
        mod = asm.relative_to(objdir).with_suffix("").as_posix()
        if mod == "far_stubs":
            continue
        mine = banks.get(mod, 0)
        for ln, line in enumerate(asm.read_text(encoding="utf-8", errors="ignore").splitlines(), 1):
            m = INSN_RE.match(line.split(";")[0])
            if not m:
                continue
            for ref in REF_RE.findall(m.group(2)):
                if ref not in syms:
                    continue
                r = region(syms[ref])
                if r and r != mine:
                    where = "カーネル" if mine == 0 else f"バンク{mine}"
                    errors.append(f"{mod}.c ({where}) が バンク{r} の {ref[1:]} を直接参照している"
                                  f"（{asm.name}:{ln}）。FAR で定義して中継を通す")
    if errors:
        sys.exit("check_banks:\n  " + "\n  ".join(sorted(set(errors))))
    print(f"check_banks: OK（{len(banks)} ファイルがコードバンク）")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
