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

## 二、本会话完成的工作（最新 git log）

```
07493ba  fix: 修复 else body 被嵌套 if 语句覆盖的 Bug（用户手工提交）
ca033dd  docs: 添加新会话交接文档 session_handoff.md
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
| **`if-else` else 分支从不执行** | `tcl_core.c` | ST_IF_BODY 假分支忽略了 `tmp_roots[11]` 中的 else body；修复后创建 else 子帧 |
| 自举迁移：`lrange` | `tcllib.tcl` | 从 `tcl_core.c` 删除 C 实现，改为 `llength`+`lindex`+`while` Tcl 自举 |
| 自举迁移：`global` | `tcllib.tcl` | 从 `tcl_core.c` 删除 C 实现，改为 `uplevel 1 [list upvar #0 x x]` 自举 |
| 自举迁移：`info` | `extcmd.c` + `tcllib.tcl` | C 层提供 `__info_commands_core` 底层查询，Tcl 层封装 `proc info` |
| `info commands` 返回 TCL_NULL | `extcmd.c` | 找不到命令时应返回真空字符串对象，而非 TCL_NULL |
| **嵌套 if 覆盖 else_body（用户修复）** | `tcl_core.c` | 把 else_body 从 `tmp_roots[11]` 移到当前帧 `frame->result` 字段，避免全局竞争 |

---

## 三、当前测试状态（`bash build.sh`）

运行 `tests/tests.tcl`，**最新结果**（commit `07493ba`）：

| 分类 | 测试项 | 状态 |
|------|--------|------|
| 分类1: 核心原子指令 | 变量读写、算术运算、逻辑比较、命令替换 | ✅ PASS |
| 分类2: 过程与作用域 | 过程定义与调用、Uplevel 跨帧访问 | ✅ PASS |
| 分类3: 自举库 | for循环、min/max/abs、incr、info_exists、lappend、lrange、lreverse、lset、foreach | ✅ PASS |
| 分类4: 深度递归 | Fibonacci(10)、汉诺塔(4盘)、**8皇后求解** | ✅ PASS |
| 分类5: GC压力测试 | 字符串拼接压力、变量频繁创建销毁、对象移动稳定性 | ✅ PASS |
| 分类6: 错误捕获 | Catch 错误捕获 | ✅ PASS |
| 分类7: 历史回归 | Legacy: append 指令 | ✅ PASS |
| 分类7: 历史回归 | **Legacy: foreach 指令** | ❌ FAIL（`Error: 0`） |
| 分类7: 历史回归 | 后续测试未到达 | ❓ 未知 |

> **注**：8皇后通过是用户修复了嵌套 if else_body 覆盖 Bug 的直接成果。

---

## 四、下一步工作：Legacy foreach 失败分析

### 失败的测试用例（`tests/tests.tcl`）

```tcl
run_test "Legacy: foreach 指令" "test_foreach.tcl 内容" "输出应匹配" {
    set ml_l {a b c}
    set res_l {}
    foreach it_l $ml_l { append res_l $it_l }
    expr {[t_scmp $res_l {abc}] == 0}
}
```

### 根因分析（已通过调试确认）

**根本 Bug：空字符串参数（`{}`）在传入 proc 的 `args` 参数时被丢弃。**

测试验证：
```bash
proc test_args {first args} { puts "args=$args len=[llength $args]" }
test_args compare {} {abc}   # 输出：args= （空！{} 被吃掉了）
test_args compare hello {abc} # 输出：args=hello （正常）
```

当 `{}` 空字符串作为调用参数时，在 ST_EXPAND 阶段剥括号后变成空字符串，**然后这个空字符串没有被收集进 `args` 列表**。

这导致 `string compare {} {abc}` → proc string 被调用时 `args = {abc}`（只有一个元素），  
`lindex $args 0` 返回 `abc`，`lindex $args 1` 返回空，最终 `compare(abc, "") = -1`（而非期望的 `compare("", abc) = -1`），结果语义混乱。

### 调查方向

1. **参数绑定阶段**：`tcl_cmd_proc`（proc 调用时实际执行参数绑定的逻辑，在 tcl_core.c 中的 EXECUTE 阶段）是否在 `argv[i]` 为空字符串（不是 TCL_NULL，而是一个空字符串对象）时跳过了该参数？

2. **`args` 收集逻辑**：`proc` 调用时，如何将剩余参数打包成列表传入 `args` 变量？是否有跳过空字符串的逻辑？

3. **调试命令**：
```bash
gcc examples/demo.c -o tclsh_dbg -g -fsanitize=address
cat > /tmp/test_args.tcl << 'EOF'
proc test_args {first args} {
    puts "first=$first"
    puts "argc=[llength $args]"
    set i 0
    while {$i < [llength $args]} {
        puts "args[$i]=[lindex $args $i]"
        set i [expr {$i + 1}]
    }
}
test_args compare {} {abc}
EOF
timeout 5s ./tclsh_dbg /tmp/test_args.tcl 2>&1
```

---

## 五、已知待解决的其他问题（按优先级排序）

| 问题 | 严重度 | 说明 |
|------|--------|------|
| **空字符串参数 `{}` 在 proc args 中被丢弃** | 🔴 高 | `test_args compare {} {abc}` 中 `args` 为空，`{}` 没有进入 args 列表；是当前最优先 Bug |
| `[cmd]` 在双引号字符串内不执行 | 🟠 中 | `puts "result=[cmd]"` 中 `[cmd]` 被当字面量输出；需在 `tcl_str_interp` 中添加命令替换支持（需 yield）|
| `expr {$a == ""}` 与空字符串比较 | 🟠 中 | `expr {$r == ""}` 返回空而非 `1`；字符串 `==` 比较路径在 expr 三元归约中未实现 |
| `incr` 不支持增量参数（`incr i -1`） | 🟡 中 | 当前 `tcllib.tcl` 中 `incr` 只支持 `incr varname`，未支持 `incr varname N` |
| `lindex $list end` 不支持 `end` 关键字 | 🟡 中 | C 实现的 `lindex` 没有处理 `end` 字符串 |
| `lrange $list 0 end-1` 不支持 | 🟡 中 | `end-1` 算术表达式在 lrange 索引位置未实现 |
| `## 第 $round / 5 轮` 显示为空 | 🟢 低 | for 循环 body 内变量无法查找到外部变量（作用域链问题） |

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
1. 运行 bash build.sh，确认当前失败在「Legacy: foreach 指令」
2. 阅读 docs/session_handoff.md 第四节，了解根因（空字符串参数被丢弃）
3. 启动新一轮 A-B 协作，修复「空字符串参数 {} 在 proc args 中被丢弃」这个 Bug
4. 严格遵循 GEMINI.md 中的 A-B 协作框架（5.2节）

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
