#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""リンクマップから静的 RAM（_DATA/_INITIALIZED/_BSS）の合計を出し、予算超過で失敗させる（仕様 §11）。
RAM の割り当て: C000h〜 静的 RAM（3,072 B まで）／ CC00h〜D9FFh RAM で実行するコード（hot, 3,584 B）／
DA00h〜HIMEM(DE67h) スタック（1,127 B。実測の最大 378 B の約 3 倍）。"""
import re
import sys

BUDGET = 3072   # 静的 RAM は C000h から CC00h（RAM 実行コードの先頭）まで（DECISIONS #1, #72）


def main(path):
    sizes = {}
    for line in open(path, encoding="utf-8", errors="ignore"):
        m = re.match(r"^(_DATA|_INITIALIZED|_BSS)\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})", line)
        if m:
            sizes[m.group(1)] = int(m.group(3), 16)     # マップには同じ領域が 2 回出るので名前で 1 つに
    total = sum(sizes.values())
    print(f"RAM used: {total} / {BUDGET}")
    if total > BUDGET:
        sys.exit("ram_used: RAM 予算超過")


if __name__ == "__main__":
    main(sys.argv[1])
