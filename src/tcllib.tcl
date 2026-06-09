# tcllib.tcl - BareTcl 自举脚本库 (Bootstrap Library)

# --- 基础数学函数 ---
# abs: 返回绝对值
proc abs {x} { 
    if {$x < 0} { 
        return [expr 0 - $x] 
    }; 
    return $x 
}

# --- 变量与流程控制 ---
# incr: 变量自增
proc incr {varName} {
    upvar 1 $varName v
    set v [expr $v + 1]
}

# for: 标准 for 循环实现
proc for {start cond next body} {
    uplevel 1 $start
    while {[uplevel 1 [list expr $cond]]} {
        set res [catch {uplevel 1 $body} err]
        if { $res == 3 } { break }        ;# TCL_BREAK
        if { $res == 4 } { uplevel 1 $next; continue } ;# TCL_CONTINUE
        if { $res == 1 } { error $err }   ;# TCL_ERROR
        if { $res == 2 } { return $err }  ;# TCL_RETURN
        uplevel 1 $next
    }
}

# foreach: 标准列表遍历
proc foreach {var list body} {
    set len [llength $list]
    set i 0
    while {$i < $len} {
        uplevel 1 [list set $var [lindex $list $i]]
        set res [catch {uplevel 1 $body} err]
        if {$res == 3} { break }
        if {$res == 4} { set i [expr $i + 1]; continue }
        if {$res == 1} { error $err }
        if {$res == 2} { return $err }
        set i [expr $i + 1]
    }
}

# --- 列表操作 ---
# lappend: 追加元素至列表变量
proc lappend {varName val} {
    upvar 1 $varName v
    set tmp $v
    if { [string compare $tmp {}] != 0 } {
        append tmp { }
        append tmp $val
        set v $tmp
    } else {
        set v $val
    }
    return $v
}

# lset: 修改列表中指定索引的元素
proc lset {varName index val} {
    upvar 1 $varName list
    set pre [lrange $list 0 [expr $index - 1]]
    set post [lrange $list [expr $index + 1] end]
    set res $pre
    if { [llength $res] > 0 } { append res { } }
    append res $val
    if { [llength $post] > 0 } { append res { }; append res $post }
    set list $res
}

# lsearch: 查找元素在列表中的索引
proc lsearch {list pattern} {
    set i 0
    set len [llength $list]
    while {$i < $len} {
        set item [lindex $list $i]
        if { [string compare $item $pattern] == 0 } { return $i }
        set i [expr $i + 1]
    }
    return -1
}

# --- 字符串操作 (对齐标准 Tcl) ---
# string: 字符串操作集合
proc string {subcmd args} {
    if { [__string_core compare $subcmd {compare}] == 0 } {
        return [__string_core compare [lindex $args 0] [lindex $args 1]]
    }
    set str [lindex $args 0]
    if { [__string_core compare $subcmd {length}] == 0 } {
        return [llength $str] 
    }
    if { [__string_core compare $subcmd {index}] == 0 } {
        return [lindex $str [lindex $args 1]]
    }
    if { [__string_core compare $subcmd {range}] == 0 } {
        return [lrange $str [lindex $args 1] [lindex $args 2]]
    }
    error [list unknown string subcommand $subcmd]
}

# --- 格式化输出 (极简实现) ---
proc format {fmt args} {
    set res {}
    set arg_idx 0
    set len [llength $fmt]
    set i 0
    while {$i < $len} {
        set part [lindex $fmt $i]
        set c1 [string compare $part {%s}]
        set c2 [string compare $part {%d}]
        if { $c1 == 0 || $c2 == 0 } {
            append res [lindex $args $arg_idx]
            set arg_idx [expr $arg_idx + 1]
        } else {
            append res $part
        }
        append res { }
        set i [expr $i + 1]
    }
    return $res
}

# --- 方言兼容性 Shim ---
# t_scmp: 向后兼容旧的方言指令
proc t_scmp {s1 s2} { return [string compare $s1 $s2] }
