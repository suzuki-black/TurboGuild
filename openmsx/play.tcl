# play.tcl — Phase1 の受け入れ: 新規開始から 30 分（エミュレータ時間）遊んで落ちないこと、
# 保存 → リセット → 読み込みで所持カードとステージ進行が一致すること。
#
# 画面とUI状態をメモリから読み、人が押すボタンを g_test_keys に 1 フレームぶん書いて操作する。
# 出力は $OUT。最後の行が "RESULT PASS" か "RESULT FAIL ..."。
# ★after time は相対時間。proc 名か list で登録する。

set renderer none
set throttle off
set ::log [open $::env(OUT) w]
set ::map $::env(MAP)
set ::DURATION [expr {[info exists ::env(DURATION)] ? $::env(DURATION) : 1800}]

proc sym {name} {
    set f [open $::map r]; set t [read $f]; close $f
    if {![regexp "\\m(\[0-9A-F\]{8})\\s+$name\\M" $t -> a]} { error "no symbol $name" }
    return [expr 0x$a]
}
proc rd8 {a} { debug read memory $a }
proc rd16 {a} { expr {[rd8 $a] | ([rd8 [expr {$a+1}]] << 8)} }
proc rd32 {a} { expr {[rd16 $a] | ([rd16 [expr {$a+2}]] << 16)} }
proc say {msg} { puts $::log [format "%7.1f %s" [machine_info time] $msg]; flush $::log }

set ::A_MAGIC [sym _g_boot_magic]
set ::A_CUR   [sym _g_screen_cur]
set ::A_STATE [sym _g_ui_state]
set ::A_KEYS  [sym _g_test_keys]
set ::A_CURSOR [sym _g_cursor]
set ::A_SAVE  [sym _g_save]
set ::A_SAVEST [sym _g_save_status]
set ::A_QR    [sym _g_qr]
set ::SCR {}
foreach n {title name firstgacha home area stage gacha gacharesult cardlist carddetail deck deckpick shop p0menu} {
    dict set ::SCR [sym _scr_$n] $n
}
# SaveData のオフセット（src/core/save.h の並び。SDCC は詰めて並べる）
array set ::O {level 36 gold 39 gems 43 ap 47 potions 48 stage 56 cards_n 69 cards 71 deck 871 savecnt 885}
proc sv {k} { expr {$::A_SAVE + $::O($k)} }

set BTN_UP 1 ; set BTN_DOWN 2 ; set BTN_LEFT 4 ; set BTN_RIGHT 8 ; set BTN_OK 16 ; set BTN_CANCEL 32
proc press {b} { debug write memory $::A_KEYS $b ; incr ::presses }

set ::presses 0
set ::quests 0 ; set ::wins 0 ; set ::gachas 0 ; set ::resets 0 ; set ::fails 0
set ::plan_i 0 ; set ::session 0 ; set ::busy_since -1 ; set ::last_action 0
set ::started_play 0 ; set ::leaving 0 ; set ::in_quest 0 ; set ::pending_check 0 ; set ::did_reroll 0
set ::plan {quest quest gacha quest card quest shop save quest gacha quest save}

proc fail {why} {
    incr ::fails
    say "FAIL $why"
    finish
}
proc finish {} {
    say [format "STATS quests=%d wins=%d gachas=%d resets=%d presses=%d level=%d stage=%d cards=%d gold=%d gems=%d" \
        $::quests $::wins $::gachas $::resets $::presses [rd8 [sv level]] [rd16 [sv stage]] [rd16 [sv cards_n]] \
        [rd32 [sv gold]] [rd32 [sv gems]]]
    if {$::fails == 0} { say "RESULT PASS" } else { say "RESULT FAIL" }
    close $::log
    exit
}

# 所持カード・進行の要約（保存直後とリセット後を比べる）
proc snapshot {} {
    set n [rd16 [sv cards_n]]
    set sum 0
    for {set i 0} {$i < $n * 4} {incr i} { set sum [expr {($sum * 31 + [rd8 [expr {[sv cards] + $i}]]) & 0xFFFFFF}] }
    return [list stage [rd16 [sv stage]] cards $n sum $sum level [rd8 [sv level]] gold [rd32 [sv gold]] gems [rd32 [sv gems]]]
}

proc goto_menu {target} {
    set c [rd8 $::A_CURSOR]
    if {$c == $target} { press $::BTN_OK ; return 1 }
    press $::BTN_DOWN
    return 0
}

proc act_home {} {
    set ::leaving 0
    if {$::pending_check} { return [do_reset] }
    set a [lindex $::plan [expr {$::plan_i % [llength $::plan]}]]
    switch $a {
        quest { if {[goto_menu 0]} { incr ::plan_i ; set ::session 0 } }
        gacha { if {[goto_menu 1]} { incr ::plan_i ; set ::session 0 } }
        card  { if {[goto_menu 2]} { incr ::plan_i } }
        shop  { if {[goto_menu 3]} { incr ::plan_i } }
        save  { if {[goto_menu 4]} { incr ::plan_i ; set ::pending_check 1 } }
    }
}

proc do_reset {} {
    set ::pending_check 0
    if {[rd8 $::A_SAVEST] == 3} { say "no disk; skip reset check" ; return }
    set ::snap [snapshot]
    say "SAVE+RESET snapshot $::snap"
    debug write memory $::A_MAGIC 0
    reset
    incr ::resets
    set ::after_reset 1
}

proc check_loaded {} {
    set now [snapshot]
    foreach k {stage cards sum level gold gems} {
        if {[dict get $now $k] != [dict get $::snap $k]} { fail "reload mismatch $k: saved [dict get $::snap $k] loaded [dict get $now $k]" ; return }
    }
    say "reload OK $now"
}

proc act {scr st} {
    if {$st == 2} {                                   ;# メッセージ・確認・結果
        if {$scr eq "stage" && $::in_quest} {
            set ::in_quest 0
            incr ::quests ; incr ::session
            if {[rd8 [expr {$::A_QR + 5}]]} { incr ::wins }    ;# QuestResult.b.win
        }
        if {$scr eq "gacha"} { press $::BTN_OK ; return }     ;# 1回 を選ぶ
        press $::BTN_OK ; return
    }
    switch $scr {
        title {
            if {[info exists ::after_reset] && $::after_reset} { set ::after_reset 0 ; check_loaded }
            if {$::fails} return
            press $::BTN_OK
        }
        name { press $::BTN_OK }
        firstgacha {
            if {!$::did_reroll} { set ::did_reroll 1 ; press $::BTN_OK ; return }   ;# 1 回は引き直す
            if {[rd8 $::A_CURSOR] == 0} { press $::BTN_RIGHT } else { press $::BTN_OK }
        }
        home { act_home }
        area { if {$::leaving} { press $::BTN_CANCEL } else { press $::BTN_OK } }
        stage {
            if {$::leaving || $::session >= 6 || ([rd8 [sv ap]] < 14 && [rd8 [sv potions]] == 0)} {
                set ::leaving 1 ; press $::BTN_CANCEL ; return
            }
            set ::in_quest 1
            press $::BTN_OK
        }
        gacha {
            if {$::session >= 1} { press $::BTN_CANCEL ; return }
            set want [expr {[rd32 [sv gems]] >= 300 ? 2 : 0}]
            if {[rd8 $::A_CURSOR] != $want} { press $::BTN_DOWN ; return }
            incr ::session ; incr ::gachas
            press $::BTN_OK
        }
        gacharesult { press $::BTN_OK }
        cardlist {
            if {$::session == 0} { set ::session 1 ; press $::BTN_OK } else { set ::session 0 ; press $::BTN_CANCEL }
        }
        deck {
            if {$::session == 1} {
                if {[rd8 $::A_CURSOR] != 5} { press $::BTN_UP ; return }
                set ::session 2 ; press $::BTN_OK
            } else { press $::BTN_CANCEL }
        }
        shop {
            if {[rd32 [sv gems]] > 1000 && [rd8 [sv potions]] < 3} { press $::BTN_OK } else { press $::BTN_CANCEL }
        }
        default { press $::BTN_CANCEL }
    }
}

proc tick {} {
    set t [machine_info time]
    if {$t - $::t0 > $::DURATION} { finish ; return }
    if {[rd16 $::A_MAGIC] != 0x4754} { after time 0.2 tick ; return }   ;# 起動中
    set cur [rd16 $::A_CUR]
    if {![dict exists $::SCR $cur]} { fail [format "unknown screen %04X pc=%04X" $cur [reg pc]] ; return }
    set scr [dict get $::SCR $cur]
    set st [rd8 $::A_STATE]
    if {$st == 0} {
        if {$::busy_since < 0} { set ::busy_since $t }
        if {$t - $::busy_since > 30} {
            set stk ""
            for {set i 0} {$i < 24} {incr i 2} { append stk [format " %04X" [rd16 [expr {[reg sp] + $i}]]] }
            fail [format "busy 30s on %s pc=%04X iff=%s jiffy=%04X sp=%04X bank=%02X stack:%s" $scr [reg pc] [reg iff] [rd16 0xFC9E] [reg sp] [rd8 [sym _g_bank_code]] $stk]
            return
        }
        after time 0.05 tick ; return
    }
    set ::busy_since -1
    if {$t - $::last_report > 120} {
        set ::last_report $t
        say [format "progress quests=%d gachas=%d resets=%d level=%d stage=%d cards=%d ap=%d" \
            $::quests $::gachas $::resets [rd8 [sv level]] [rd16 [sv stage]] [rd16 [sv cards_n]] [rd8 [sv ap]]]
    }
    if {$::trace && ($scr ne $::last_scr || $st != $::last_st)} {
        say "  $scr st=$st cursor=[rd8 $::A_CURSOR]"
        set ::last_scr $scr ; set ::last_st $st
    }
    act $scr $st
    after time 0.12 tick
}

set ::t0 0
set ::last_scr "" ; set ::last_st -1
set ::trace [info exists ::env(TRACE)]
set ::last_report 0
proc start {} {
    if {[rd16 $::A_MAGIC] == 0x4754 && [rd8 0xFEDA] == 0xC9} {
        set ::t0 [machine_info time]
        say "boot ok"
        tick
        return
    }
    after time 0.5 start
}
after time 1 start
after time [expr {$::DURATION + 600}] { say "TIMEOUT total" ; finish }
