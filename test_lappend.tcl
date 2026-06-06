proc lappend {varName val} {
    upvar 1 $varName v
    if {expr [t_scmp $v {}] == 0} {
        set v $val
    } else {
        set old $v
        append old { }
        append old $val
        set v $old
    }
    return $v
}
set mylist {a b}
lappend mylist {c}
puts $mylist
