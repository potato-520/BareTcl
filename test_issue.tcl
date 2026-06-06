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
}
set mylist {a b}
lappend mylist {c}
puts $mylist
