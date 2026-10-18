#!/usr/bin/env python3
"""FAR(...) と SCREEN(...) の印を集めて、カーネル側の中継と画面表を生成する（DECISIONS #14）。

    gen_far.py <banks.mk> <out.s> <out_screens.h>

banks.mk（link_rom.py header が作る）で「ソース → バンク」を知る。
FAR 関数の引数がレジスタで渡らない組み合わせならエラーにする（スタック引数はトランポリンで崩れる）。
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FAR_RE = re.compile(r"^FAR\(\s*([^,]+?)\s*,\s*(\w+)\s*,\s*\((.*?)\)\s*\)", re.M)
SCR_RE = re.compile(r"^SCREEN\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*\)", re.M)
WIDE = ("u16", "s16", "*")
NARROW = ("u8", "s8", "char")


def die(msg):
    sys.exit(f"gen_far: {msg}")


def load_banks(path):
    banks = {}
    for line in Path(path).read_text().splitlines():
        m = re.match(r"BANK_OF_(\S+)\s*:=\s*(\d+)", line)
        if m:
            banks[m.group(1)] = int(m.group(2))
    return banks


def arg_width(a):
    a = a.strip()
    if "*" in a:
        return 2
    t = a.split()[0] if a.split() else ""
    if a.startswith("const "):
        t = a.split()[1]
    if t in ("u16", "s16"):
        return 2
    if t in NARROW:
        return 1
    return 0


def check_args(where, name, args):
    args = args.strip()
    if args in ("", "void"):
        return
    parts = [p for p in args.split(",")]
    widths = [arg_width(p) for p in parts]
    if 0 in widths or len(parts) > 2 or (len(parts) == 2 and widths[0] != widths[1]):
        die(f"{where}: {name}({args}) はレジスタで渡らない。引数は u8 か 16bit を 2 個まで、同じ幅で揃える")


def main(banks_mk, out_s, out_h):
    banks = load_banks(banks_mk)
    fars, screens = [], []
    for src, bank in sorted(banks.items()):
        text = (ROOT / src).read_text(encoding="utf-8")
        for ret, name, args in FAR_RE.findall(text):
            check_args(src, name, args)
            fars.append((name, bank, src))
        for m in SCR_RE.findall(text):
            screens.append(m)
    for src in ROOT.glob("src/**/*.c"):
        rel = str(src.relative_to(ROOT))
        if rel not in banks and (FAR_RE.search(src.read_text(encoding="utf-8"))):
            die(f"{rel}: FAR はコードバンクのソースにだけ書ける（MEMORY_MAP に載っていない）")
    names = [f[0] for f in fars]
    dup = {n for n in names if names.count(n) > 1}
    if dup:
        die(f"FAR 関数名が重複: {sorted(dup)}")
    known = set(names)
    s = ["; far_stubs.s — tools/gen_far.py の生成物。手で編集しない", "\t.module\tfar_stubs",
         "\t.globl\tfar_tramp", "\t.area\t_CODE", ""]
    for name, bank, src in fars:
        s += [f"\t.globl\t_{name}", f"\t.globl\t_{name}_b",
              f"_{name}:\t\t\t; {src}", "\tcall\tfar_tramp", f"\t.dw\t_{name}_b", f"\t.db\t{bank}", ""]
    for var, *fns in screens:
        for fn in fns:
            if fn != "0" and fn not in known:
                die(f"SCREEN({var}) の {fn} が FAR で定義されていない")
        s += [f"\t.globl\t_{var}", f"_{var}:"]
        s += [f"\t.dw\t{('_' + fn) if fn != '0' else '0'}" for fn in fns]
        s.append("")
    Path(out_s).write_text("\n".join(s) + "\n", encoding="utf-8")
    h = ["/* screens.h — tools/gen_far.py の生成物。手で編集しない */",
         "#ifndef DATA_SCREENS_H", "#define DATA_SCREENS_H", '#include "../ui/screen.h"']
    h += [f"extern const Screen {var};" for var, *_ in screens]
    h += ["#endif", ""]
    text = "\n".join(h)
    p = Path(out_h)
    if not p.exists() or p.read_text(encoding="utf-8") != text:
        p.write_text(text, encoding="utf-8")
    print(f"gen_far: 中継 {len(fars)} 本, 画面 {len(screens)} 枚")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
