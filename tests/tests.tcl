# ====================================================
# BareTcl 工业级综合测试套件 (tests.tcl)
# ====================================================
# 规约：
# 1. 目标对齐 PC 标准版 Tcl 语法。
# 2. 每个测试必须输出：内容、预期、过程、结果。
# 3. 缺失的功能在本脚本中通过 Tcl 自行实现（Polyfills）。
# ====================================================

# --- 兼容性补丁层 (Polyfills) ---

# 模拟 BareTcl 内部指令
if {[info commands t_scmp] == ""} {
    proc t_scmp {s1 s2} { return [string compare $s1 $s2] }
}

# 补全 PC 版可能缺失或不一致的指令
if {[info commands min] == ""} {
    proc min {a b} { if {$a < $b} { return $a }; return $b }
}
if {[info commands max] == ""} {
    proc max {a b} { if {$a > $b} { return $a }; return $b }
}
if {[info commands abs] == ""} {
    proc abs {x} { if {$x < 0} { return [expr {0 - $x}] }; return $x }
}
if {[info commands info_exists] == ""} {
    proc info_exists {v} { uplevel 1 [list info exists $v] }
}
if {[info commands lreverse] == ""} {
    proc lreverse {list} {
        set res {}
        for {set i [expr {[llength $list] - 1}]} {$i >= 0} {incr i -1} {
            lappend res [lindex $list $i]
        }
        return $res
    }
}
if {[info commands lpop] == ""} {
    proc lpop {varName} {
        upvar 1 $varName list
        set val [lindex $list end]
        set list [lrange $list 0 end-1]
        return $val
    }
}

# 定义辅助命令
proc PASS {} { return "PASS" }
proc FAIL {} { return "FAIL" }

set pass_count 0
set fail_count 0

# 增强型断言过程
# 参数：
#   label: 测试项名称
#   content: 测试的具体操作描述
#   expected: 预期结果描述
#   cond_script: Tcl 脚本，其 eval 结果应为布尔真值
proc run_test {label content expected cond_script} {
    global pass_count fail_count
    puts "----------------------------------------------------"
    puts "测试项: $label"
    puts "测试内容: $content"
    puts "预期结果: $expected"
    puts "执行过程..."
    
    set code [catch {uplevel 1 $cond_script} result]
    
    if {$code == 0 && ($result != 0 && $result ne "")} {
        set pass_count [expr {$pass_count + 1}]
        puts "最终结果: [PASS]"
    } else {
        set fail_count [expr {$fail_count + 1}]
        puts "最终结果: [FAIL]"
        if {$code != 0} { puts "错误信息: $result" }
        puts "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
        puts "致命错误: 零容忍策略触发，测试中止。"
        puts "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
        exit 1
    }
}

puts "===================================================="
puts "BareTcl 系统验证开始执行 (共 5 轮压力测试)..."
puts "===================================================="

for {set round 1} {$round <= 5} {set round [expr {$round + 1}]} {

puts "\n####################################################"
puts "##  第 $round / 5 轮测试开始执行"
puts "####################################################"

# ====================================================
# 第一部分：核心原子指令与变量替换 (Core Atoms)
# ====================================================
puts "\n{分类 1: 核心原子指令与变量替换}"

run_test "变量读写" \
         "设置变量 a 为 100，并读取其值进行比较" \
         "结果应为 100" \
         {expr {[set a 100] == 100}}

run_test "算术运算" \
         "计算 10 + 20 - 5 * 2 / 2" \
         "结果应为 25" \
         {expr {[expr {10 + 20 - 5 * 2 / 2}] == 25}}

run_test "逻辑比较" \
         "执行 1 == 1, 2 != 1, 1 < 2 组合逻辑" \
         "结果应为真 (1)" \
         {expr {[expr {1 == 1}] && [expr {2 != 1}] && [expr {1 < 2}]}}

run_test "命令替换" \
         "嵌套执行 set b [expr \$a + 1]" \
         "b 的值应为 101" \
         {expr {[set b [expr {$a + 1}]] == 101}}

# ====================================================
# 第二部分：过程、作用域与环境 (Procs & Scopes)
# ====================================================
puts "\n{分类 2: 过程调用与作用域管理}"

proc test_proc {x y} {
    set local_v 50
    return [expr {$x + $y + $local_v}]
}
run_test "过程定义与调用" \
         "定义 test_proc 并计算 10 + 20 + 局部变量 50" \
         "结果应为 80" \
         {expr {[test_proc 10 20] == 80}}

set upv 0
proc check_uplevel {} {
    uplevel {set upv 123}
}
check_uplevel
run_test "Uplevel 跨帧访问" \
         "在过程中通过 uplevel 修改调用者的变量 upv" \
         "全局变量 upv 应变为 123" \
         {expr {$upv == 123}}

# ====================================================
# 第三部分：标准库指令验证 (Bootstrap Lib)
# ====================================================
puts "\n{分类 3: Tcl 自举库指令验证}"

# for 循环
set sum 0
for {set i 0} {$i < 10} {set i [expr {$i + 1}]} {
    set sum [expr {$sum + $i}]
}
run_test "自举: for 循环" \
         "累加 0 到 9" \
         "总和应为 45" \
         {expr {$sum == 45}}

# 数学工具
run_test "自举: 数学工具 (min/max/abs)" \
         "测试 min(10,20), max(10,20), abs(-5)" \
         "结果应全为真" \
         {expr {[min 10 20] == 10 && [max 10 20] == 20 && [abs -5] == 5}}

# incr
set counter 10
incr counter
incr counter 4
run_test "自举: incr" \
         "执行 incr 1次默认及1次增量4" \
         "counter 应为 15" \
         {expr {$counter == 15}}

# info_exists
set real_var 1
run_test "自举: info_exists" \
         "检查已存在变量 real_var 和不存在变量 ghost_var" \
         "应分别返回 1 和 0" \
         {expr {[info_exists real_var] == 1 && [info_exists ghost_var] == 0}}

# 列表操作: lappend
set mylist "10 20"
lappend mylist 30 40
run_test "自举: lappend" \
         "向列表追加 30 40" \
         "列表长度应为 4 且末尾为 40" \
         {expr {[llength $mylist] == 4 && [lindex $mylist 3] == 40}}

# 列表操作: lrange
set sublist [lrange $mylist 1 2]
run_test "自举: lrange" \
         "从列表中截取索引 1 到 2 的子列表" \
         "子串应为 {20 30}" \
         {expr {[llength $sublist] == 2 && [lindex $sublist 1] == 30}}

# 列表操作: lreverse
set revlist [lreverse $sublist]
run_test "自举: lreverse" \
         "翻转子列表 {20 30}" \
         "结果应为 {30 20}" \
         {expr {[lindex $revlist 0] == 30 && [lindex $revlist 1] == 20}}

# 列表操作: lset
lset revlist 1 99
run_test "自举: lset" \
         "修改 revlist 索引 1 的值为 99" \
         "索引 1 应变为 99" \
         {expr {[lindex $revlist 1] == 99}}

# foreach
set f_sum 0
foreach it "30 88" {
    set f_sum [expr {$f_sum + $it}]
}
run_test "自举: foreach" \
         "遍历列表 {30 88} 进行累加" \
         "结果应为 118" \
         {expr {$f_sum == 118}}

# ====================================================
# 第四部分：深度递归与算法压力测试 (Recursion & Stress)
# ====================================================
puts "\n{分类 4: 深度递归与无栈架构验证}"

# 斐波那契
proc fib {n} {
    if {$n < 2} { return $n }
    return [expr {[fib [expr {$n - 1}]] + [fib [expr {$n - 2}]]}]
}
run_test "递归: Fibonacci(10)" \
         "计算第 10 个斐波那契数 (递归深度验证)" \
         "结果应为 55" \
         {expr {[fib 10] == 55}}

# 汉诺塔
set moves 0
proc hanoi_counter {n from to aux} {
    global moves
    if {$n == 1} {
        set moves [expr {$moves + 1}]
    } else {
        hanoi_counter [expr {$n - 1}] $from $aux $to
        set moves [expr {$moves + 1}]
        hanoi_counter [expr {$n - 1}] $aux $to $from
    }
}
hanoi_counter 4 A C B
run_test "算法: 汉诺塔(4盘)" \
         "执行 4 盘汉诺塔移动计数" \
         "步数应为 15" \
         {expr {$moves == 15}}

# 8皇后
proc q_check {row col board} {
    set i 0
    while {$i < $row} {
        set b_i [lindex $board $i]
        if {$b_i == $col} { return 0 }
        set diff [expr {$row - $i}]
        set col_diff [expr {$col - $b_i}]
        if {$col_diff < 0} { set col_diff [expr {0 - $col_diff}] }
        if {$diff == $col_diff} { return 0 }
        incr i
    }
    return 1
}
proc q_solve {row board} {
    if {$row == 8} { return 1 }
    set col 0
    while {$col < 8} {
        if {[q_check $row $col $board]} {
            set new_board [lrange $board 0 [expr {$row - 1}]]
            lappend new_board $col
            if {[q_solve [expr {$row + 1}] $new_board]} { return 1 }
        }
        incr col
    }
    return 0
}
run_test "算法: 8皇后求解" \
         "寻找 8 皇后的第一个解 (极深度递归与列表生成)" \
         "结果应为找到解 (1)" \
         {q_solve 0 {}}

# ====================================================
# 第五部分：内存管理与 GC 压力测试 (GC & Memory)
# ====================================================
puts "\n{分类 5: 内存管理与垃圾回收}"

run_test "GC: 字符串拼接压力" \
         "循环 200 次拼接字符串以触发多次 GC" \
         "结果应正常完成" \
         {
             set s_gc {}
             set i_gc 0
             while {$i_gc < 200} {
                 append s_gc "abcdefghij"
                 incr i_gc
             }
             expr 1
         }

run_test "GC: 变量频繁创建与销毁" \
         "循环 200 次 set 变量后 unset 释放" \
         "结果应正常完成且内存无溢出" \
         {
             set i_vc 0
             while {$i_vc < 200} {
                 set v_tmp $i_vc
                 unset v_tmp
                 incr i_vc
             }
             expr 1
         }

run_test "GC: 对象移动稳定性" \
         "创建变量 a, b, c，触发 GC 后验证其值是否被正确搬迁" \
         "值应保持 original_data 且 a==b==c" \
         {
             set a_mv "original_data"
             set b_mv $a_mv
             set c_mv $b_mv
             set s_fill {}
             set i_fill 0
             while {$i_fill < 100} {
                 append s_fill "x"
                 incr i_fill
             }
             expr {[t_scmp $a_mv $b_mv] == 0 && [t_scmp $b_mv $c_mv] == 0 && [t_scmp $a_mv "original_data"] == 0}
         }

# ====================================================
# 第六部分：异常处理 (Error Handling)
# ====================================================
puts "\n{分类 6: 错误捕获与信号冒泡}"

run_test "Catch 错误捕获" \
         "使用 catch 捕获强制抛出的错误" \
         "catch 应返回 1 (TCL_ERROR)" \
         {expr {[catch {error "boom"}] == 1}}

# ====================================================
# 第七部分：历史导入测试 (Legacy & Regression)
# ====================================================
puts "\n{分类 7: 历史回归测试 - 严禁删减}"

# Imported from test_append.tcl
run_test "Legacy: append 指令" "test_append.tcl 内容" "x == {a b c}" {
    set x_l {a b}
    append x_l { }
    append x_l {c}
    expr {[t_scmp $x_l {a b c}] == 0}
}

# Imported from test_foreach.tcl
run_test "Legacy: foreach 指令" "test_foreach.tcl 内容" "输出应匹配" {
    set ml_l {a b c}
    set res_l {}
    foreach it_l $ml_l { append res_l $it_l }
    expr {[t_scmp $res_l {abc}] == 0}
}

# Imported from test_lappend.tcl & variants
proc lappend_l {vN val} {
    upvar 1 $vN v
    if {$v eq ""} { set v $val } else { append v " " $val }
    return $v
}
run_test "Legacy: lappend 实现" "通过 upvar 实现的 lappend" "mylist == {a b c}" {
    set ml_la {a b}
    lappend_l ml_la {c}
    expr {[t_scmp $ml_la {a b c}] == 0}
}

# Imported from test_lsearch.tcl
proc lsearch_l {list pattern} {
    set i 0; set res -1
    foreach item $list { if {[t_scmp $item $pattern] == 0} { set res $i; break }; set i [expr {$i + 1}] }
    return $res
}
run_test "Legacy: lsearch 实现" "搜索列表中的 X" "索引应为 1" {expr {[lsearch_l {a X c} {X}] == 1}}

# Imported from test_upvar_append.tcl
proc test_ua_l {vN} { upvar 1 $vN v; append v { world} }
run_test "Legacy: upvar 追加" "在 proc 中追加外部变量" "x == {hello world}" {
    set x_ua {hello}
    test_ua_l x_ua
    expr {[t_scmp $x_ua {hello world}] == 0}
}

# Imported from tests/rec_test.tcl
proc rec_l {n} { if {$n == 0} { return 1 }; set s [rec_l [expr {$n - 1}]]; return [expr {$s + 1}] }
run_test "Legacy: 线性递归" "递归深度 50" "结果应为 51" {expr {[rec_l 50] == 51}}

# Imported from tests/loop_rec_test.tcl
proc lr_l {n} { if {$n == 0} { return 1 }; set c 0; set i 0; while {$i < 2} { set s [lr_l [expr {$n - 1}]]; set c [expr {$c + $s}]; set i [expr {$i + 1}] }; return $c }
run_test "Legacy: 循环递归" "loop_rec 3" "结果应为 8" {expr {[lr_l 3] == 8}}

# Imported from tests/gc_test.tcl
run_test "Legacy: GC 循环" "运行 100 次循环自增" "结果为 100" {
    set a_gc 1
    while {$a_gc < 100} { set a_gc [expr {$a_gc + 1}] }
    expr {$a_gc == 100}
}

# ====================================================
# 第八部分：新增验证 (New Refactoring Verifications)
# ====================================================
puts "\n{分类 8: 重构逻辑深度验证}"

proc upvar_level2_test {vname} {
    upvar_level1_test $vname
}
proc upvar_level1_test {vname} {
    upvar 2 $vname v
    set v "level2_val"
}
run_test "Upvar 相对层级 (level 2)" \
         "在两层嵌套调用中通过 upvar 2 修改最外层变量" \
         "变量 uv_l2 应变为 level2_val" \
         {
             set uv_l2 "orig"
             upvar_level2_test uv_l2
             expr {[t_scmp $uv_l2 "level2_val"] == 0}
         }

run_test "Catch 结果变量捕获" \
         "执行 catch {error \"msg\"} res 并检查 res 的值" \
         "catch 返回 1 且 res 应为 \"msg\"" \
         {
             set catch_res "none"
             set catch_code [catch {error "msg"} catch_res]
             expr {$catch_code == 1 && [t_scmp $catch_res "msg"] == 0}
         }

}

# ====================================================
# 最终汇总
# ====================================================
puts "\n===================================================="
puts "最终 5 轮压力测试总结报告"
puts "  总计轮次: 5"
puts "  总计通过: $pass_count"
puts "  总计失败: $fail_count"
puts "===================================================="
if {$fail_count == 0} {
    puts "结论: 所有测试在标准 Tcl 环境下均已通过！考卷逻辑 100% 正确。"
} else {
    puts "结论: 存在逻辑错误，请检查 Polyfills 或用例实现。"
}
puts "===================================================="
exit
