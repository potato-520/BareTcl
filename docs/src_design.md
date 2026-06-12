# BareTcl 关键流程与功能改进设计文档 (src_design.md)

## 1. 设计目的

为了彻底解决 BareTcl 中由于变量作用域穿透、表达式求值时字符分割、以及特殊参数解析而导致的一系列回归和标准 Tcl 对齐问题，特制定此设计文档。本文档对以下关键改进进行架构定义与算法描述：
1. **形参变参 `args` 绑定机制**：解决 proc 的可变参数传递问题。
2. **`upvar` 逻辑作用域根绑定**：解决链接变量在 `uplevel` / 循环内失效或提前释放的问题。
3. **`tcl_str_interp` 方括号屏蔽设计**：解决双引号及表达式中 `[cmd $var]` 子命令变量被提前替换导致的分词裂变 Bug。
4. **`expr` 表达式中 `eq` 和 `ne` 运算符实现**：提供对标准 Tcl 字符串等于与不等比较的原生支持。
5. **`string length` 核心原初化实现**：将 `string length` 迁移至 C 原生指令，避免在 Tcl 自举层因使用 `llength` 产生非空格字符串长度计算错误。
6. **`lappend` / `incr` 自举回迁**：将可由脚本组合完成的列表追加与数值自增重新放回 `tcllib.tcl`，保持 Core Atoms 与 Bootstrap Library 的边界清晰。

---

## 2. 形参变参 `args` 绑定机制

### 2.1 物理背景
In Tcl 语法规约中，若过程的最后一个形参名为 `args`，则该过程为可变参数过程。所有多出的实参会被格式化为一个以空格分隔的列表并绑定至名为 `args` 的本地变量上；若没有多余实参，则 `args` 绑定为空字符串 `""`。

### 2.2 算法实现
在 `src/tcl_core.c` 的 `ST_EXECUTE` 阶段，绑定过程的形参和实参时：
1. 将原来基于实参个数循环（`while (arg_idx < frame->argc)`）的绑定逻辑改为**基于形参列表**循环。
2. 对于每个形参：
   - 提取形参名称，检测其是否为 `args`。
   - 探测形参列表后续是否还有其他形参。
   - 若当前形参为 `args` 且为最后一个形参：
     - 计算所有剩余实参的总长度（加上空格分隔符）。
     - 在 Arena 堆池中分配空间，拼接剩余实参（以空格分隔）形成一个 Tcl 列表。
     - 调用 `tcl_set_var` 将生成的列表字符串绑定到局部变量 `args`。
     - 立即终止参数绑定循环。
   - 若非 `args`：
     - 若实参游标尚未越界，绑定当前实参；若已越界，则绑定为 `TCL_NULL`。

---

## 3. `upvar` 逻辑作用域根绑定

### 3.1 物理背景
`upvar` 在指定的父级作用域中检索变量，并在当前作用域中创建一个链接变量指向目标变量。
在现有的 BareTcl 实现中，新分配的链接节点被直接链入 `context->curr_f` 指向的活跃栈帧的 `vars` 中。但在 `uplevel` 或是嵌套的 `while`/`if` 中，`curr_f` 是一个临时共享或执行帧。当该临时执行帧退出并被 Arena 回收时，创建在其中的链接节点一同丢失，而真正的调用者帧（proc 帧）却未能获得此链接变量，导致外部变量修改失败。

### 3.2 算法实现
在 `tcl_cmd_upvar` 中，创建链接节点时的插入目标栈帧不再固定为 `context->curr_f`。
应沿着 `context->curr_f` 开始的 `scope` 链（逻辑作用域链）向上回溯，直到找到 `scope == TCL_NULL` 的物理帧（即真正的非共享局部作用域根，如 proc 执行帧）。
```c
tcl_u32 insert_frame_offset = context->curr_f;
while (insert_frame_offset != TCL_NULL) {
    TclFrame *parent_frame = TO_PTR(context, insert_frame_offset);
    if (parent_frame->scope == TCL_NULL) {
        break;
    }
    insert_frame_offset = parent_frame->scope;
}
```
将分配的 `TclVar` 链接节点链入 `insert_frame_offset` 指向栈帧的 `vars` 中。这样链接变量的生命周期与所在的逻辑过程同宽，能跨越子执行帧正确留存。

---

## 4. `tcl_str_interp` 方括号屏蔽设计

### 4.1 物理背景
在 `expr` 命令中，如果存在 `[llength $list]` 这样含有空格的变量的命令替换，如果使用 `tcl_str_interp` 提前将整个表达式中的所有 `$list` 进行变量插值展开，会导致表达式变成 `[llength 20 30]`。当下级状态机执行该子脚本时，由于空格分割，`llength` 被迫接收了两个参数而报错或计数错误。
根据 Tcl 规范，方括号 `[...]` 内的脚本具有延迟执行语义。其内部的变量应在子帧求值时再行展开，而不应在父级表达式展开时被提前插值。

### 4.2 算法实现
在 `tcl_str_interp` 中，维护一个局部变量 `bracket_depth`（初始为 0）：
1. 字符扫描游标遇到普通字符时：
   - 若为 `[`，`bracket_depth` 自增。
   - 若为 `]`，`bracket_depth` 自减（下限为 0）。
2. 字符扫描游标遇到 `$` 时：
   - 若 `bracket_depth > 0`，则**不执行**变量提取与插值，原样将 `$` 字符写入输出缓冲区。
   - 若 `bracket_depth == 0`，则正常进行变量检索及值替换。
3. 转义字符 `\` 后引导的 `[` 和 `]` 不改变 `bracket_depth`。

---

## 5. `expr` 中 `eq` 和 `ne` 运算符支持

### 5.1 物理背景
标准 Tcl 提供了 `eq`（字符串全等） and `ne`（字符串不等）两个比较运算符，用以强制进行非数值的字符串比较（避免如 `0x10 == 16` 在字符串层面的误判）。

### 5.2 算法实现
1. 在 `tokenize_expr` 中，`eq` 和 `ne` 会被常规单词扫描逻辑单独识别为独立的 Token。
2. 在 `parse_equality` 中，修改运算符判定：
   - `is_eq` 匹配 `==` 或 `eq`。
   - `is_ne` 匹配 `!=` 或 `ne`。
   - 检测操作符是否为严格字符串比较 `is_strict_str`（即为 `eq` 或 `ne`）。
3. 如果 `is_strict_str` 为真，或者操作数两端不全是合法十进制数字，则强制调用 `t_scmp` 执行经典字符串匹配；只有在非严格比较且两端均为数字时，才执行 `t_atoi` 后进行数值相等性判断。

---

## 6. `string length` 核心原初化实现

### 6.1 物理背景
之前的 `string length` 在 `tcllib.tcl` 中使用 `llength $str` 自举包装实现。但这会导致不含空格的长字符串（如 `abcdefghij` 连续拼接）的 list 长度始终为 1。

### 6.2 算法实现
1. 在 `src/extcmd.c` 的 `tcl_cmd_string` 中：
   - 增加对子命令 `length` 的支持。
   - 验证参数数量并调用 `t_slen` 获取原始字符串字符数。
   - 格式化长度为十进制字符串写入 `context->result`。
2. 在 `src/tcllib.tcl` 的 `string` 命令包装中，将 `length` 子命令重新定位到 `__string_core length` 核心扩展调用。

---

## 7. 多优先级复合表达式归约求值设计

在标准 Tcl 中，`expr` 命令支持复杂的优先级嵌套与多级复合表达式求值（例如 `expr 3 * 4 * 5 * 6` 或 `expr (1 + 2) * 3`）。
之前的 BareTcl 实现中，由于受限于固定状态机的三元结构（即只期望左操作数、运算符、右操作数共 3 个 Token 的单一表达式），在面对超过 2 个运算符的复杂复合算术时，由于状态机无法自持复杂的嵌套递归，程序会退化为直接返回最后一个 Token 的字面值，无法执行正确求值。为此，需要引入无栈化的分轮优先级归约机制。

### 7.2 算法实现

在状态机 `ST_EXPR_REDUCE` 阶段，表达式求值主要包含：预处理清洗、括号配对消除（最内层优先）、以及扁平表达式优先级规约三个核心子阶段。

#### 7.2.1 括号保留与清洗（预处理）
在执行任何归约计算前，先对输入 Token 序列进行预处理和变量展开，并妥善保留与拆分括号 Token。为了解决如 `($result` 等由于无空格粘连导致左括号丢失及匹配失败的问题，实施以下策略：
1. **括号 Token 一次性预分配与 GC 保护**：
   在 `ST_EXPR_REDUCE` 的求值周期开始时，通过 `tcl_alc_p` 分配两个独立的括号 Token 字符串对象 `(` 和 `)`。
   - 分配得到的偏移量分别挂载在 `context->tmp_roots[12]` 与 `context->tmp_roots[13]` 中。
   - 这样做可以确保在后续复杂的清洗和分配（如字面量拷贝）触发 GC 时，两个括号对象的引用能被 GC 标记并自动修正偏移量，保持绝对的 GC 安全。
   - 在整个归约求值结束后（或异常退出时），必须将这两个临时根指针置回 `TCL_NULL` 以释放根保护。
2. **前导与尾部粘连括号的统计与拆分**：
   对于表达式帧中的每个原始 Token，线性统计其字符：
   - 统计 Token 前导的连续左括号 `(` 数量为 `leading_parentheses`。
   - 统计 Token 尾部的连续右括号 `)` 数量为 `trailing_parentheses`。
3. **拆分插入机制**：
    - **纯括号 Token 拆分**：若 `val_len > 0` 且 `leading_parentheses + trailing_parentheses == val_len`（即 Token 纯由括号组成，如独立的 `((` 或 `)`，且长度大于0），则不进行任何剥离，而是遍历其字符，根据每个字符类型向 `temp_argv` 中依次填入 `context->tmp_roots[12]`（左括号）或 `context->tmp_roots[13]`（右括号）的物理偏移量。必须保证 `val_len > 0` 约束以防止空字符串 `""` 被误过滤。
   - **普通操作数粘连拆分**：若 Token 包含操作数或变量且首尾粘连有括号（如 `($result` 或 `3)`）：
     1. 首先向 `temp_argv` 依次拷入 `leading_parentheses` 个左括号对象偏移量 `context->tmp_roots[12]`。
     2. 提取并清理中间不含括号的干净操作数部分（起始于 `leading_parentheses`，长度为 `val_len - leading_parentheses - trailing_parentheses`）。若干净部分以 `$` 开头（如 `$result`），则调用 `tcl_get_var` 原地展开获取其变量值；若为常规数字或字面量，且存在括号需要剥离（`clean_len < val_len`），则在进行 GC 安全的备份/恢复同步后，使用 `tcl_alc_p` 分配干净字符串空间拷贝并拷入。将干净操作数的偏移量填入 `temp_argv`。
     3. 最后向 `temp_argv` 依次拷入 `trailing_parentheses` 个右括号对象偏移量 `context->tmp_roots[13]`。

#### 7.2.2 扁平表达式规约辅助函数 (`tcl_reduce_flat_expr`)
针对不含任何圆括号的扁平子表达式（或消除括号后的主表达式），设计统一的扁平规约辅助函数。

*   **函数接口**：
    ```c
    static tcl_u32 tcl_reduce_flat_expr(
        TclCtx *context,
        tcl_u32 *base_argv,
        tcl_u32 *base_argc_ptr,
        tcl_u32 sub_start,
        tcl_u32 sub_len
    );
    ```
*   **功能说明**：在 `base_argv` 的 `[sub_start, sub_start + sub_len)` 区间内执行四轮优先级扫描（Level 3: 乘除模 -> Level 2: 加减 -> Level 1: 比较 -> Level 0: 逻辑与或），并在区间内就地前移覆盖完成规约，最终将求值结果写回 `base_argv[sub_start]` 处，同时更新总数 `*base_argc_ptr` 与子长度 `sub_len`。
*   **GC 安全保护同步逻辑**：在函数执行算术计算并调用 `tcl_alc_p` 申请空间存放结果前，必须将当前外层全量数组 `base_argv` 同步备份到 `frame->argv` 中并更新 `frame->argc = *base_argc_ptr`。在内存分配完毕后重新加载栈帧物理指针，并从 `frame->argv` 拷回 `base_argv`。这确保了在分配结果空间触发 Arena GC 时，所有在规约过程中的 Token 偏移量均能被 GC 标记和移动，防范悬空野指针。

#### 7.2.3 基于 while 循环的括号消除与配对校验
利用最内层括号优先规约的策略，通过 while 循环逐步消除圆括号对：
1. **寻找匹配括号对**：
   - 从左到右寻找第一个右括号 `)`。
   - 若找到右括号，则从该右括号所在位置向左反向搜索，定位最近的左括号 `(`。
   - **括号配对校验**：如果找到了右括号但左侧不存在任何左括号，或者 `while` 循环退出后整个数组中仍残留有未配对的左括号 `(`，则视为语法错误，设置错误状态并返回 `TCL_ERROR`。
2. **区间规约与消除**：
   - 锁定最内层括号对的子区间 `[left_idx + 1, right_idx)`，调用 `tcl_reduce_flat_expr` 对其执行优先级规约。
   - 规约完成后，子区间内只剩下一个结果 Token，此时将该结果 Token 覆盖到原左括号 `(` 所在位置 `temp_argv[left_idx]`，并将右括号 `)` 之后的元素整体前移 2 位覆盖右括号，将 `temp_argc` 递减 2。
3. **退出循环与最终规约**：
   - 重复执行上述匹配 and 消除，直到数组中不包含任何右括号 `)`。
   - 确认无残留左括号后，对剩下的扁平主表达式数组调用 `tcl_reduce_flat_expr(context, temp_argv, &temp_argc, 0, temp_argc)` 进行最后一轮总体规约，完成求值闭环。

### 7.3 物理存储结构与生存期管理
*   **无栈化控制**：为了契合 BareTcl 无 C 原生递归栈的核心约束，计算状态在自定义的 `TclFrame` 链表上保存，不进行 C 函数递归。
*   **本地临时工作区**：在 C 本地局部变量中维护防御型数组 `temp_argv`，限制长度为 `MAX_ARGS`，避免在 Arena 中反复构造列表产生的额外开销。
*   **生存期与垃圾回收同步**：因为算术结果（如 `t_itoa`）会产生新字符串偏移，在 Arena 分配期间，若触发垃圾回收（GC），所有堆上的指针会被重构。为了保护 `temp_argv` 中的所有偏移量不致因 GC 变成野指针，采用同步宏 `SAFE_ALC_P`，在分配前将本地所有的变量偏移量强制写回 `frame->argv`，让 GC 的 Trace 逻辑能够标记并更新它们，分配完成后再拷贝回 `temp_argv` 以继续归约，以此实现绝对的物理安全性。
*   **结果闭环**：当 4 轮归约全部完成后，`temp_argv` 中仅剩第 0 个元素，将其透传回 `context->result`。最后修改帧状态 `frame->state = ST_TOKENIZE`，促使在 `ST_RESUME` 阶段安全回收该子求值帧。

#### 7.3.1 tcl_cmd_expr 中的临时分配与 GC 危险期
在 `tcl_cmd_expr` 函数启动复合表达式求值时，会有两个关键内存分配点：
1. 处理以花括号包裹的表达式时，调用 `tcl_alc_p` 分配 `inner_offset` 用于剥离括号；
2. 调用 `tcl_alc_t` 在高地址栈区分配 `TclFrame` 子栈帧空间。

在这些分配执行期间，如果内存池空间不足，会触发垃圾回收（GC）。由于此时剥离括号的局部偏移量 `inner_offset` 和原始入参 `argument_values[1]` 尚处于 C 语言局部变量的临时堆栈中，不处于 GC 追溯的活跃引用根（如执行栈帧或全局变量表）中。若触发 GC，这些内存块会被搬迁并压紧，而 C 局部变量无法自动感知到物理偏移量的变化，从而导致它们变成指向已被搬移甚至回收的脏区域的野指针。

#### 7.3.2 双重 GC 根保护机制的实现
为了彻底保证上述分配过程的 GC 安全性，设计并实施了双重临时根保护机制：
*   **输入表达式保护**：在分配剥离花括号空间 `inner_offset` 之前，将输入参数偏移量 `argument_values[1]` 保护于垃圾回收保护根数组 `context->tmp_roots[0]` 中。分配完成后，从该临时根中重新读取已被 GC 修正过的新偏移量，然后进行物理指针对齐和剥括号拷贝。
*   **内层表达式保护**：在分配子栈帧 `sub_expression_frame_offset` 之前，将已清洗去括号的内层表达式偏移量 `inner_offset` 挂载到 `context->tmp_roots[0]` 中进行根保护。子帧成功分配后，再次从临时根中恢复并修正 `inner_offset`，最后安全地将其绑定至子栈帧的指令指针 `sub_frame_ptr->script` 上。
*   *注*：每次保护结束后，必须立即将 `context->tmp_roots[0]` 置为 `TCL_NULL` 以释放保护，防止在后续 GC 周期中产生错误的垃圾标记。

---

## 8. GC 标记阶段的绝对无栈化改造（mark_obj 非递归化）

### 8.1 问题背景
当前 `mark_obj` 采用 C 递归深度优先搜索。该实现虽可利用 `OBJ_MARK_BIT` 防止环路死递归，但在“超长变量链 + GC 触发”场景下，递归深度仍与对象图深度线性相关，违背 BareTcl “绝对无栈化”原则，并在极端平台栈预算下存在调用栈溢出风险。

### 8.2 设计目标
1. GC 标记阶段禁止任何 C 递归调用。
2. 不新增 Arena 常驻结构，不改变 `tcl_gc` 对外行为与回收结果。
3. 在不引入额外堆内存分配的前提下，保持对链式变量图（`name/val/next`，含 `upvar` 链接）的完整可达性标记。

### 8.3 核心算法：基于对象头 `forward` 字段的工作链迭代标记
在标记阶段，临时复用 `ObjHeader.forward` 作为“待处理工作链 next 指针”（LIFO 栈语义）：
1. 定义 `work_head`（初始 `TCL_NULL`）。
2. `push_if_unmarked(offset)`：
   - 若 `offset` 非法（`TCL_NULL` 或越界）则忽略；
   - 读取对象头，若已带 `OBJ_MARK_BIT` 则忽略；
   - 设置 `OBJ_MARK_BIT`；
   - 写入 `obj_header->forward = work_head`；
   - 更新 `work_head = offset`。
3. 先对根对象执行一次 `push_if_unmarked(root)`。
4. `while (work_head != TCL_NULL)` 循环：
   - 弹出 `current = work_head`；
   - 读取 `current_header->forward` 作为下一个 `work_head`；
   - 若 `current` 是 `OBJ_VAR_BIT` 对象，则对 `name/val/next` 依次执行 `push_if_unmarked(...)`。

### 8.4 正确性与 GC 阶段相容性
1. `forward` 字段在标记阶段后会在“迁移地址计算阶段”被完整重写，因此临时复用不会污染最终搬迁映射。
2. 由于 `push` 前先判 `OBJ_MARK_BIT`，环路与重复引用只会入链一次，时间复杂度保持 O(N)。
3. 算法仅使用固定数量局部变量，不消耗 C 调用栈深度，满足“绝对无栈化”约束。

### 8.5 约束与边界
1. 仅标记 Arena 低地址对象区（`HS <= offset < p_top`）；静态字符串与越界句柄被安全忽略。
2. 该改造不改变根集合定义（`result/g_vars/tmp_roots/活跃帧 script/vars/cond/body/argv`）。
3. 改造后 `tcl_gc` 的压缩、重定位、指针修复流程无需调整。
