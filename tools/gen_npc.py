#!/usr/bin/env python3
"""NPC 1000 人の初期状態を作る（仕様 §4.6）。build_data.py から呼ばれる。

1 人 16 バイト: name_id u16, guild u8, level u8, power u16（総戦力/10）, last_login u32,
event_pt u32（イベント終了時の最終ポイント）, activity u8, flags u8。
並びは event_pt の大きい順（順位は二分探索で数える）。総戦力の順位用に、総戦力の大きい順の添字表も作る。
名前は「前半 20 × 後半 50」のハンドル名。乱数は種固定で毎回同じものができる。
"""
import random
import struct

COUNT = 1000
FMT = "<HBBHIIBB"


def generate(n_pre, n_suf, n_guild, seed=20111011):
    rnd = random.Random(seed)
    names = list(range(n_pre * n_suf))
    rnd.shuffle(names)
    npcs = []
    for i in range(COUNT):
        activity = min(255, int(rnd.paretovariate(1.6) * 24))
        level = max(1, min(150, int(activity / 3 + rnd.gauss(20, 8))))
        power = max(50, int(level * rnd.uniform(60, 140) + activity * 30))
        # 一部の「廃人」はとても強い（当時のランキング上位の雰囲気）
        if rnd.random() < 0.02:
            power *= rnd.randint(3, 8)
        event_pt = int(activity ** 1.6 * rnd.uniform(5, 15)) if rnd.random() < 0.85 else 0
        npcs.append([names[i], rnd.randrange(n_guild), level, min(65535, power // 10), 0,
                     event_pt, activity, 0])
    npcs.sort(key=lambda r: -r[5])
    for k, r in enumerate(npcs[:10]):
        r[7] |= 2                                        # bit1: ランカー
    by_power = sorted(range(COUNT), key=lambda i: -npcs[i][3])
    return npcs, by_power


def pack(npcs):
    return b"".join(struct.pack(FMT, *r) for r in npcs)
