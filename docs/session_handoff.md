# BareTcl A-B 协作开发 —— 新会话交接文档

> 生成时间：2026-06-10（第二次更新）
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
我正在继续 BareTcl 解释器的 A-B 协作开发。请先阅读
docs/session_handoff.md 了解上一会话的工作成果和当前状态，
然后阅读 GEMINI.md 了解开发规约。

你作为 Master Agent，请：
1. 运行 bash build.sh，确认当前失败在「Legacy: foreach 指令」
2. 启动新一轮 A-B 协作，以「Legacy: foreach 指令失败」为首个目标
3. 严格遵循 GEMINI.md 中的 A-B 协作框架（5.2节）

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
