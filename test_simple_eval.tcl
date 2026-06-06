proc fib {n} {
    if {expr $n < 2} { return $n }
    return [expr [fib [expr $n - 1]] + [fib [expr $n - 2]]]
}
set f10 [fib 10]
puts $f10
