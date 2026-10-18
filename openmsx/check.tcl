# check.tcl — Phase0 のヘッドレス確認。起動 → 各試験画面のスクショ → 保存 → リセット → ロード。
# 出力は $OUT（テキスト）と $SHOTDIR/*.png。
# ★throttle は on のまま。off だと screenshot が古いフレームを返す。
# ★after time は相対時間。proc 名か list で登録する（"..." は登録時に評価される）。

set ::log [open $::env(OUT) w]
set ::map [file join [file dirname $::env(OUT)] .. kernel.map]

proc sym {name} {
    set f [open $::map r]
    set txt [read $f]
    close $f
    if {[regexp "\\m(\[0-9A-F\]{8})\\s+$name\\M" $txt -> addr]} { return [expr 0x$addr] }
    error "symbol $name not found"
}
proc rd16 {a} { expr {[debug read memory $a] | ([debug read memory [expr {$a+1}]] << 8)} }
proc rd32 {a} { expr {[rd16 $a] | ([rd16 [expr {$a+2}]] << 16)} }

set ::A_CUR    [sym _g_boot_magic]
set ::A_STAT   [sym _g_save_status]
set ::A_TSTAT  [sym _g_time_status]
set ::A_ADDED  [sym _g_time_added]
set ::A_SAVE   [sym _g_save]

proc logstate {tag} {
    set gt [rd32 [expr {$::A_SAVE + 12}]]
    puts $::log [format "%s himem=%04X status=%d tstat=%d added=%d g_time=%d save_count=%d mode=%s" \
        $tag [rd16 0xFC4A] [debug read memory $::A_STAT] [debug read memory $::A_TSTAT] \
        [rd32 $::A_ADDED] $gt [rd16 [expr {$::A_SAVE + 880}]] [get_screen_mode]]
    flush $::log
}

proc key {row mask} {
    keymatrixdown $row $mask
    after time 0.25 [list keymatrixup $row $mask]
}
proc shot {name} { screenshot -raw [file join $::env(SHOTDIR) $name.png] }

# 手順を 0.6 秒刻みで 1 つずつ進める
set ::steps {}
proc nop {} {}
proc step {args} { lappend ::steps $args }
proc run_steps {} {
    if {[llength $::steps] == 0} return
    set s [lindex $::steps 0]
    set ::steps [lrange $::steps 1 end]
    eval $s
    after time 0.8 run_steps
}

proc wait_ready {then n} {
    if {[rd16 $::A_CUR] == 0x4754 && [debug read memory 0xFEDA] == 0xC9} { after time 1.0 $then ; return }
    if {$n <= 0} { puts $::log [format "TIMEOUT boot magic=%04X stke=%02X pc=%04X" [rd16 $::A_CUR] [debug read memory 0xFEDA] [reg pc]] ; shot timeout ; finish ; return }
    after time 0.5 [list wait_ready $then [expr {$n - 1}]]
}

proc finish {} {
    puts $::log "DONE"
    close $::log
    exit
}

proc phase2 {} {
    logstate BOOT2
    step nop ; step nop
    step key 8 0x40 ; step key 8 0x40     ;# DOWN DOWN -> SAVE/RTC
    step key 8 0x01
    step shot save3
    step logstate LOADED
    step finish
    run_steps
}

proc do_reset {} {
    debug write memory $::A_CUR 0
    debug write memory [expr {$::A_CUR+1}] 0
    puts $::log [format "RESET t=%.1f" [machine_info time]] ; flush $::log
    reset
    after time 1.0 [list wait_ready phase2 60]
}

proc phase_nodisk {} {
    logstate NODISK
    step key 8 0x40 ; step key 8 0x40 ; step key 8 0x01 ; step shot nodisk
    step key 8 0x01 ; step nop ; step nop ; step shot nodisk_save
    step logstate NODISK_SAVE
    step finish
    run_steps
}

proc phase1 {} {
    logstate BOOT1
    step shot menu
    step key 8 0x01 ; step shot bank ; step key 7 0x04
    step key 8 0x40 ; step key 8 0x01 ; step shot cards ; step key 7 0x04
    step key 8 0x40 ; step key 8 0x01 ; step shot save1
    step key 8 0x01 ; step nop ; step nop ; step nop ; step shot save2
    step logstate SAVED
    step nop ; step nop ; step nop ; step nop ; step nop ; step nop ; step nop ; step nop
    step do_reset
    run_steps
}

if {[info exists ::env(NODISK)]} {
    after time 1.0 [list wait_ready phase_nodisk 80]
} else {
    after time 1.0 [list wait_ready phase1 80]
}
after time 150 {puts $::log "TIMEOUT total" ; finish}
