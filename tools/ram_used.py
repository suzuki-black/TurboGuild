#!/usr/bin/env python3
"""リンクマップから静的 RAM（_DATA/_INITIALIZED/_BSS）の合計を出し、予算超過で失敗させる（仕様 §11）。"""
import re
import sys

BUDGET = 6144   # 実測 HIMEM=DE67h で C000h-HIMEM は 7,783 B。残りはスタック（DECISIONS #1）


def main(path):
    total = 0
    for line in open(path, encoding="utf-8", errors="ignore"):
        m = re.match(r"^(_DATA|_INITIALIZED|_BSS)\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})", line)
        if m:
            total += int(m.group(3), 16)
    print(f"RAM used: {total} / {BUDGET}")
    if total > BUDGET:
        sys.exit("ram_used: RAM 予算超過")


if __name__ == "__main__":
    main(sys.argv[1])
