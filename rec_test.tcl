proc rec {n} {
    if {expr $n == 0} { return 1 }
    set sub [rec [expr $n - 1]]
    return [expr $sub + 1]
}
puts [rec 100]
