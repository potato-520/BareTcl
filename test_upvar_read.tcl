proc test_upvar_read {varName} {
    upvar 1 $varName v
    puts $v
}
set x {hello}
test_upvar_read x
