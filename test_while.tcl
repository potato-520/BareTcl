set mylist {a b c}
set i 0
set len [llength $mylist]
while {expr $i < $len} {
    puts [lindex $mylist $i]
    set i [expr $i + 1]
}
