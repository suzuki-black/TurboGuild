# check_msx2.tcl — turboR 以外の機種で「turboR 専用」を出して止まることを確認する。
# テキスト画面なので get_screen で文字を読む。
set ::log [open $::env(OUT) w]
proc done {} {
    puts $::log "MSXVER=[debug read memory 0x002D]"
    puts $::log "SCREEN:"
    puts $::log [get_screen]
    screenshot -raw [file join $::env(SHOTDIR) msx2.png]
    close $::log
    exit
}
after time 15 done
