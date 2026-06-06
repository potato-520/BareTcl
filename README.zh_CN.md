# BareTcl

**全球最精简的工业级 Tcl 解释器内核。**

BareTcl 是一款专为极致严苛的裸机 (Bare-metal) MCU 环境（如 Renesas RH850, ARM Cortex-M）设计的解释器。它完全基于无栈化 (Stackless) 状态机运行，不依赖 Libc，强调执行的确定性、内存安全与架构纯净度。

[English](./README.md) | [日本語版](./README.ja.md)

---

## 核心特性

- **工业级可靠性**：专为任务关键型系统构建，坚持零依赖策略。
- **无栈化状态机 (FSM)**：完全非递归执行，从根源杜绝 C 栈溢出风险。
- **原子 18 指令核心**：C 内核仅包含 18 条最基础指令。所有高级逻辑（如 `for`, `incr`）均通过 Tcl 脚本自举实现。
- **Arena 紧凑化 GC**：采用移动/紧凑化垃圾回收器，确保内存零碎片。
- **完全脱离 Libc**：不包含任何标准 C 库头文件。支持 `-ffreestanding -nostdlib` 编译。
- **固定宽度架构**：严格使用 `tcl_i32`, `tcl_u8` 等类型，确保跨平台的位级确定性。

---

## 性能与可靠性验证

BareTcl 经过严苛的测试，足以应对复杂逻辑与高压力环境：

- **自举完备性 (Self-Bootstrap)**：内核指令集高度完备，`for`、`foreach`、`incr` 等高级指令全部由 Tcl 脚本编写并集成至内核。
- **8皇后挑战 (8-Queens)**：完美运行经典的 8 皇后算法，通过了深层递归与复杂列表操作的验证，证明了逻辑引擎在裸机环境下的成熟度。
- **GC 极限压力测试**：在极小的 Arena 空间内承受了数万次变量创建与销毁的冲击，实现零泄露、零碎片。
- **高效执行**：针对 MCU 周期优化的状态机跳转逻辑，在低主频硬件上依然能提供流畅的脚本执行体验。

---

## 开发人员指南

### 1. 验证与测试
BareTcl 附带了完整的工业级测试集，确保内核的稳健。
```bash
# 执行代码生成、Libc 独立性校验及全量测试
bash build.sh
```

### 2. 移植到裸机环境
将 BareTcl 集成到 MCU 项目非常简单：
1. **实现 HAL 层**：实现 `void tcl_hal_puts(const tcl_u8 *s)` 函数，将输出映射到 UART 或控制台。
2. **初始化 Arena**：分配一段静态内存作为 Arena，调用 `tcl_init(buffer, size)`。
3. **注册硬件指令**：调用 `tcl_register_c_cmd` 注册与硬件相关的 C 函数。
4. **加载引导库**：（可选）调用 `tcl_load_bootstrap(ctx)` 以启用 `for`, `incr` 等高级 Tcl 指令。

### 3. 使用 C 语言扩展命令
你可以轻松扩充 Tcl 的功能：
```c
static tcl_i32 my_cmd(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    const tcl_u8 *arg1 = TO_PTR(ctx, argv[1]);
    // 这里实现你的逻辑...
    return TCL_OK;
}

// 在初始化流程中：
tcl_register_c_cmd((const tcl_u8 *)"my_cmd", my_cmd);
```

---

## 快速开始 (Linux 演示)

### 交互式 REPL
```bash
./tclsh
> set x 10
> puts [expr $x * 2]
20
```

---

## 开源协议
采用 Apache License, Version 2.0 协议授权。
