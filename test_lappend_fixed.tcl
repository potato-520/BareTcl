proc lappend {varName val} {
    upvar 1 $varName v
    if {expr [t_scmp $v {}] == 0} {
        set v $val
    } else {
        set old $v
        set space { }
        set new $old
        append new $space
        append new $val
        set v $new
    }
}
set mylist {a b}
lappend mylist {c}
puts $mylist
