proc test_upvar_append {varName} {
    upvar 1 $varName v
    append v { world}
}
set x {hello}
test_upvar_append x
puts $x
