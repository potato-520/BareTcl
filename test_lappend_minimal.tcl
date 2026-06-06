proc lappend {varName val} {
    upvar 1 $varName v
    if {expr [t_scmp $v {}] == 0} {
        set v $val
    } else {
        puts "BEFORE APPEND: $v"
        append v { }
        puts "AFTER SPACE: $v"
        append v $val
        puts "AFTER VAL: $v"
    }
}
set mylist {a b}
lappend mylist {c}
puts "FINAL: $mylist"
