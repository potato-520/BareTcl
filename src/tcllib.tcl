# tcllib.tcl - BareTcl Bootstrap Library

# --- Basic Math ---
proc abs {x} { if {expr $x < 0} { return [expr 0 - $x] }; return $x }

# --- Variable & Flow Control ---
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
        if {expr $res == 4} { set i [expr $i + 1]; continue }
        if {expr $res == 1} { error $err }
        if {expr $res == 2} { return $err }
        set i [expr $i + 1]
    }
}

# --- List Operations ---
proc lappend {varName val} {
    upvar 1 $varName v
    set tmp $v
    set cond [t_scmp $tmp {}]
    if {expr $cond != 0} {
        append tmp { }
        append tmp $val
        set v $tmp
    } else {
        set v $val
    }
    return $v
}

proc lset {varName index val} {
    upvar 1 $varName list
    set pre [lrange $list 0 [expr $index - 1]]
    set post [lrange $list [expr $index + 1] end]
    set res $pre
    set cond [llength $res]
    if {expr $cond > 0} { append res { } }
    append res $val
    set cond_p [llength $post]
    if {expr $cond_p > 0} { append res { }; append res $post }
    set list $res
}

proc lsearch {list pattern} {
    set i 0
    set len [llength $list]
    while {expr $i < $len} {
        set item [lindex $list $i]
        if {expr [t_scmp $item $pattern] == 0} { return $i }
        set i [expr $i + 1]
    }
    return -1
}

# --- String Operations ---
proc string {subcmd args} {
    set str [lindex $args 0]
    if {expr [t_scmp $subcmd {length}] == 0} {
        return [llength $str] 
    }
    if {expr [t_scmp $subcmd {index}] == 0} {
        return [lindex $str [lindex $args 1]]
    }
    if {expr [t_scmp $subcmd {range}] == 0} {
        return [lrange $str [lindex $args 1] [lindex $args 2]]
    }
    error [list unknown string subcommand $subcmd]
}

# --- Format (Basic implementation) ---
proc format {fmt args} {
    set res {}
    set arg_idx 0
    set len [llength $fmt]
    set i 0
    while {expr $i < $len} {
        set part [lindex $fmt $i]
        set c1 [t_scmp $part {%s}]
        set c2 [t_scmp $part {%d}]
        if {expr $c1 == 0} {
            append res [lindex $args $arg_idx]
            set arg_idx [expr $arg_idx + 1]
        } else if {expr $c2 == 0} {
            append res [lindex $args $arg_idx]
            set arg_idx [expr $arg_idx + 1]
        } else {
            append res $part
        }
        append res { }
        set i [expr $i + 1]
    }
    return $res
}
