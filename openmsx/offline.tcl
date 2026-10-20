# offline.tcl — Phase3 の受け入れ: 「1 週間放置して起動」が 3 秒以内か（エミュレータの時間で測る）。
# 新規開始 → 第 1 回イベントに参加して戦う → 保存 → リセット → 現実 7 日後（ゲーム 70 日後）として読み込み、
#   (1) 電源投入（リセット）からタイトルが操作できるまで
#   (2) 「つづきから」を押してからマイページが操作できるまで（イベントの終了処理・順位計算・年表を含む）
# を測る。出力は $OUT（最後の行が RESULT PASS / FAIL）。
set renderer none
set throttle off
set ::log [open $::env(OUT) w]
set ::map $::env(MAP)
source [file join [file dirname $::env(OUT)] .. save_offsets.tcl]
proc sym {name} { set f [open $::map r]; set t [read $f]; close $f; regexp "\\m(\[0-9A-F\]{8})\\s+$name\\M" $t -> a; return [expr 0x$a] }
proc rd8 {a} { debug read memory $a }
proc rd16 {a} { expr {[rd8 $a] | ([rd8 [expr {$a+1}]] << 8)} }
proc wr32 {a v} { for {set i 0} {$i < 4} {incr i} { debug write memory [expr {$a + $i}] [expr {($v >> ($i * 8)) & 255}] } }
proc say {m} { puts $::log [format "%7.2f %s" [machine_info time] $m]; flush $::log }
set ::SAVE [sym _g_save] ; set ::CUR [sym _g_screen_cur] ; set ::ST [sym _g_ui_state]
set ::KEYS [sym _g_test_keys] ; set ::DG [sym _g_test_digit] ; set ::MAGIC [sym _g_boot_magic]
proc sv {k} { expr {$::SAVE + $::O($k)} }
proc scr {n} { sym _scr_$n }
proc at {n} { expr {[rd16 $::CUR] == [scr $n]} }
proc ready {} { expr {[rd16 $::MAGIC] == 0x4754 && [rd8 $::ST] != 0} }
proc press {b} { debug write memory $::KEYS $b }
proc digit {d} { debug write memory $::DG [expr {$d + 1}] }
set ::phase 0 ; set ::fights 0
proc fail {m} { say "FAIL $m" ; say "RESULT FAIL" ; close $::log ; exit }
proc tick {} {
    if {![ready]} { after time 0.02 tick ; return }
    switch $::phase {
        0 { if {[at home] && [rd8 $::ST] == 1} { wr32 [sv gtime] [expr {3 * 86400 + 3600}] ; set ::phase 1 ; digit 4 } else { press 16 } }
        1 { if {[at deck]} { press 32 } elseif {[at home] && [rd8 $::ST] == 1} { set ::phase 2 ; digit 9 } else { press 16 } }
        2 { if {[at event] && $::fights < 3} { incr ::fights ; digit 1 } elseif {[at event]} { press 32 ; set ::phase 3 } else { press 16 } }
        3 { if {[at home]} { set ::phase 4 ; digit 7 } else { press 16 } }
        4 { if {[at home] && [rd8 $::ST] == 1} {
                say "saved at day 3 event_pt=[rd16 [sv event_pt]] event_id=[rd16 [sv event_id]]"
                debug write memory $::MAGIC 0 ; set ::t_reset [machine_info time] ; reset ; set ::phase 5
            } else { press 16 } }
        5 { if {[at title]} {
                set ::boot [expr {[machine_info time] - $::t_reset}]
                wr32 [sv gtime] [expr {[rd16 [sv gtime]] + 70 * 86400}]          ;# 現実 7 日 = ゲーム 70 日
                set ::t_cont [machine_info time] ; set ::phase 6 ; press 16
            } }
        6 { if {![at title]} {
                set ::cont [expr {[machine_info time] - $::t_cont}]
                say [format "boot %.2fs (power on -> title), continue %.2fs (-> %s usable)" $::boot $::cont [expr {[at evresult] ? "event result" : "home"}]]
                say "after load: last_event=[rd16 [sv last_event]] last_rank=[rd16 [sv last_rank]] event_id=[rd16 [sv event_id]]"
                if {[rd16 [sv last_event]] != 1} { fail "event end not processed" ; return }
                if {$::cont > 3.0} { fail "continue took [format %.2f $::cont]s > 3s" ; return }
                say "RESULT PASS" ; close $::log ; exit
            } }
    }
    after time 0.1 tick
}
after time 1 tick
after time 300 { fail "timeout" }
