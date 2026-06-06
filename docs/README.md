# BareTcl

**The world's smallest industrial-grade Tcl interpreter core.**

BareTcl is not just an interpreter; it is a **precision weapon** for embedded engineers. Designed to liberate you from the tedious drudgery of manual UART protocol parsing and fragile ad-hoc state machines, BareTcl offers the ultimate "plug-and-play" experience for bare-metal systems.

[中文版](./README.zh_CN.md) | [日本語版](./README.ja.md)

---

## The "Zero-Requirement" Philosophy

BareTcl is engineered to be the easiest interpreter you will ever port. It demands **nothing** from your system environment:

- **No `malloc` / No `free`**: Uses a fixed-size static Arena. Zero heap fragmentation risks.
- **No `libc`**: Pure standalone C code. No `<stdio.h>`, No `<string.h>`, No headers at all.
- **No C-Stack dependency**: Fully stackless FSM architecture. It will **never** cause a C stack overflow, no matter how deep the Tcl recursion.
- **No `setjmp` / `longjmp`**: Deterministic control flow without complex jump mechanics.
- **No Complex Build**: A single-file core that compiles with `-ffreestanding -nostdlib`.

---

## Why BareTcl? (The Ultimate Embedded Weapon)

For decades, embedded developers have been trapped in a loop of writing "boring" serial command parsers. BareTcl ends this era. 

By integrating BareTcl, you transform your serial port into a powerful dynamic terminal. Instead of hard-coding every command, you gain the ability to execute complex logic, loops, and conditional sequences on-the-fly. It is the **God-tier tool** (神兵利器) that turns a static MCU into a flexible, programmable powerhouse.

---

## Key Features

- **Industrial Reliability**: Built for mission-critical systems with a zero-dependency policy.
- **Atomic 18-Core**: Only 18 fundamental instructions. All high-level logic (e.g., `for`, `incr`, `foreach`) is implemented in Tcl via self-bootstrapping.
- **Compacting GC**: A moving garbage collector ensures your memory Arena remains optimized even during massive object churn.
- **Fixed-Width Architecture**: Strictly defined `tcl_i32`, `tcl_u8` for cross-platform bit-level determinism.

---

## Battle-Tested Performance

- **Full Self-Bootstrap**: The instruction set is so complete that the core language constructs itself.
- **8-Queens Solver**: Successfully handles deep recursion and complex list manipulations on bare-metal.
- **GC Stress Resistance**: Endured 10,000+ object cycles in a tiny 64KB Arena without a single byte of leakage.

---

## Developer Guide

### 1. Porting to Bare-Metal
1. **Implement HAL**: Provide a simple `void tcl_hal_puts(const tcl_u8 *s)`.
2. **Initialize Arena**: Call `tcl_init(static_buffer, size)`.
3. **Load Logic**: Call `tcl_load_bootstrap(ctx)` to enable the high-level Tcl library.

### 2. Extending with C
```c
static tcl_i32 my_hardware_cmd(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    // Direct hardware control logic here...
    return TCL_OK;
}
tcl_register_c_cmd((const tcl_u8 *)"hw_ctrl", my_hardware_cmd);
```

---

## Quick Start (Linux Demo)
```bash
bash build.sh
./tclsh
```

---

## License
Licensed under the Apache License, Version 2.0.
