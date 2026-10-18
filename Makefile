# TurboGuild — ASCII8 メガROM のビルド定義
#
#   make          bin/TurboGuild.rom と bin/save.dsk
#   make test     core/ の PC 単体テスト (gcc)
#   make run      openMSX (FS-A1ST, FDD あり) で起動
#   make check    ヘッドレスで起動しスクリーンショットと自己診断を取る
#   make check-nodisk  ディスク無しでも起動して遊べることを確認
#   make check-msx2  MSX2 機種で「turboR 専用」表示を確認
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

CFLAGS   := -mz80 --std-c99 --sdcccall 1 --opt-code-size -Isrc
LDFLAGS  := -mz80 --no-std-crt0 --code-loc 0x4010 --data-loc 0xC000 -Wl-b_BANK1=0x6010

# 常駐コードバンク 1（窓 6000h）に載せるソース。カーネル 8KB に入らないものをここへ
BANK1_SRC := src/ui/p0menu.c src/ui/p0tests.c src/ui/p0save.c src/ui/savedisk.c

CSRC     := $(wildcard src/*.c src/sys/*.c src/core/*.c src/ui/*.c)
ASRC     := $(filter-out src/sys/crt0.s,$(wildcard src/sys/*.s))
GENH     := src/data/cards.h src/data/rompages.h
HDRS     := $(wildcard src/*.h src/*/*.h) $(GENH)
COBJ     := $(patsubst src/%.c,$(B)/obj/%.rel,$(CSRC))
AOBJ     := $(patsubst src/%.s,$(B)/obj/%.rel,$(ASRC))
CRT0     := $(B)/obj/sys/crt0.rel
DATABIN  := $(B)/cards.bin $(B)/text.dat

.PHONY: all test run check check-nodisk check-msx2 clean

all: $(ROM) $(DSK)

$(B)/cards.bin $(B)/text.dat src/data/cards.h &: tools/build_data.py data/cards.csv
	$(PYTHON) tools/build_data.py

src/data/rompages.h: docs/MEMORY_MAP.md tools/link_rom.py
	$(PYTHON) tools/link_rom.py header $@

$(B)/obj/%.rel: src/%.c $(HDRS) Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(if $(filter $<,$(BANK1_SRC)),--codeseg BANK1 --constseg BANK1) -c -o $@ $<

$(B)/obj/%.rel: src/%.s
	@mkdir -p $(dir $@)
	$(AS) -g -o $@ $<

# crt0 を必ず先頭に置く（_HEADER が 0x4000、領域の並びも crt0 が決める）
$(IHX): $(CRT0) $(AOBJ) $(COBJ)
	$(CC) $(LDFLAGS) -o $@ $(CRT0) $(AOBJ) $(COBJ)

$(ROM): $(IHX) $(DATABIN) docs/MEMORY_MAP.md tools/link_rom.py tools/ram_used.py
	$(PYTHON) tools/link_rom.py rom $(IHX) $@
	@$(PYTHON) tools/ram_used.py $(B)/kernel.map

$(DSK): tools/mkblank_dsk.py
	@if [ ! -f $@ ]; then $(PYTHON) tools/mkblank_dsk.py $@; fi

# ---- PC 単体テスト ----
TESTS := $(wildcard tests/test_*.c)
test:
	@mkdir -p $(B)/tests
	@set -e; for t in $(TESTS); do \
	  n=$$(basename $$t .c); \
	  gcc -std=c99 -Wall -Wextra -Werror -Isrc -o $(B)/tests/$$n $$t src/core/*.c; \
	  $(B)/tests/$$n; \
	done

# ---- openMSX ----
run: all
	$(OPENMSX) -machine $(MACHINE) -carta $(ROM) -romtype ASCII8 -diska $(DSK)

# 後始末はこの ROM のパスで絞る（pkill -f openmsx は他プロジェクトを巻き込む）
KILL_MINE = for p in `ps -eo pid,args | grep "[o]penmsx" | grep -F "$(abspath $(B))/run" | awk '{print $$1}'`; do kill -9 $$p 2>/dev/null || true; done

check: all
	@rm -rf $(B)/run && mkdir -p $(B)/run
	@cp $(ROM) $(B)/run/TurboGuild.rom
	@cp $(DSK) $(B)/run/save.dsk
	-OUT=$(abspath $(B))/run/check.txt SHOTDIR=$(abspath $(B))/run timeout 170 \
	  $(OPENMSX) -machine $(MACHINE) -carta $(abspath $(B))/run/TurboGuild.rom -romtype ASCII8 \
	  -diska $(abspath $(B))/run/save.dsk -script $(abspath openmsx/check.tcl) >/dev/null 2>&1
	@$(KILL_MINE)
	@cat $(B)/run/check.txt

check-nodisk: all
	@rm -rf $(B)/run3 && mkdir -p $(B)/run3
	@cp $(ROM) $(B)/run3/TurboGuild.rom
	-NODISK=1 OUT=$(abspath $(B))/run3/check.txt SHOTDIR=$(abspath $(B))/run3 timeout 90 \
	  $(OPENMSX) -machine $(MACHINE) -carta $(abspath $(B))/run3/TurboGuild.rom -romtype ASCII8 \
	  -script $(abspath openmsx/check.tcl) >/dev/null 2>&1
	@for p in `ps -eo pid,args | grep "[o]penmsx" | grep -F "$(abspath $(B))/run3" | awk '{print $$1}'`; do kill -9 $$p 2>/dev/null || true; done
	@cat $(B)/run3/check.txt

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
