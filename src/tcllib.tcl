# tcllib.tcl - BareTcl Bootstrap Library

proc abs {x} { if {expr $x < 0} { return [expr 0 - $x] }; return $x }

proc incr {varName} {
    upvar 1 $varName v
    set v [expr $v + 1]
}

proc for {start cond next body} {
    uplevel 1 $start
    while {uplevel 1 $cond} {
        set res [catch {uplevel 1 $body} err]
        if {expr $res == 3} { break }
        if {expr $res == 4} { uplevel 1 $next; continue }
        if {expr $res == 1} { error $err }
        if {expr $res == 2} { return $err }
        uplevel 1 $next
    }
}

proc foreach {var list body} {
    set len [llength $list]
    set i 0
    while {expr $i < $len} {
        uplevel 1 [list set $var [lindex $list $i]]
        set res [catch {uplevel 1 $body} err]
        if {expr $res == 3} { break }
        if {expr $res == 4} { incr i; continue }
        if {expr $res == 1} { error $err }
        if {expr $res == 2} { return $err }
        incr i
    }
}

proc hanoi {n from to aux} {
    if {expr $n == 1} { puts {Move disk 1}; return 0 }
    set n1 [expr $n - 1]
    hanoi $n1 $from $aux $to
    puts {Move disk N}
    hanoi $n1 $aux $to $from
    return 0
}
