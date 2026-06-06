# BareTcl

**The world's smallest industrial-grade Tcl interpreter core.**

BareTcl is an ultra-compact, stackless, and Libc-free Tcl interpreter designed for the most demanding bare-metal MCU environments (e.g., Renesas RH850, ARM Cortex-M). It prioritizes deterministic execution, memory safety, and architectural purity.

[中文版](./README.zh_CN.md) | [日本語版](./README.ja.md)

---

## Key Features

- **Industrial Reliability**: Built for mission-critical systems with a zero-dependency policy.
- **Stackless FSM**: Fully non-recursive execution engine. Eliminates C stack overflow risks.
- **Atomic 18-Core**: Only 18 fundamental instructions in the C core. All high-level logic (e.g., `for`, `incr`) is implemented in Tcl via self-bootstrapping.
- **Arena GC**: Compacting garbage collector ensures zero memory fragmentation.
- **Libc-Free**: Zero includes from standard C libraries. Compiles with `-ffreestanding -nostdlib`.
- **Fixed-Width Architecture**: Uses strictly defined `tcl_i32`, `tcl_u8` for cross-platform bit-level determinism.

---

## Technical Validation

BareTcl is engineered for complexity and stability, as demonstrated by the following milestones:

- **Full Self-Bootstrap**: The instruction set is so complete that standard commands like `for`, `foreach`, `incr`, and `switch` are implemented in Tcl itself and cross-compiled into the kernel.
- **8-Queens Solver**: Successfully executes the classic 8-Queens puzzle using deep recursion and complex list manipulations, proving logical maturity on bare-metal.
- **GC Stress Resistance**: Endured massive variable churn (10,000+ objects) in a minimal Arena without a single byte of leakage or fragmentation.
- **Cycle Efficiency**: Optimized state-transition loop designed for MCU efficiency, providing snappy execution even on low-clock hardware.

---

## Developer Guide

### 1. Verification & Testing
BareTcl includes a comprehensive industrial-grade test suite.
```bash
# Runs code generation, Libc-independence check, and full test suite
bash build.sh
```

### 2. Porting to Bare-Metal
To integrate BareTcl into your MCU project:
1. **Define HAL**: Implement `void tcl_hal_puts(const tcl_u8 *s)` to map output to your UART/Console.
2. **Initialize Arena**: Allocate a static buffer and call `tcl_init(buffer, size)`.
3. **Register Commands**: Call `tcl_register_c_cmd` for your hardware-specific procs.
4. **Load Core**: (Optional) Run the cross-compiled `tcl_load_bootstrap(ctx)` to enable high-level Tcl commands.

### 3. Extending BareTcl with C
Adding custom commands is straightforward:
```c
static tcl_i32 my_cmd(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    const tcl_u8 *arg1 = TO_PTR(ctx, argv[1]);
    // Your logic here...
    return TCL_OK;
}

// In your initialization:
tcl_register_c_cmd((const tcl_u8 *)"my_cmd", my_cmd);
```

---

## Quick Start (Linux Demo)

### REPL
```bash
./tclsh
> set x 10
> puts [expr $x * 2]
20
```

---

## License
Licensed under the Apache License, Version 2.0.
