proc lsearch {list pattern} {
    set i 0
    foreach item $list {
        set res $i
        set done 1
        # break
    }
    return $res
}
set mylist {a X c}
set idx [lsearch $mylist {X}]
puts $idx
