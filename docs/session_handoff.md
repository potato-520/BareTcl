# BareTcl FACT 协作开发 —— 新会话交接文档

> 生成时间：2026-06-10（第二次更新）
> 本文档用于在新的 AI Agent 会话中无缝继续 BareTcl 的 FACT 协作开发工作。

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

## 二、历次会话完成的工作（最新 git log）

```
07493ba  fix: 修复 else body 被嵌套 if 语句覆盖的 Bug
ca033dd  docs: 添加新会话交接文档 session_handoff.md
9871843  fix: __info_commands_core 未找到命令时返回空字符串而非 TCL_NULL
92be578  feat: 自举迁移 lrange/global/info + 修复 if-else else 分支不执行 Bug
861493d  fix: 修复 upvar #0 全局帧查找逻辑
d3eff3d  fix: 实现反斜杠换行续行（Line Continuation）
13f1fd4  fix: 修复 expr 指令对 $var 变量不展开的问题
f26b3de  feat: 实现双引号字符串插值（String Interpolation）
```

### 本次会话核心修复

| 修复项 | 文件 | 说明 |
|--------|------|------|
| **`else` body 被嵌套 if 语句覆盖** | `tcl_core.c` | `tmp_roots[11]` 是全局状态，内层无 else 的 if 会将其清空为 TCL_NULL，导致外层 if 的 else 分支永远不执行。修复方案：改存于当前 if 帧的 `result` 字段（ST_IF_COND/ST_IF_BODY 阶段 result 字段空闲，可安全复用）。 |

---

## 三、当前测试状态（`bash build.sh`）

运行 `tests/tests.tcl`，当前结果（第二次会话后）：

| 分类 | 测试项 | 状态 |
|------|--------|------|
| 分类1: 核心原子指令 | 变量读写、算术运算、逻辑比较、命令替换 | ✅ PASS |
| 分类2: 过程与作用域 | 过程定义与调用、Uplevel 跨帧访问 | ✅ PASS |
| 分类3: 自举库 | for循环、min/max/abs、incr、info_exists、lappend、lrange、lreverse、lset、foreach | ✅ PASS |
| 分类4: 深度递归 | Fibonacci(10)、汉诺塔(4盘)、**8皇后求解** | ✅ PASS（本次修复）|
| 分类5: GC压力测试 | 字符串拼接、变量频繁创建销毁、对象移动稳定性 | ✅ PASS |
| 分类6: 错误捕获 | Catch 错误捕获 | ✅ PASS |
| 分类7: 历史回归 | append 指令 | ✅ PASS |
| 分类7: 历史回归 | **Legacy: foreach 指令** | ❌ FAIL（`Error: 0`）|

**测试套件共 5 轮**，当前因「Legacy: foreach 指令」失败提前退出。

---

## 四、下一步工作：Legacy foreach 指令失败分析

### 失败测试代码（`tests/tests.tcl` L356-361）

```tcl
run_test "Legacy: foreach 指令" "test_foreach.tcl 内容" "输出应匹配" {
    set ml_l {a b c}
    set res_l {}
    foreach it_l $ml_l { append res_l $it_l }
    expr {[t_scmp $res_l {abc}] == 0}
}
```

期望：`res_l` = `abc`（三次无空格拼接）  
实际：`res_l` 为空（`Error: 0`，说明 `[t_scmp {} {abc}]` 返回 0 是因为 `{}` != `abc`，结果为非零，但返回 0 意味着 t_scmp 异常？）

### 调试结果（已复现）

```bash
# 手动测试
set ml_l {a b c}
set res_l {}
foreach it_l $ml_l { append res_l $it_l }
puts $res_l  # 输出空行（res_l 为空）
```

### 疑点分析

1. **`append` 在 `uplevel 1 $body` 中的作用域**：
   - `foreach` 的 `tcllib.tcl` 实现使用 `uplevel 1 $body` 执行循环体
   - `append res_l $it_l` 中的 `res_l` 应在调用者帧（外层）
   - `it_l` 通过 `uplevel 1 [list set $var $elem]` 设置在调用者帧
   - 但 `append` 的实现是否正确处理了 `uplevel 1` 下的变量修改？

2. **`append` 指令实现**：`append` 在 `extcmd.c` 中实现，它直接修改**当前帧**的变量，而在 `uplevel 1 $body` 的上下文中，`$body` 在调用者帧执行，应该能看到调用者的 `res_l`。

3. **`lappend new_board $col`**（8皇后代码里也有 `lappend`）：8皇后里 `lappend new_board $col` 在 proc 内部直接调用（不通过 uplevel），而 foreach 里的 `append` 在 `uplevel 1 $body` 里调用——作用域层级不同。

4. **run_test 的 `uplevel 1` 层级**：`run_test` 中通过 `uplevel 1 $cond_script` 执行测试脚本，`cond_script` 内的 `foreach it_l $ml_l { append res_l $it_l }` 再通过 `tcllib.tcl` 的 `foreach` 中的 `uplevel 1 $body` 执行 `append`——**这里的 `uplevel 1` 相对于谁？**

5. **`lappend` vs `append` 的作用域差异**：分类3的 `foreach` 测试用 `set f_sum [expr ...]`（通过 uplevel 设置），这次测试用 `append res_l $it_l`（`append` 直接修改变量，可能路径不同）。

### 调试方法

```bash
cd /mnt/c/myprog/BareTcl
gcc examples/demo.c -o tclsh_dbg -g 2>&1

# 最小复现
cat > /tmp/test_foreach_debug.tcl << 'EOF'
proc myforeach {var list body} {
    set len [llength $list]
    set i 0
    while {$i < $len} {
        uplevel 1 [list set $var [lindex $list $i]]
        uplevel 1 $body
        set i [expr $i + 1]
    }
}

set res {}
myforeach it {a b c} { append res $it }
puts $res
EOF
timeout 5s ./tclsh_dbg /tmp/test_foreach_debug.tcl 2>&1
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

### 关键修复：else body 存储位置（本次会话完成）

```c
// tcl_cmd_if 中（tcl_core.c L688-701）
// 修复前：context->tmp_roots[11] = argument_values[4]（全局状态，会被内层 if 清空）
// 修复后：
active_frame_ptr->result = argument_values[4];  // 存于当前帧，各帧独立

// ST_IF_BODY 中（tcl_core.c L1921）
// 修复前：if (context->tmp_roots[11] != TCL_NULL)
// 修复后：
if (frame->result != TCL_NULL) {
    tcl_u32 else_body_script = frame->result;
    frame->result = TCL_NULL;
    // ...创建 else 子帧执行
}
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
| ST_COND | 6 | while 条件求值 |
| ST_LOOP | 7 | while body 执行 |

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
    tcl_u32 result;    // 临时结果槽位（兼用于 if 的 else_body 存储！）
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
我正在继续 BareTcl 解释器的 FACT 协作开发。请先阅读
docs/session_handoff.md 了解上一会话的工作成果和当前状态，
然后阅读 GEMINI.md 了解开发规约。

你作为 FACT 流程协调者，请：
1. 运行 bash build.sh，确认当前失败在「Legacy: foreach 指令」
2. 启动新一轮 FACT 协作，以「Legacy: foreach 指令失败」为首个目标
3. 严格遵循 GEMINI.md 中的 FACT 四方质证框架（第5节）

项目路径：/mnt/c/myprog/BareTcl
```

---

## 八、附：tcllib.tcl 当前实现清单

| proc | 说明 | 依赖的原初指令 |
|------|------|--------------| 
| `abs` | 绝对值 | expr, if, return |
| `incr` | 变量自增（支持步长 N） | upvar, expr, set |
| `for` | 标准 for 循环实现 | uplevel, while, catch, break, continue |
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

---

## 十、超级质检暴力测试结果（2026-06-12）

> 范围：仅针对当前已编译二进制 `./tclsh` 做黑盒测试（边界值、异常输入、压力输入）。  
> 参考基线：`/usr/bin/tclsh8.6`（用于标准 Tcl 对齐比对）。

### 10.1 发现的不符合项（需后续整改）

| 分类 | 用例脚本（最小复现） | 期望（标准 Tcl） | 当前 `./tclsh` 实际 |
|---|---|---|---|
| 条件真值语义 | `if {false} {puts bad} else {puts ok}` | `ok` | `bad` |
| 条件真值语义 | `if {off} {puts bad} else {puts ok}` | `ok` | `bad` |
| 条件真值语义 | `if {no} {puts bad} else {puts ok}` | `ok` | `bad` |
| 条件真值语义 | `set c [catch {if {abc} {puts x}} m]; puts $c` | `1`（非法布尔表达式） | 输出 `x` 且 `0` |
| while 条件判定 | `set i 0; while {false} {set i [expr {$i+1}]}; puts $i` | `0` | `Error: false` |
| 双引号命令替换 | `puts "r=[expr {1+2}]"` | `r=3` | `r=[expr {1+2}]` |
| 列表索引 end | `puts [lindex {a b c} end]` | `c` | `a` |
| 列表索引 end（长列表） | `set l {a b c d e f g h i j}; puts [lindex $l end]` | `j` | `a` |
| 列表切片 end-1 | `puts [lrange {a b c d} 0 end-1]` | `a b c` | `a b c d` |
| 列表切片 end-2 | `set l {a b c d e f g h i j}; puts [lrange $l 0 end-2]` | `a b c d e f g h` | `a b c d e f g h i j` |
| 压力/GC（append） | `set s {}; set i 0; while {$i < 1500} {append s a; set i [expr {$i+1}]}; puts [string length $s]` | `1500` | `Error: a` |
| 压力/GC（lappend） | `set l {}; set i 0; while {$i < 1200} {lappend l $i; set i [expr {$i+1}]}; puts [llength $l]` | `1200` | `Error: 0` |
| 递归/命令替换稳定性 | `proc f {n} {if {$n==0} {return 1}; return [expr {$n+[f [expr {$n-1}]]}]}; puts [f 12]` | `79` | `Error:`（空消息） |
| 异常输入诊断 | `set c [catch {puts [expr {1+2}} m]; puts $c; puts $m` | `1` + `missing close-bracket` | 输出 `1+` 且 `Error: 0` |
| 异常输入诊断 | `set c [catch {set no_such_var} msg]; puts $c; puts $msg` | `1` + 明确变量不存在错误 | `1` + `Error: 1`（信息不可用） |

### 10.2 备注

1. 以上用例覆盖了设计文档关注的“标准 Tcl 对齐”重点：`if/while` 条件判定、`expr/命令替换`、错误语义、列表操作及压力场景。
2. 本次仅记录问题，不包含任何源码修改；后续可按“单点突破”原则逐项修复并回归 `tests/tests.tcl`。

---

## 九、设计对齐审计（2026-06-12）

> 本节为“仅审计、不改代码”的现状评估，基准文档为 `docs/design.zh_CN.md`。

### 9.1 总结结论

当前实现与设计文档在主干架构上**总体同向**（静态 Arena、双游标、`tcl_eval` 无递归 FSM），但仍存在若干“规范级偏差”，主要集中在：
1. 三层指令架构边界（部分非环境命令下沉到 `extcmd.c`）
2. 标准 Tcl 语义对齐完整度（条件真值、双引号内命令替换、`info commands` 行为）
3. 基本类型规约执行一致性（局部仍使用原生 `int`）
4. “绝对无栈化”在 GC 标记阶段的边缘冲突（递归标记）

### 9.2 偏差清单（含证据）

| 偏差项 | 设计要求 | 代码证据 | 影响 |
|---|---|---|---|
| 扩展层职责越界 | `extcmd.c` 主要承载环境相关命令（文档示例为 `puts/exit`） | `src/extcmd.c` 注册 `append/lappend/incr/__string_core/__info_commands_core`（L232-236） | 三层分层纯度下降，核心语义与平台扩展边界变模糊 |
| 高级逻辑部分下沉 C 层 | 可由 Core Atoms 组合的高级逻辑应优先 Tcl 自举 | `incr/lappend` 在 `extcmd.c` 直接实现（L140-226）；`tcllib.tcl` 明示无需 Tcl 定义（L11, L43） | 自举层可移植性与可替换性下降 |
| 方言兼容未彻底消退 | 设计要求逐步消除方言行为 | `tcllib.tcl` 仍保留 `proc t_scmp` shim（L112-113） | 与标准 Tcl 命令面并存，存在历史包袱 |
| 双引号内 `[...]` 未执行 | 标准 Tcl 对齐应支持双引号内命令替换 | `tcl_str_interp` 仅处理转义与 `$var`（`src/tcl_core.c` L557, L583, L618），未处理 `[...]`；双引号分支调用该函数（L1819） | `"x=[cmd]"` 语义不对齐，影响脚本兼容性 |
| `if/while` 真值判定过简 | 条件判定要尽量对齐标准 Tcl | 当前使用“非空且首字节非`0`为真”（`src/tcl_core.c` L2316, L2419） | 对 `false/no/off` 等标准布尔字面量兼容不足 |
| `info commands` 功能不完整 | 错误处理与行为需向标准 Tcl 靠拢 | 无参分支返回空（`src/tcl_core.c` L2558-2560），`tcllib.tcl` 也 `return {}`（L158） | 与常见 Tcl 期望（列出命令）不一致 |
| 类型规约未完全贯彻 | 文档要求统一固定宽度类型，避免原生 `int` | `src/extcmd.c` 多处 `static int ...`（L6,16,21,66,101,140,196）；`src/tcl_core.c` 有 `for (int i...)`（L2013） | 风格与可移植性规约不一致 |
| “绝对无栈化”边缘冲突 | 强调无栈化 FSM | `mark_obj` 使用递归（`src/tcl_core.c` L213, L229-231） | 极深链路下仍有 C 调用栈风险 |

### 9.3 已对齐的关键项（确认）

| 项目 | 对齐情况 | 代码证据 |
|---|---|---|
| 零 Libc（核心层） | 已对齐（`src/tcl_core.c` 未包含标准库头） | `src/tcl_core.c` 顶部无 `#include <...>` |
| 静态 Arena + 双游标 | 已对齐 | `TclCtx` 中 `p_top/t_bot`，`tcl_alc_p/tcl_alc_t` 实现 |
| `tcl_eval` 无递归 FSM 主循环 | 已对齐 | `while (context->curr_f != TCL_NULL)` + 多状态 `switch`（`src/tcl_core.c`） |
| 相对偏移引用与 GC 紧凑化 | 已对齐 | `TO_PTR` 偏移模型、`tcl_gc` 中重定位与指针修正 |
