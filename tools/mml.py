#!/usr/bin/env python3
"""BGM の MML（data/bgm/*.mml）を FM 音源（OPLL）ドライバ用のバイト列にして C の配列で出す。

    mml.py <out.h> <曲.mml>...

書き方（1 行 = 1 チャンネル。「A:」「B:」「C:」で始める。# から行末はコメント）:
    T6      16 分音符のフレーム数（60 フレーム = 1 秒）
    @n      音色（OPLL の内蔵音色 1..15）      vN  音量（0 = 最大, 15 = 無音）
    oN      オクターブ（1..7）  > <  1 つ上 / 下
    c d e f g a b  音（後に + か # で半音上、- で半音下）  後に長さ 1 2 4 8 16（. で付点）
    r       休み                 lN  省略したときの長さ
    L       ここに戻ってループ（無ければ曲の終わりで止まる）

出力の形: 曲ごとに「チャンネル数, 各チャンネルの位置(u16)…」の後にチャンネルの列。
列のバイト: 00-7F = 音（番号 = オクターブ×12+音名）+ 長さ(フレーム), 80 = 休み + 長さ,
            81 = 音色, 82 = 音量, FE = ループ位置, FF = 終わり
"""
import re
import sys
from pathlib import Path

NOTE = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}


def die(msg):
    sys.exit(f"mml: {msg}")


def compile_channel(text, where):
    out = bytearray()
    unit, octave, deflen = 6, 4, 4
    i = 0
    s = re.sub(r"\s+", "", text)

    def number(j):
        m = re.match(r"\d+", s[j:])
        return (int(m.group()), j + len(m.group())) if m else (None, j)

    def length(j):
        n, j = number(j)
        n = n or deflen
        frames = unit * 16 // n
        if j < len(s) and s[j] == ".":
            frames += frames // 2
            j += 1
        if not 1 <= frames <= 255:
            die(f"{where}: 長さ {frames} フレームは扱えない")
        return frames, j

    while i < len(s):
        raw = s[i]
        c = raw.lower()
        i += 1
        if raw == "L":                      # ループ位置（大文字の L だけ。小文字の l は長さ）
            out.append(0xFE)
            continue
        if c in NOTE:
            n = NOTE[c]
            while i < len(s) and s[i] in "+#-":
                n += -1 if s[i] == "-" else 1
                i += 1
            frames, i = length(i)
            num = octave * 12 + n
            if not 0 <= num < 128:
                die(f"{where}: 音域の外")
            out += bytes([num, frames])
        elif c == "r":
            frames, i = length(i)
            out += bytes([0x80, frames])
        elif c == "t":
            unit, i = number(i)
        elif c == "l":
            deflen, i = number(i)
        elif c == "o":
            octave, i = number(i)
        elif c == ">":
            octave += 1
        elif c == "<":
            octave -= 1
        elif c == "@":
            v, i = number(i)
            out += bytes([0x81, v])
        elif c == "v":
            v, i = number(i)
            out += bytes([0x82, v])
        else:
            die(f"{where}: 知らない文字 '{c}'")
    out.append(0xFF)
    return bytes(out)


def compile_song(path):
    chans = []
    for ln, line in enumerate(Path(path).read_text(encoding="utf-8").splitlines(), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        m = re.match(r"([A-I]):(.*)", line)
        if not m:
            die(f"{path}:{ln}: 行は A: 〜 I: で始める")
        idx = ord(m.group(1)) - ord("A")
        while len(chans) <= idx:
            chans.append("")
        chans[idx] += m.group(2)
    bodies = [compile_channel(c, f"{path}") for c in chans]
    head = 1 + 2 * len(bodies)
    out = bytearray([len(bodies)])
    pos = head
    for b in bodies:
        out += bytes([pos & 255, pos >> 8])
        pos += len(b)
    for b in bodies:
        out += b
    return bytes(out)


def main(out, files):
    lines = ["/* bgm_data.h — tools/mml.py の生成物（data/bgm/ の .mml）。手で編集しない */",
             "#ifndef DATA_BGM_H", "#define DATA_BGM_H"]
    names = []
    for k, f in enumerate(files, 1):
        name = re.sub(r"^\d+", "", Path(f).stem).upper()   # 先頭の数字は並び順のためだけ
        data = compile_song(f)
        names.append(name)
        lines.append(f"#define BGM_{name} {k}")
        lines.append(f"#ifdef BGM_DATA_HERE")
        lines.append(f"static const u8 song_{name.lower()}[{len(data)}] = {{ {', '.join(str(b) for b in data)} }};")
        lines.append("#endif")
    lines.append(f"#define BGM_COUNT {len(files)}")
    lines += ["#ifdef BGM_DATA_HERE", "static const u8 * const songs[BGM_COUNT] = { " +
              ", ".join(f"song_{n.lower()}" for n in names) + " };", "#endif", "#endif", ""]
    text = "\n".join(lines)
    p = Path(out)
    if not p.exists() or p.read_text(encoding="utf-8") != text:
        p.write_text(text, encoding="utf-8")
    print(f"mml: {len(files)} 曲")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    main(sys.argv[1], sorted(sys.argv[2:]))
