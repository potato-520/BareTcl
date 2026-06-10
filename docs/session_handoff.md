# BareTcl A-B 协作开发 —— 新会话交接文档

> 生成时间：2026-06-10  
> 本文档用于在新的 AI Agent 会话中无缝继续 BareTcl 的 A-B 协作开发工作。

---

## 一、项目概述

**BareTcl** 是一个为工业级裸机（Bare-metal）MCU（如 Renesas RH850/U2A）设计的极简 Tcl 解释器。核心特性：
- 零标准库依赖（`-nostdlib` 编译）
- 静态 Arena 内存管理（双向游标，无 malloc/free）
- 完全无栈化状态机（FSM）实现，杜绝栈溢出
- 三层指令架构：`tcl_core.c`（原初原子）/ `extcmd.c`（环境扩展）/ `tcllib.tcl`（Tcl 自举）

**项目路径**：`/mnt/c/myprog/BareTcl`  
**GitHub**：`https://github.com/potato-520/BareTcl.git`（`main` 分支，已同步）

---

## 二、上一会话完成的工作（最新 git log）

```
9871843  fix: __info_commands_core 未找到命令时返回空字符串而非 TCL_NULL
92be578  feat: 自举迁移 lrange/global/info + 修复 if-else else 分支不执行 Bug
861493d  fix: 修复 upvar #0 全局帧查找逻辑
d3eff3d  fix: 实现反斜杠换行续行（Line Continuation）
13f1fd4  fix: 修复 expr 指令对 $var 变量不展开的问题
f26b3de  feat: 实现双引号字符串插值（String Interpolation）
```

### 具体修复内容

| 修复项 | 文件 | 说明 |
|--------|------|------|
| `expr {$var + 1}` 变量不展开 | `tcl_core.c` | ST_EXPAND 剥花括号后 expr 未展开 $var；重构单参数路径，先 interp 再新帧求值 |
| 反斜杠换行续行（`\<newline>`） | `tcl_core.c` | ST_TOKENIZE 增加续行检查，使 `run_test "x" \\ "y"` 多行写法生效 |
| 双引号字符串插值 | `tcl_core.c` | 新增 `tcl_str_interp`，ST_EXPAND 双引号分支调用它展开 `$var` 和 `\escape` |
| `upvar #0` 找不到顶层帧变量 | `tcl_core.c` | `#0` 分支从沿 parent 链追踪到最顶层帧，而非直接使用 `g_vars`（TCL_NULL） |
| **`if-else` else 分支从不执行** | `tcl_core.c` | ST_IF_BODY 假分支完全忽略了 `tmp_roots[11]` 中的 else body；修复后创建 else 子帧 |
| 自举迁移：`lrange` | `tcllib.tcl` | 从 `tcl_core.c` 删除 C 实现，改为 `llength`+`lindex`+`while` Tcl 自举 |
| 自举迁移：`global` | `tcllib.tcl` | 从 `tcl_core.c` 删除 C 实现，改为 `uplevel 1 [list upvar #0 x x]` 自举 |
| 自举迁移：`info` | `extcmd.c` + `tcllib.tcl` | C 层提供 `__info_commands_core` 底层查询，Tcl 层封装 `proc info` |
| `info commands` 返回 TCL_NULL | `extcmd.c` | 找不到命令时应返回真空字符串对象，而非 TCL_NULL（后者会导致 `$var` 展开误判为未定义） |

---

## 三、当前测试状态（`bash build.sh`）

运行 `tests/tests.tcl`，**第 1 轮**结果：

| 分类 | 测试项 | 状态 |
|------|--------|------|
| 分类1: 核心原子指令 | 变量读写、算术运算、逻辑比较、命令替换 | ✅ PASS |
| 分类2: 过程与作用域 | 过程定义与调用、Uplevel 跨帧访问 | ✅ PASS |
| 分类3: 自举库 | for循环、min/max/abs、incr、info_exists、lappend、lrange、lreverse、lset、foreach | ✅ PASS |
| 分类4: 深度递归 | Fibonacci(10)、汉诺塔(4盘) | ✅ PASS |
| 分类4: 深度递归 | **8皇后求解** | ❌ FAIL（`Error:`） |
| 分类5: GC压力测试 | 未到达（因8皇后失败提前退出） | ❓ 未知 |

**测试套件循环 5 轮**，当前因 8 皇后失败无法到达"所有测试通过"的结论行。

---

## 四、下一步工作：8 皇后求解失败分析

### 8 皇后测试代码（`tests/tests.tcl` L253-L283）

```tcl
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
```

### 疑点分析（需在新会话调查）

1. **`incr i` 带隐式步长**：当前 `tcllib.tcl` 中的 `incr` 只支持无参数版（步长=1），`incr col` 和 `incr i` 应该没问题，但 `incr i -1`（在 lreverse polyfill 里）可能失败。

2. **`lrange $board 0 [expr {$row - 1}]`**：当 `$row=0` 时，`$row - 1 = -1`，`lrange list 0 -1` 的行为（应返回空列表）是否正确？

3. **`lappend new_board $col`**：在 proc 内部，`new_board` 的 `lappend` 需要 `upvar 1`，但 `tcllib.tcl` 中 `lappend` 使用的是 `upvar 1 $varName v`，这在嵌套调用中是否正确？

4. **深度递归导致 Arena 栈区耗尽**：8皇后递归深度达 8 层（每层多个帧），加上 while 循环帧，可能触发栈区与堆区碰撞（OOM）。

5. **`[q_check ...]` 返回值用于 if 条件**：`if {[q_check ...]}` 中命令替换返回 `0` 或 `1`，这需要 `[cmd]` 在花括号条件里正确执行。

### 调试方法

```bash
# 在项目根目录
gcc examples/demo.c -o tclsh_dbg -g -fsanitize=address
cat > /tmp/test_queens.tcl << 'EOF'
# 先测试最简单的 q_check
proc q_check {row col board} {
    set i 0
    while {$i < $row} {
        set b_i [lindex $board $i]
        if {$b_i == $col} { return 0 }
        incr i
    }
    return 1
}
puts [q_check 1 0 {0}]
puts [q_check 1 1 {0}]
EOF
timeout 5s ./tclsh_dbg /tmp/test_queens.tcl 2>&1
```

---

## 五、已知待解决的其他问题

| 问题 | 严重度 | 说明 |
|------|--------|------|
| `[cmd]` 在双引号字符串内不执行 | 高 | `puts "result=[cmd]"` 中 `[cmd]` 被当字面量输出；`tcl_str_interp` 只处理 `$var` 和 `\escape`，未处理 `[...]` |
| `for` 第一次循环体执行前 `$i` 为空 | 低 | 表现为输出第一行 `round=`（空），随后正常 `round=1 2 3`；for body 第一次执行时变量可能还未在正确帧中 |
| `expr {$a == ""}` 与空字符串比较 | 中 | `expr {$r == ""}` 返回空而非 `1`；字符串 `==` 比较路径在 expr 三元归约中未正确实现 |
| `incr` 不支持增量参数（`incr i -1`） | 中 | 当前 `tcllib.tcl` 中 `incr` 只支持 `incr varname`，未支持 `incr varname N` 的第二个参数 |
| `lindex $list end` 不支持 `end` 关键字 | 中 | C 实现的 `lindex` 没有处理 `end` 字符串，需要补充 |
| `lrange $list 0 end-1` 不支持 | 中 | `end-1` 算术表达式在 lrange 索引位置未实现 |
| `## 第 $round / 5 轮` 显示为空 | 低 | for 循环 body 内的 `$round` 在 tcllib.tcl 的 for proc 中无法查找到外部变量（作用域链问题） |

---

## 六、架构关键知识

### 文件结构

```
src/
  tcl_core.c    - 解释器核心（FSM、Arena GC、17个原初原子指令）
  extcmd.c      - 环境扩展（puts, exit, append, __string_core, __info_commands_core）
  tcllib.tcl    - Tcl 自举库（for, foreach, lrange, global, info, lappend 等）
  tcllib.c      - tcllib.tcl 的 C 字节数组形式（由 tools/tcl2c.py 自动生成）
  baretcl_shell.c - 交互式 REPL shell
examples/
  demo.c        - 测试用外壳（#include tcl_core.c + extcmd.c + tcllib.c）
tests/
  tests.tcl     - 工业级测试套件（5轮压力测试，约30个用例）
tools/
  tcl2c.py      - 将 tcllib.tcl 转换为 C 字节数组的工具
build.sh        - 完整构建+测试流水线
```

### 构建与测试

```bash
bash build.sh                   # 完整流水线：生成tcllib.c → 编译 → 测试
gcc examples/demo.c -o tclsh_dbg -g -fsanitize=address  # 调试构建
timeout 15s ./tclsh_dbg tests/tests.tcl 2>&1            # 手动跑测试
```

### FSM 状态说明

| 状态 | 值 | 说明 |
|------|-----|------|
| ST_TOKENIZE | 0 | 分词阶段，提取 token |
| ST_EXPAND | 1 | 展开阶段，处理 `$var`、`[cmd]`、`{}` |
| ST_EXECUTE | 2 | 执行阶段，调用命令函数 |
| ST_RESUME | 3 | 恢复阶段，子帧返回后处理结果 |
| ST_IF_COND | 8 | if 条件求值 |
| ST_IF_BODY | 9 | if 结果判定并执行 body/else |
| ST_WHILE_COND | 4 | while 条件求值 |
| ST_WHILE_BODY | 5 | while body 执行 |

### 关键数据结构

```c
typedef struct TclFrame {
    tcl_u32 script;    // 当前执行脚本的 Arena 偏移
    tcl_u32 pc;        // 程序计数器（脚本内字节偏移）
    tcl_u32 vars;      // 局部变量链表头偏移（TCL_NULL=无）
    tcl_u32 parent;    // 物理调用链父帧偏移
    tcl_u32 scope;     // 逻辑作用域帧偏移（变量查找起点）
    tcl_u32 cond;      // if/while 条件脚本偏移
    tcl_u32 body;      // if/while body 脚本偏移
    tcl_u32 result;    // 临时结果槽位
    tcl_u32 argv[MAX_ARGS]; // 参数数组
    tcl_i32 argc;      // 参数数量
    tcl_i32 exp_idx;   // 当前展开的参数索引
    tcl_u32 state;     // 当前 FSM 状态
    tcl_u32 flags;     // 帧标志（FRAME_SHARE_SCOPE | FRAME_IS_EXPR 等）
} TclFrame;
```

---

## 七、新会话 Prompt

请将以下内容复制到新会话的第一条消息中：

---

```
我正在继续 BareTcl 解释器的 A-B 协作开发。请先阅读
docs/session_handoff.md 了解上一会话的工作成果和当前状态，
然后阅读 GEMINI.md 了解开发规约。

你作为 Master Agent，请：
1. 运行 bash build.sh，确认当前失败在「8皇后求解」
2. 启动新一轮 A-B 协作，以「8皇后求解失败」为首个目标
3. 严格遵循 GEMINI.md 中的 A-B 协作框架（5.2节）

项目路径：/mnt/c/myprog/BareTcl
```

---

## 八、附：tcllib.tcl 当前实现清单

| proc | 说明 | 依赖的原初指令 |
|------|------|--------------|
| `abs` | 绝对值 | expr, if, return |
| `incr` | 变量自增（仅支持步长1） | upvar, expr, set |
| `for` | for 循环 | uplevel, while, catch, break, continue |
| `foreach` | 列表遍历 | llength, lindex, uplevel, while, set |
| `lappend` | 追加元素至列表变量 | upvar, string compare, append |
| `lset` | 修改列表指定索引 | upvar, lrange, llength, append |
| `lsearch` | 查找元素索引 | llength, lindex, while, string compare |
| `string` | 字符串操作集合（compare/length/index/range） | __string_core |
| `format` | 极简格式化（仅支持 %s/%d） | llength, lindex, append |
| `t_scmp` | 方言兼容 shim | string compare |
| `lrange` | 列表范围截取（支持 end） | llength, lindex, while, append |
| `global` | 全局变量声明 | uplevel, upvar |
| `info` | 运行时自省（仅 info commands） | __info_commands_core |
| `info_exists` | 变量存在性检查 | catch, uplevel |
