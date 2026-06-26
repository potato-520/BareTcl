# tcllib.tcl - BareTcl 自举脚本库 (Bootstrap Library)

# --- 基础数学函数 ---
# abs: 返回绝对值
proc abs {x} { 
    if {$x < 0} { 
        return [expr 0 - $x] 
    }; 
    return $x 
}

# --- 圆周率高精度测试函数 ---
# pi: 使用 Machin 公式计算圆周率，digits 为小数点后位数。
proc pi_big_add {A B} {
    set res {}
    set carry 0
    set lenA [llength $A]
    set lenB [llength $B]
    set max_len $lenA
    if {$lenB > $max_len} { set max_len $lenB }
    for {set i 0} {$i < $max_len || $carry > 0} {incr i} {
        set valA 0
        if {$i < $lenA} { set valA [lindex $A $i] }
        set valB 0
        if {$i < $lenB} { set valB [lindex $B $i] }
        set sum [expr {$valA + $valB + $carry}]
        append res " " [expr {$sum % 10000}]
        set carry [expr {$sum / 10000}]
    }
    return $res
}

proc pi_big_sub {A B} {
    set res {}
    set borrow 0
    set lenA [llength $A]
    set lenB [llength $B]
    for {set i 0} {$i < $lenA} {incr i} {
        set valA [lindex $A $i]
        set valB 0
        if {$i < $lenB} { set valB [lindex $B $i] }
        set diff [expr {$valA - $valB - $borrow}]
        if {$diff < 0} {
            set diff [expr {$diff + 10000}]
            set borrow 1
        } else {
            set borrow 0
        }
        append res " " $diff
    }
    while {[llength $res] > 1 && [lindex $res end] == 0} {
        set res [lrange $res 0 end-1]
    }
    return $res
}

proc pi_big_div_short {A divisor} {
    set len [llength $A]
    set rem 0
    set res {}
    for {set i [expr {$len - 1}]} {$i >= 0} {incr i -1} {
        set val [lindex $A $i]
        set cur [expr {$rem * 10000 + $val}]
        set d [expr {$cur / $divisor}]
        set rem [expr {$cur % $divisor}]
        set res "$d $res"
    }
    while {[llength $res] > 1 && [lindex $res end] == 0} {
        set res [lrange $res 0 end-1]
    }
    return $res
}

proc pi_arccot_scaled {x multiplier M} {
    set start {}
    for {set i 0} {$i < $M} {incr i} {
        append start " 0"
    }
    append start " " $multiplier
    set term [pi_big_div_short $start $x]
    set sum $term
    set n 1
    set x2 [expr {$x * $x}]
    while {1} {
        set term [pi_big_div_short $term $x2]
        if {[llength $term] == 1 && [lindex $term 0] == 0} {
            break
        }
        set term_to_add [pi_big_div_short $term [expr {2 * $n + 1}]]
        if {[llength $term_to_add] == 1 && [lindex $term_to_add 0] == 0} {
            break
        }
        if {[expr {$n % 2}] == 1} {
            set sum [pi_big_sub $sum $term_to_add]
        } else {
            set sum [pi_big_add $sum $term_to_add]
        }
        incr n
    }
    return $sum
}

proc pi_pad_zero {val} {
    if {$val < 10} { return "000$val" }
    if {$val < 100} { return "00$val" }
    if {$val < 1000} { return "0$val" }
    return $val
}

proc pi_pad_width {val width} {
    set text $val
    while {[string length $text] < $width} {
        set text "0$text"
    }
    return $text
}

proc pi {digits} {
    if {$digits < 0} {
        error "digits must be >= 0"
    }
    if {$digits == 0} {
        return 3
    }

    set blocks [expr {($digits + 3) / 4}]
    set remainder [expr {$digits % 4}]
    set M [expr {$blocks + 3}]

    set part1 [pi_arccot_scaled 5 16 $M]
    set part2 [pi_arccot_scaled 239 4 $M]
    set pi_val [pi_big_sub $part1 $part2]

    set pi_rev {}
    set len_pi [llength $pi_val]
    for {set i [expr {$len_pi - 1}]} {$i >= 0} {incr i -1} {
        append pi_rev " " [lindex $pi_val $i]
    }

    set first [lindex $pi_rev 0]
    set rest_str {}
    set full_blocks $blocks
    if {$remainder != 0} {
        set full_blocks [expr {$blocks - 1}]
    }

    set idx 1
    while {$idx <= $full_blocks} {
        append rest_str [pi_pad_zero [lindex $pi_rev $idx]]
        incr idx
    }

    if {$remainder != 0} {
        set last_group [lindex $pi_rev [expr {$full_blocks + 1}]]
        if {$remainder == 1} {
            set last_group [expr {$last_group / 1000}]
        }
        if {$remainder == 2} {
            set last_group [expr {$last_group / 100}]
        }
        if {$remainder == 3} {
            set last_group [expr {$last_group / 10}]
        }
        append rest_str [pi_pad_width $last_group $remainder]
    }

    return "$first.$rest_str"
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



proc q_rec {r b} {
    if {$r == 8} {
        puts $b
    } else {
        for {set c 0} {$c < 8} {incr c} {
            set k 1
            for {set i 0} {$i < $r} {incr i} {
                set y [lindex $b $i]
                if {$y == $c || [abs [expr $c - $y]] == [expr $r - $i]} {
                    set k 0
                    break
                }
            }
            if {$k} {
                set n $b
                lappend n $c
                q_rec [expr $r + 1] $n
            }
        }
    }
}

proc queens {} {
    q_rec 0 {}
}

# help: BareTcl 基础命令帮助
proc help {} {
    puts "BareTcl commands:"
    puts "  abs <x>                            # return absolute value"
    puts "  incr <varName> ?step?              # increment variable"
    puts "  for <start> <cond> <next> <body>   # standard for loop"
    puts "  foreach <var> <list> <body>        # standard foreach loop"
    puts "  lappend <varName> ?arg ...?        # append items to list"
    puts "  lset <varName> <index> <val>       # set list element"
    puts "  lsearch <list> <pattern>           # search item in list"
    puts "  pi <digits>                        # calculate Pi to N decimal places"
    puts "  queens                             # solve 8-queens puzzle (recursive test)"
    puts "  exit                               # leave BareTcl mode"
}
