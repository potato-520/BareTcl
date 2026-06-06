proc loop_rec {n} {
    if {expr $n == 0} { return 1 }
    set count 0
    set i 0
    while {expr $i < 2} {
        set sub [loop_rec [expr $n - 1]]
        set count [expr $count + $sub]
        set i [expr $i + 1]
    }
    return $count
}
puts [loop_rec 10]
