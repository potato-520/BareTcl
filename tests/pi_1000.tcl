# ==============================================================================
# BareTcl 圆周率计算脚本 (计算小数点后1000位)
# ==============================================================================
# 本脚本使用大数高精度算法 (Machin 经典公式: Pi/4 = 4*arctan(1/5) - arctan(239))
# 算法核心：
# 1. 每一个大数使用 Tcl List 存储，List 的每个元素代表一个 10000 进制的数字 (4个十进制位)
# 2. List 中元素采用反向存储 (低位在前，高位在后)，便于从低位向高位进行加减法的进位/借位处理
# 3. 编写基于 List 的高精度加法 (big_add)、减法 (big_sub)、单精度除法 (big_div_short)
# 4. 实现 scaled 形式的 arctan(x) 展开计算，最终合成 Pi 并按十进制格式输出
# ==============================================================================

# --- 高精度加法 ---
# A 和 B 分别为低位在前的大数 List，返回相加后的新 List
proc big_add {A B} {
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
        
        set s [expr {$valA + $valB + $carry}]
        append res " " [expr {$s % 10000}]
        set carry [expr {$s / 10000}]
    }
    return $res
}

# --- 高精度减法 ---
# 计算 A - B，假定 A >= B，返回相减后的新 List
proc big_sub {A B} {
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
    
    # 移除最高位的无用 0 (即反向存储的 List 尾部的 0)
    while {[llength $res] > 1 && [lindex $res end] == 0} {
        set res [lrange $res 0 [expr {[llength $res] - 2}]]
    }
    return $res
}

# --- 高精度除以单精度除数 ---
# 计算 A / divisor，返回相除后的新 List
proc big_div_short {A divisor} {
    set len [llength $A]
    set rem 0
    set res {}
    
    # 除法从高位 (MSB) 向低位 (LSB) 顺序进行
    for {set i [expr {$len - 1}]} {$i >= 0} {incr i -1} {
        set val [lindex $A $i]
        set cur [expr {$rem * 10000 + $val}]
        set d [expr {$cur / $divisor}]
        set rem [expr {$cur % $divisor}]
        
        # 前置插入，自动实现反转 (高位在后，低位在前)
        set res "$d $res"
    }
    
    # 移除最高位 (即反转后 List 尾部) 的无用 0
    while {[llength $res] > 1 && [lindex $res end] == 0} {
        set res [lrange $res 0 end-1]
    }
    return $res
}

# --- scaled arccot(x) 计算 ---
# 计算 multiplier * arctan(1/x) * 10^(M*4)
proc arccot_scaled {x multiplier M} {
    # 构造初始大数 multiplier * 10^(M*4)，在 10000 进制下即为 M 个 0，后接 multiplier
    set start {}
    for {set i 0} {$i < $M} {incr i} {
        append start " 0"
    }
    append start " " $multiplier
    
    set term [big_div_short $start $x]
    set sum $term
    set n 1
    set x2 [expr {$x * $x}]
    
    while {1} {
        set term [big_div_short $term $x2]
        if {[llength $term] == 1 && [lindex $term 0] == 0} {
            break
        }
        set term_to_add [big_div_short $term [expr {2 * $n + 1}]]
        if {[llength $term_to_add] == 1 && [lindex $term_to_add 0] == 0} {
            break
        }
        if {[expr {$n % 2}] == 1} {
            set sum [big_sub $sum $term_to_add]
        } else {
            set sum [big_add $sum $term_to_add]
        }
        incr n
    }
    return $sum
}

# --- 辅助补零函数 ---
# 将不足 4 位的元素用前导 0 补齐为 4 位字符串
proc pad_zero {val} {
    if {$val < 10} { return "000$val" }
    if {$val < 100} { return "00$val" }
    if {$val < 1000} { return "0$val" }
    return $val
}

# ==============================================================================
# 主执行入口
# ==============================================================================
puts "正在计算圆周率 Pi 小数点后 1000 位，请稍候..."

# 1000 个十进制位需要 250 个 10000 进制位
# 为防止舍入误差，设定精度为 253 个 10000 进制位 (即 M = 253)
set M 253

# Machin 公式: Pi = 16*arctan(1/5) - 4*arctan(239)
set part1 [arccot_scaled 5 16 $M]
set part2 [arccot_scaled 239 4 $M]
set pi [big_sub $part1 $part2]

# 将低位在前大数反转为正常的高位在前顺序，准备进行十进制格式化输出
set pi_rev {}
set len_pi [llength $pi]
for {set i [expr {$len_pi - 1}]} {$i >= 0} {incr i -1} {
    append pi_rev " " [lindex $pi $i]
}

# 格式化输出: 第一位为整数 3，后面接点，再接 250 个元素 (共 1000 个十进制数字)
set first [lindex $pi_rev 0]
set rest [lrange $pi_rev 1 250]
set rest_str {}
foreach d $rest {
    append rest_str [pad_zero $d]
}

puts "计算结果 (小数点后 1000 位):"
puts "$first.$rest_str"
