# BareTcl 🚀

**全球最精简的工业级 Tcl 解释器内核。**

BareTcl 是一款专为极致严苛的**裸机 (Bare-metal)** MCU 环境（如 Renesas RH850, ARM Cortex-M）设计的解释器。它完全**无栈化 (Stackless)** 运行，**不依赖 Libc**，强调执行的确定性、内存安全与架构纯净度。

[English](./README.md) | [日本語版](./README.ja.md)

---

## 🌟 核心特性

- **🛡️ 工业级可靠性**：专为任务关键型系统构建，坚持零依赖策略。
- **🏗️ 无栈化状态机 (FSM)**：完全非递归执行。从根源杜绝 C 栈溢出。
- **💎 原子 18 指令核心**：C 内核仅包含 18 条最基础指令。所有高级逻辑（如 `for`, `incr`）均由 Tcl 实现。
- **🧠 Arena GC**：采用移动/紧凑化垃圾回收器，确保内存零碎片。
- **🔌 完全脱离 Libc**：不包含任何标准 C 库头文件。支持 `-ffreestanding -nostdlib` 编译。
- **⚡ 固定宽度架构**：严格使用 `tcl_i32`, `tcl_u8` 等类型，确保跨平台的位级确定性。

---

## 🏗️ 架构纯净哲学

BareTcl 遵循 **“大清洗 (The Great Purge)”** 哲学：
1. **核心 (Core)**：仅保留构建一门语言所需的 18 条原子指令。
2. **扩展 (Extensions)**：平台相关逻辑（I/O, OS 调用）严格隔离在 `extcmd.c` 中。
3. **自举 (Bootstrap)**：高级命令由 Tcl 编写，并交叉编译进 C 内核。

---

## 🚀 快速开始

### 构建与测试
```bash
# 环境要求: gcc, python3
bash build.sh
```

### 尝试 REPL
```bash
./tclsh
> set x 10
> puts [expr $x * 2]
20
```

---

## 📄 开源协议
采用 Apache License, Version 2.0 协议授权。
