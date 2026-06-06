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

## Architectural Purity

BareTcl follows "The Great Purge" philosophy:
1. **The Core**: Only the minimal set of 18 atomic instructions required to build a language.
2. **The Extensions**: Platform-dependent logic (I/O, OS calls) is strictly separated in `extcmd.c`.
3. **The Bootstrap**: High-level commands are written in Tcl and integrated into the C core at build time.

---

## Quick Start

### Build & Test
```bash
# Requirements: gcc, python3
bash build.sh
```

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
