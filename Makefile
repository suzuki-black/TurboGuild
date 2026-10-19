# TurboGuild — ASCII8 メガROM のビルド定義
#
#   make          bin/TurboGuild.rom と bin/save.dsk
#   make test     core/ の PC 単体テスト (gcc)
#   make run      openMSX (FS-A1ST, FDD あり) で起動
#   make check    ヘッドレスで起動しスクリーンショットと自己診断を取る
#   make check-nodisk  ディスク無しでも起動して遊べることを確認
#   make check-msx2  MSX2 機種で「turboR 専用」表示を確認
#   make play     30 分の自動プレイ（Phase1 の受け入れ）。DURATION=秒 で短くできる
#   make sim      経済シミュレーション（Phase3 の受け入れ）
#   make clean
.DELETE_ON_ERROR:

CC       := sdcc
AS       := sdasz80
PYTHON   := python3
OPENMSX  := openmsx
MACHINE  ?= Panasonic_FS-A1ST
MACHINE_MSX2 ?= C-BIOS_MSX2_JP

B        := build
ROM      := bin/TurboGuild.rom
DSK      := bin/save.dsk
IHX      := $(B)/kernel.ihx
BANKSMK  := $(B)/banks.mk

# ソース→コードバンクの対応は docs/MEMORY_MAP.md が唯一の定義。そこから banks.mk を作って読む
-include $(BANKSMK)

# --disable-warning 158: SDCC 4.6 は「定数 + 定数」が 127 を超えて u8 の引数に渡ると、
#   値は正しく渡すのに桁あふれ警告を出す（124+8 → 0x84 を確認済み）。誤検知なので止める
# --disable-warning 110: 「最適化で条件の流れを変えた」の通知。a ? 定数 : 定数 などで毎回出るだけ
CFLAGS   := -mz80 --std-c99 --sdcccall 1 --opt-code-size --disable-warning 158 --disable-warning 110 -Isrc
LDFLAGS  := -mz80 --no-std-crt0 --code-loc 0x4010 --data-loc 0xC000 \
            $(foreach n,$(CODE_BANKS),-Wl-b_BANK$(n)=0x$(shell printf %X $(n))6010)

CSRC     := $(wildcard src/*.c src/sys/*.c src/core/*.c src/ui/*.c)
ASRC     := $(filter-out src/sys/crt0.s,$(wildcard src/sys/*.s))
GENH     := src/data/cards.h src/data/rompages.h src/data/screens.h src/data/tables.h src/data/text_ids.h \
            src/data/art.h src/data/palette.h src/data/bgm_data.h
HDRS     := $(wildcard src/*.h src/*/*.h) $(GENH)
COBJ     := $(patsubst src/%.c,$(B)/obj/%.rel,$(CSRC))
AOBJ     := $(patsubst src/%.s,$(B)/obj/%.rel,$(ASRC)) $(B)/obj/far_stubs.rel
CRT0     := $(B)/obj/sys/crt0.rel
DATABIN  := $(wildcard $(B)/*.bin $(B)/*.dat)
DATASRC  := $(wildcard data/*.csv data/text/*.txt)

.PHONY: all test run check check-nodisk check-msx2 check-offline check-timeline play sim clean

all: $(ROM) $(DSK)

# 1 回の実行で複数のファイルを作るものは、代表 1 つを本当の目標にして残りはそれに従わせる
# （macOS の make 3.81 は &: を知らない）
$(B)/data.stamp: tools/build_data.py tools/gen_stages.py $(DATASRC)
	@mkdir -p $(B)
	$(PYTHON) tools/build_data.py
	@touch $@
src/data/cards.h src/data/tables.h src/data/text_ids.h: $(B)/data.stamp
	@:

ARTSRC   := $(wildcard tools/art/*.py) tools/gen_art.py data/card_art.csv data/cards.csv
$(B)/art.stamp: $(ARTSRC)
	@mkdir -p $(B)
	$(PYTHON) tools/gen_art.py --preview
	@touch $@
src/data/art.h src/data/palette.h: $(B)/art.stamp
	@:

src/data/bgm_data.h: tools/mml.py $(wildcard data/bgm/*.mml)
	$(PYTHON) tools/mml.py $@ data/bgm/*.mml

$(BANKSMK): docs/MEMORY_MAP.md tools/link_rom.py
	$(PYTHON) tools/link_rom.py header src/data/rompages.h $(BANKSMK)
src/data/rompages.h: $(BANKSMK)
	@:

$(B)/far_stubs.s: $(BANKSMK) tools/gen_far.py $(CSRC)
	$(PYTHON) tools/gen_far.py $(BANKSMK) $(B)/far_stubs.s src/data/screens.h
src/data/screens.h: $(B)/far_stubs.s
	@:

$(B)/obj/%.rel: src/%.c $(HDRS) Makefile $(BANKSMK)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(if $(BANK_OF_$<),--codeseg BANK$(BANK_OF_$<) --constseg BANK$(BANK_OF_$<)) -c -o $@ $<

$(B)/obj/%.rel: src/%.s
	@mkdir -p $(dir $@)
	$(AS) -g -o $@ $<

$(B)/obj/far_stubs.rel: $(B)/far_stubs.s
	@mkdir -p $(dir $@)
	$(AS) -g -o $@ $<

# crt0 を必ず先頭に置く（_HEADER が 0x4000、領域の並びも crt0 が決める）
$(IHX): $(CRT0) $(AOBJ) $(COBJ)
	$(CC) $(LDFLAGS) -o $@ $(CRT0) $(AOBJ) $(COBJ)

$(ROM): $(IHX) $(B)/data.stamp $(B)/art.stamp docs/MEMORY_MAP.md tools/link_rom.py tools/ram_used.py tools/check_banks.py
	$(PYTHON) tools/check_banks.py $(B)/kernel.map $(BANKSMK) $(B)/obj
	$(PYTHON) tools/link_rom.py rom $(IHX) $@
	@$(PYTHON) tools/ram_used.py $(B)/kernel.map

$(DSK): tools/mkblank_dsk.py
	@if [ ! -f $@ ]; then $(PYTHON) tools/mkblank_dsk.py $@; fi

# 自動試験（Tcl）が使う SaveData の欄の位置
$(B)/save_offsets.tcl: tests/save_offsets.c src/core/save.h src/config.h src/types.h
	@mkdir -p $(B)
	gcc -std=c99 -Isrc -o $(B)/save_offsets tests/save_offsets.c
	$(B)/save_offsets > $@

# ---- 経済シミュレーション（Phase3 の受け入れ: 課金しない人の SSR が終了時に 3〜8 枚） ----
sim: $(B)/data.stamp
	$(PYTHON) tools/sim_economy.py --csv $(B)/sim_economy.csv

# ---- PC 単体テスト ----
TESTS := $(wildcard tests/test_*.c)
test: $(B)/data.stamp
	@mkdir -p $(B)/tests
	@set -e; for t in $(TESTS); do \
	  n=$$(basename $$t .c); \
	  gcc -std=c99 -Wall -Wextra -Werror -Isrc -o $(B)/tests/$$n $$t src/core/*.c tests/tables_pc.c; \
	  $(B)/tests/$$n; \
	done

# ---- openMSX ----
run: all
	$(OPENMSX) -machine $(MACHINE) -carta $(ROM) -romtype ASCII8 -diska $(DSK)

# 後始末はこの ROM のパスで絞る（pkill -f openmsx は他プロジェクトを巻き込む）
KILL_MINE = for p in `ps -eo pid,args | grep "[o]penmsx" | grep -F "$(abspath $(B))/run" | awk '{print $$1}'`; do kill -9 $$p 2>/dev/null || true; done

check: all $(B)/save_offsets.tcl
	@rm -rf $(B)/run && mkdir -p $(B)/run
	@cp $(ROM) $(B)/run/TurboGuild.rom
	@cp $(DSK) $(B)/run/save.dsk
	-OUT=$(abspath $(B))/run/check.txt SHOTDIR=$(abspath $(B))/run timeout 170 \
	  $(OPENMSX) -machine $(MACHINE) -carta $(abspath $(B))/run/TurboGuild.rom -romtype ASCII8 \
	  -diska $(abspath $(B))/run/save.dsk -script $(abspath openmsx/check.tcl) >/dev/null 2>&1
	@$(KILL_MINE)
	@cat $(B)/run/check.txt

check-nodisk: all $(B)/save_offsets.tcl
	@rm -rf $(B)/run3 && mkdir -p $(B)/run3
	@cp $(ROM) $(B)/run3/TurboGuild.rom
	-NODISK=1 OUT=$(abspath $(B))/run3/check.txt SHOTDIR=$(abspath $(B))/run3 timeout 90 \
	  $(OPENMSX) -machine $(MACHINE) -carta $(abspath $(B))/run3/TurboGuild.rom -romtype ASCII8 \
	  -script $(abspath openmsx/check.tcl) >/dev/null 2>&1
	@for p in `ps -eo pid,args | grep "[o]penmsx" | grep -F "$(abspath $(B))/run3" | awk '{print $$1}'`; do kill -9 $$p 2>/dev/null || true; done
	@cat $(B)/run3/check.txt

# Phase1 の受け入れ: 30 分（エミュレータ時間）自動で遊び、保存・リセット・読み込みを繰り返す
DURATION ?= 1800
play: all $(B)/save_offsets.tcl
	@rm -rf $(B)/run4 && mkdir -p $(B)/run4
	@cp $(ROM) $(B)/run4/TurboGuild.rom
	@$(PYTHON) tools/mkblank_dsk.py $(B)/run4/save.dsk >/dev/null
	-OUT=$(abspath $(B))/run4/play.txt MAP=$(abspath $(B))/kernel.map DURATION=$(DURATION) $(if $(TRACE),TRACE=1) timeout 1500 \
	  $(OPENMSX) -machine $(MACHINE) -carta $(abspath $(B))/run4/TurboGuild.rom -romtype ASCII8 \
	  -diska $(abspath $(B))/run4/save.dsk -script $(abspath openmsx/play.tcl) >/dev/null 2>&1
	@for p in `ps -eo pid,args | grep "[o]penmsx" | grep -F "$(abspath $(B))/run4" | awk '{print $$1}'`; do kill -9 $$p 2>/dev/null || true; done
	@cat $(B)/run4/play.txt
	@tail -1 $(B)/run4/play.txt | grep -q "RESULT PASS"

# Phase3 の受け入れ: 1 週間放置して起動しても 3 秒以内にマイページへ
check-offline: all $(B)/save_offsets.tcl
	@rm -rf $(B)/run5 && mkdir -p $(B)/run5
	@cp $(ROM) $(B)/run5/TurboGuild.rom
	@$(PYTHON) tools/mkblank_dsk.py $(B)/run5/save.dsk >/dev/null
	-OUT=$(abspath $(B))/run5/offline.txt MAP=$(abspath $(B))/kernel.map timeout 400 \
	  $(OPENMSX) -machine $(MACHINE) -carta $(abspath $(B))/run5/TurboGuild.rom -romtype ASCII8 \
	  -diska $(abspath $(B))/run5/save.dsk -script $(abspath openmsx/offline.tcl) >/dev/null 2>&1
	@for p in `ps -eo pid,args | grep "[o]penmsx" | grep -F "$(abspath $(B))/run5" | awk '{print $$1}'`; do kill -9 $$p 2>/dev/null || true; done
	@cat $(B)/run5/offline.txt
	@tail -1 $(B)/run5/offline.txt | grep -q "RESULT PASS"

# Phase3: 運営年表をサービス終了まで通す（日付を進めながらマイページに入り直す）
check-timeline: all $(B)/save_offsets.tcl
	@rm -rf $(B)/run6 && mkdir -p $(B)/run6
	@cp $(ROM) $(B)/run6/TurboGuild.rom
	@$(PYTHON) tools/mkblank_dsk.py $(B)/run6/save.dsk >/dev/null
	-OUT=$(abspath $(B))/run6/timeline.txt MAP=$(abspath $(B))/kernel.map SHOTDIR=$(abspath $(B))/run6 timeout 1000 \
	  $(OPENMSX) -machine $(MACHINE) -carta $(abspath $(B))/run6/TurboGuild.rom -romtype ASCII8 \
	  -diska $(abspath $(B))/run6/save.dsk -script $(abspath openmsx/timeline.tcl) >/dev/null 2>&1
	@for p in `ps -eo pid,args | grep "[o]penmsx" | grep -F "$(abspath $(B))/run6" | awk '{print $$1}'`; do kill -9 $$p 2>/dev/null || true; done
	@cat $(B)/run6/timeline.txt
	@tail -1 $(B)/run6/timeline.txt | grep -q "RESULT PASS"

check-msx2: all
	@rm -rf $(B)/run2 && mkdir -p $(B)/run2
	@cp $(ROM) $(B)/run2/TurboGuild.rom
	-OUT=$(abspath $(B))/run2/check.txt SHOTDIR=$(abspath $(B))/run2 timeout 60 \
	  $(OPENMSX) -machine $(MACHINE_MSX2) -carta $(abspath $(B))/run2/TurboGuild.rom -romtype ASCII8 \
	  -script $(abspath openmsx/check_msx2.tcl) >/dev/null 2>&1
	@for p in `ps -eo pid,args | grep "[o]penmsx" | grep -F "$(abspath $(B))/run2" | awk '{print $$1}'`; do kill -9 $$p 2>/dev/null || true; done
	@cat $(B)/run2/check.txt

clean:
	rm -rf $(B) bin/TurboGuild.rom src/data/*.h
