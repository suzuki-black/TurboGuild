#!/usr/bin/env python3
"""インフレ・ガチャ・報酬の経済シミュレーション（仕様 §10 Phase3 の受け入れ）。

ゲームと同じ src/core/ を gcc で組んだ tests/sim_economy.c を、コース × 乱数の種ぶん走らせる。
各回はサービス開始から終了（270 日）まで、1 日（現実）に PLAYS 回遊ぶ。

    sim_economy.py [--seeds N] [--plays P] [--csv 出力.csv]

受け入れ: 課金しないプレイヤーの終了時 SSR 所持の中央値が 1〜3 枚（DECISIONS #61 で仕様の 3〜8 枚から下げた）。外れたら終了コード 1。
"""
import argparse
import statistics
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BIN = ROOT / "build" / "sim_economy"
COURSES = [(0, "課金しない"), (1, "おこづかい"), (2, "社会人"), (3, "石油王"), (4, "無敵モード")]
TARGET = (1, 3)


def build():
    srcs = [str(p) for p in sorted((ROOT / "src" / "core").glob("*.c"))]
    subprocess.run(["gcc", "-std=c99", "-O2", f"-I{ROOT / 'src'}", "-o", str(BIN),
                    str(ROOT / "tests" / "sim_economy.c"), *srcs, str(ROOT / "tests" / "tables_pc.c")], check=True)


def run(course, plays, seed):
    out = subprocess.run([str(BIN), str(course), str(plays), str(seed)], cwd=ROOT, check=True,
                         capture_output=True, text=True).stdout.strip().splitlines()
    rows = [l.split(",") for l in out[1:]]
    end = rows[-1]
    return {"level": int(end[1]), "stage": int(end[2]), "ssr": int(end[3]), "sr": int(end[4]),
            "spent": int(end[9]), "series": [(int(r[0]), int(r[3]), int(r[7]), int(r[1])) for r in rows[:-1]]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=30)
    ap.add_argument("--plays", type=int, default=4)
    ap.add_argument("--csv")
    a = ap.parse_args()
    build()
    lines = ["course,seed,level,stage,ssr,sr,spent"]
    print(f"{'コース':<10} {'SSR 中央値':>8} {'平均':>5} {'範囲':>9} {'範囲内':>7} {'Lv':>4} {'到達':>5} {'課金(円)':>10}")
    free_median = None
    for course, name in COURSES:
        res = [run(course, a.plays, s) for s in range(1, a.seeds + 1)]
        ssr = [r["ssr"] for r in res]
        inside = sum(TARGET[0] <= v <= TARGET[1] for v in ssr)
        med = statistics.median(ssr)
        if course == 0:
            free_median = med
        print(f"{name:<10} {med:>8} {statistics.mean(ssr):>5.1f} {min(ssr):>4}〜{max(ssr):<3} "
              f"{inside:>3}/{len(ssr):<3} {statistics.median(r['level'] for r in res):>4} "
              f"{statistics.median(r['stage'] for r in res):>5} {statistics.median(r['spent'] for r in res):>10}")
        for s, r in enumerate(res, 1):
            lines.append(f"{course},{s},{r['level']},{r['stage']},{r['ssr']},{r['sr']},{r['spent']}")
    if a.csv:
        Path(a.csv).write_text("\n".join(lines) + "\n", encoding="utf-8")
    ok = TARGET[0] <= free_median <= TARGET[1]
    print(f"受け入れ（課金しない人の SSR 中央値 {TARGET[0]}〜{TARGET[1]} 枚）: {'OK' if ok else 'NG'}（{free_median} 枚）")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
