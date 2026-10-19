# timeline.tcl — Phase3: 運営年表を SERVICE_END まで通す。ゲーム内の日付を進めながらマイページに入り直し、
# イベントの参加・終了、贈り物、終了の告知、サービス終了後のタイトルを確かめる。要所でスクリーンショット。
set throttle off
set ::log [open $::env(OUT) w]
set ::map $::env(MAP)
source [file join [file dirname $::env(OUT)] .. save_offsets.tcl]
proc sym {name} { set f [open $::map r]; set t [read $f]; close $f; regexp "\\m(\[0-9A-F\]{8})\\s+$name\\M" $t -> a; return [expr 0x$a] }
proc rd8 {a} { debug read memory $a }
proc rd16 {a} { expr {[rd8 $a] | ([rd8 [expr {$a+1}]] << 8)} }
proc rd32 {a} { expr {[rd16 $a] | ([rd16 [expr {$a+2}]] << 16)} }
proc wr32 {a v} { for {set i 0} {$i < 4} {incr i} { debug write memory [expr {$a + $i}] [expr {($v >> ($i * 8)) & 255}] } }
proc say {m} { puts $::log [format "%7.2f %s" [machine_info time] $m]; flush $::log }
set ::SAVE [sym _g_save] ; set ::CUR [sym _g_screen_cur] ; set ::ST [sym _g_ui_state]
set ::KEYS [sym _g_test_keys] ; set ::DG [sym _g_test_digit] ; set ::MAGIC [sym _g_boot_magic]
proc sv {k} { expr {$::SAVE + $::O($k)} }
proc at {n} { expr {[rd16 $::CUR] == [sym _scr_$n]} }
proc ready {} { expr {[rd16 $::MAGIC] == 0x4754 && [rd8 $::ST] != 0} }
proc press {b} { debug write memory $::KEYS $b }
proc digit {d} { debug write memory $::DG [expr {$d + 1}] }
# throttle off のままだと screenshot は古いフレームを返す。撮るときだけ on にして 0.1 秒描かせる
proc shot {n} { if {[info exists ::env(SHOTDIR)]} { set ::throttle on ; after time 0.1 [list snap $n] } }
proc snap {n} { screenshot -raw [file join $::env(SHOTDIR) $n.png] ; set ::throttle off }
set ::days {3 10 17 20 36 45 60 70 95 110 120 150 165 185 200 245 271}
set ::di 0 ; set ::phase start ; set ::fails 0 ; set ::ends {} ; set ::joins {}
proc fail {m} { incr ::fails ; say "FAIL $m" }
proc done {} {
    say "events ended: $::ends  joined: $::joins"
    if {$::ends ne "1 2 3 4 5 6 7"} { fail "not every event ended: $::ends" }
    say [expr {$::fails ? "RESULT FAIL" : "RESULT PASS"}] ; close $::log ; exit
}
proc tick {} {
    if {![ready]} { after time 0.05 tick ; return }
    switch $::phase {
        start { if {[at home]} { set ::phase next } else { press 16 } }
        next {
            if {$::di >= [llength $::days]} { done ; return }
            set d [lindex $::days $::di] ; incr ::di
            set ::gems_before [rd32 [sv gems]] ; set ::le_before [rd16 [sv last_event]]
            wr32 [sv gtime] [expr {$d * 86400 + 600}]
            set ::day $d ; set ::phase leave ; digit 6          ;# ランキングへ行って戻る = マイページに入り直す
        }
        leave { if {[at ranking]} { press 32 ; set ::phase back } elseif {[at home]} { digit 6 } else { press 16 } }
        back {
            if {[at title]} {
                say "day $::day: title (service ended)" ; shot tl_end_title
                if {$::day < 270} { fail "title before the end" }
                after time 0.5 done ; return
            }
            if {[at evresult] || [at login]} { shot [format "tl_d%03d_%s" $::day [expr {[at evresult] ? "ev" : "login"}]] ; press 16 ; after time 0.3 tick ; return }
            if {[rd8 $::ST] == 2} { shot [format "tl_d%03d_msg" $::day] ; press 16 ; after time 0.3 tick ; return }
            if {[at event] || [at ranking]} { press 32 ; after time 0.3 tick ; return }
            if {![at home]} { press 16 ; after time 0.3 tick ; return }
            if {[rd8 $::ST] != 1} { after time 0.1 tick ; return }
            set le [rd16 [sv last_event]] ; set ev [rd16 [sv event_id]] ; set g [rd32 [sv gems]]
            if {$le != $::le_before} { lappend ::ends $le }
            if {$ev && [lsearch $::joins $ev] < 0} { lappend ::joins $ev }
            say "day $::day: event_id=$ev last_event=$le gems=$g ([expr {$g - $::gems_before}])"
            if {$::day == 110 && $g - $::gems_before < 500} { fail "gift not received" }
            if {[lsearch {3 110 245} $::day] >= 0} { after time 1.5 [list shot [format "tl_d%03d_home" $::day]] }
            set ::phase next
            after time 2 tick ; return
        }
    }
    after time 0.1 tick
}
after time 1 tick
after time 900 { fail "timeout" ; done }
