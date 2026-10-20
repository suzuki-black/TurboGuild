#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""stages_rule.csv の式から 500 ステージ（50 エリア × 10）を生成する（仕様 §4.2）。

build_data.py から呼ばれる。単体で実行すると先頭と各エリアボスの値を表で出す。
ステージは手で書かない。式の係数を変えたいときは data/stages_rule.csv を直す。
"""
import csv
import sys
from pathlib import Path

STAGES_PER_AREA = 10
RARITY_N, RARITY_R = 0, 1


def load_rule(path):
    with open(path, encoding="utf-8") as f:
        return {r["key"]: float(r["value"]) for r in csv.DictReader(f)}


def generate(rule_path, areas):
    """areas: (name_text, element, enemy_text, boss_card) の列。戻り値は StageRec の列。"""
    k = load_rule(rule_path)
    rows = []
    for a, area in enumerate(areas, 1):
        element = area[1]
        for s in range(1, STAGES_PER_AREA + 1):
            n = (a - 1) * STAGES_PER_AREA + s
            boss = s == STAGES_PER_AREA
            rows.append((
                int(k["ap_base"] + a // k["ap_div"]),
                int(k["hp_base"] * n ** k["hp_exp"]),
                int(k["def_mul"] * n),
                int(k["atk_mul"] * n),
                int(k["exp_mul"] * n),
                int(k["gold_mul"] * n),
                int(k["drop_boss"] if boss else k["drop_normal"]),
                ((RARITY_R if boss else RARITY_N) << 4) | element,
            ))
    return rows


def main():
    root = Path(__file__).resolve().parent.parent
    areas = [(0, 0, 0, 0)] * 50
    rows = generate(root / "data" / "stages_rule.csv", areas)
    print(" no  ap        hp    def    atk   exp  gold drop")
    for n in [1, 2, 9, 10] + list(range(20, 501, 50)) + [500]:
        r = rows[n - 1]
        print(f"{n:3d} {r[0]:3d} {r[1]:9d} {r[2]:6d} {r[3]:6d} {r[4]:5d} {r[5]:5d} {r[6]:3d}%")


if __name__ == "__main__":
    sys.exit(main())
