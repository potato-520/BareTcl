proc fib {n} {
    if {expr $n < 2} { return $n }
    return [expr [fib [expr $n - 1]] + [fib [expr $n - 2]]]
}
puts [fib 5]
