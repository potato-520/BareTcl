proc test_upvar {varName} {
    upvar 1 $varName v
    set v {new_value}
}
set x {old_value}
test_upvar x
puts $x
