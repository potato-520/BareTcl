# BareTcl

**一款专注于资源极简、快速移植、裸机运行的 Tcl 解释器内核。**

BareTcl 不仅仅是一个解释器，它是一个裸机可用的 Tcl Shell。它旨在将你从繁琐无聊的串口收发逻辑、手动协议解析以及脆弱且难以维护的硬编码状态机中彻底解放出来。BareTcl 为裸机系统提供了一种近乎“零成本”的动态脚本集成体验。

[English](./docs/README.md) | [日本語版](./docs/README.ja.md)

---

## 零依赖，专为裸机“手搓轮子”

很多号称“嵌入式”的脚本引擎（如 Lua, JavaScript 等）在裸机移植时往往是开发者不切实际的幻想。它们大多依赖于标准 Libc（如 `malloc`/`free`）、操作系统调度或复杂的浮点运算库。对于普通开发者来说，在资源受限的裸机上移植它们不仅工作量巨大，且极难成功。那些需要 Libc 支援的“嵌入式”并不是真正的底层嵌入式。

BareTcl 坚持零依赖。为了实现真正的裸机运行，我们自主实现了所有核心逻辑，对系统环境**几乎没有任何要求**：

- **无需 `malloc` / 无需 `free`**：完全基于固定大小的静态 Arena 内存池。彻底杜绝堆碎片风险。
- **无需 `libc`**：纯净独立的 C 代码。不依赖 `<stdio.h>`、`<string.h>` 或任何标准库头文件。
- **无需 C 栈支持**：完全无栈化 (Stackless) 的状态机架构。无论 Tcl 脚本中的递归有多深，**永远不会**导致 C 语言调用栈溢出。
- **无需 `setjmp` / `longjmp`**：确定性的控制流，不依赖复杂的跳转机制。
- **无需复杂构建**：单文件核心，支持 `-ffreestanding -nostdlib` 编译。

---

## 为什么选择 BareTcl？（从此告别无聊的开发）

数十年来，嵌入式开发者一直被迫重复编写乏味的串口指令解析器。BareTcl 的出现终结了这一现状。

通过集成 BareTcl，你可以将普通的串口转化为一个强大的动态控制台。你不再需要为每一个功能硬编码解析逻辑，而是可以直接在运行中执行复杂的 Tcl 逻辑、循环和条件判断。它是真正的**嵌入式瑞士军刀**，让静态的 MCU 瞬间拥有灵活的编程能力。

---

## 核心特性

- **工业级可靠性**：专为任务关键型系统构建，坚持零依赖策略。
- **智能交互外壳 (Shell)**：内置轻量级行编辑器，支持退格、方向键光标移动、最近 16 条历史命令切换，以及智能多行输入（自动识别未闭合的花括号）。
- **原子 18 指令核心**：C 内核仅包含 18 条原子指令。所有高级逻辑（如 `for`, `incr`, `foreach`）均通过 Tcl 脚本自举实现。
- **紧凑化 GC**：采用移动/紧凑化垃圾回收器，确保 Arena 空间在万级对象更迭下依然保持零碎片。
- **位级确定性**：严格使用 `tcl_i32`, `tcl_u8` 等固定宽度类型，确保跨平台行为一致。

---

## 强悍的性能与可靠性验证

- **自举完备性 (Self-Bootstrap)**：核心指令集高度完备，标准库完全由 Tcl 自身构建并静态集成。
- **8皇后解算 (8-Queens)**：在裸机环境下完美运行 8 皇后算法，证明了其处理深度递归与复杂列表的能力。
- **GC 极限压力**：在仅 64KB 的 Arena 空间内承受了数万次变量 churn 测试，零泄露，零碎片。
- **ESP32 芯片级原生移植**：在 ESP32-C3 (ESP-IDF 架构) 上实现原生 C 全功能移植（参见 [ESP32_ports](file:///home/chenming/BareTcl/ESP32_ports)）。包含非阻塞串口 REPL、VFS 无缓冲实时输入、软硬件看门狗安全让出（`TCL_YIELD_HOOK`）以及物理 GPIO (继电器/按键) 的 C 扩展绑定。详情参见 [ESP32 移植设计与踩坑笔记](./docs/ESP32移植.md)。

  ![](./docs/ESP32成功运行BareTcl.mp4)

---

## 开发人员指南

### 1. 裸机移植步骤
1. **实现 HAL 层**：提供底层输出接口 `void tcl_hal_puts(const tcl_u8 *s)`，用于 Shell 显示和日志。
2. **初始化内存池**：准备一块静态内存（如 `char arena[64KB]`），调用 `tcl_init(arena, size)`。
3. **获取上下文**：BareTcl 的上下文结构体 `TclCtx` 位于内存池头部，定义 `TclCtx *ctx = (TclCtx *)arena`。
4. **加载自举库（推荐）**：调用 `tcl_load_bootstrap(ctx)`，以支持 `for`, `foreach` 等高级语法。
5. **驱动 Shell**：调用 `shell_init(&sh)` 后，将串口收到的每个字节传入 `shell_handle_char(&sh, byte, "> ")`。
6. **解析执行**：当 Shell 返回 `1` 时，调用 `tcl_eval(ctx, sh.line)`。

### 2. 使用 C 语言扩展
```c
static tcl_i32 my_hardware_cmd(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    // 在这里实现直接控制硬件的逻辑...
    return TCL_OK;
}
tcl_register_c_cmd((const tcl_u8 *)"hw_ctrl", my_hardware_cmd);
```

---

## 快速开始 (Linux 演示)
```bash
# 自动编译并启动具备 Raw Mode 支持的高级 Shell
bash build.sh
./tclsh
```

---

## 开源协议
采用 Apache License, Version 2.0 协议授权。
