# tcllib.tcl - BareTcl 自举脚本库 (Bootstrap Library)

# --- 基础数学函数 ---
# abs: 返回绝对值
proc abs {x} { 
    if {$x < 0} { 
        return [expr 0 - $x] 
    }; 
    return $x 
}
# incr: 变量自增，支持可选步长参数，保持与标准 Tcl 的基础语义一致
proc incr {varName args} {
    upvar 1 $varName value
    set args_marker "<$args>"
    if {[string compare $args_marker "<>"] == 0} {
        set value [expr {$value + 1}]
        return $value
    }
    set step [lindex $args 0]
    if {[string compare "<[lindex $args 1]>" "<>"] != 0} {
        error "wrong # args: should be \"incr varName ?increment?\""
    }
    set value [expr {$value + $step}]
    return $value
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
# lappend: 向列表变量追加一个或多个元素，使用列表元素表示法保持空格安全
proc lappend {varName args} {
    upvar 1 $varName list_value
    set existing_value {}
    if {[catch {set existing_value $list_value}] != 0} {
        set existing_value {}
    }
    set args_marker "<$args>"
    if {[string compare $args_marker "<>"] == 0} {
        set list_value $existing_value
        return $list_value
    }
    set arg_count [llength $args]
    set current_value $existing_value
    set index 0
    while {$index < $arg_count} {
        set element_value [lindex $args $index]
        if {$current_value ne ""} {
            append current_value { }
        }
        append current_value [list $element_value]
        set index [expr {$index + 1}]
    }
    set list_value $current_value
    return $list_value
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
        return [__string_core length $str] 
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


# --- 全局变量声明 (自举实现) ---
# global: 在过程内建立对顶层命名空间变量的别名
# 等价于对每个变量名执行 upvar #0 varname varname
proc global {args} {
    set i 0
    set len [llength $args]
    while {$i < $len} {
        set vname [lindex $args $i]
        uplevel 1 [list upvar #0 $vname $vname]
        set i [expr $i + 1]
    }
}

# --- 信息检查辅助 ---
# info_exists: 检查变量是否已定义（返回 1 或 0）
proc info_exists {varName} {
    set res [catch {uplevel 1 [list set $varName]} _]
    if {$res == 0} { return 1 }
    return 0
}
