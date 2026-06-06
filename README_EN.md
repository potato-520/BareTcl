# Tclsh.v2 - Industrial-Grade Eternal Embedded Scripting Engine

Tclsh.v2 is not a general-purpose Tcl interpreter; it is a high-performance, fully controlled, and theoretically "immortal" core designed specifically for **Industrial Bare-metal Microcontrollers (MCUs)**. Born from extreme exploration of environments like Renesas RH850/U2A, it aims to provide absolute deterministic dynamic script execution in highly resource-constrained environments (e.g., 64KB SRAM).

## Core Philosophy: The Three Supreme Principles

This project strictly adheres to the core mandates in the *Tclsh.v2 Creator's Code*, building an insurmountable logical barrier:

1. **Deterministic Execution**: All memory allocation, instruction jumps, and state transitions are O(1) or controlled O(N) complexity. Any unpredictable recursion or dynamic heap allocation is strictly forbidden.
2. **Absolute Physical Independence (Total Purity)**: The core is **100% Libc-Free**. It does not include `<stdio.h>`, `<stdlib.h>`, `<string.h>`, or `<stdint.h>`. All basic type definitions, memory cursor management, string processing, and mathematical operations are handcrafted to ensure every byte of logic is fully controlled.
3. **Stackless Immortality**: A 5-dimensional Finite State Machine (FSM) replaces the native C-language call stack. Even with recursion depths in the tens of thousands, C stack consumption remains constant. Combined with the "Protocol of Reincarnation" (GC), the system can automatically collapse and reconstruct when physical memory is exhausted, enabling indefinite continuous operation.

## Deep Technical Implementation

### 1. 5D Stackless FSM (Backbone)
The core deconstructs evaluation logic into five core "Crystal States":
*   **TOKENIZE**: Physical character stream parsing, supporting atomic slicing of nested braces `{}` and brackets `[]`.
*   **EXPAND**: Causal expansion. Handles variable substitution (`$`) and recursive command substitution (`[...]`) in-place without destroying parent frames.
*   **EXECUTE**: Atomic instruction projection. Invokes the 18 primal atomic instructions or custom stored procedures.
*   **RESUME**: Breakpoint awakening. After a sub-frame finishes, it precisely backfills the execution context and restores the parent frame.
*   **COND/LOOP**: Logical iteration. Achieves perpetual `while` loops through FSM recycling rather than native C loops.

### 2. The Protocol of Reincarnation (Compacting GC)
To prevent the monotonic growth of memory cursors from draining the Arena, the core implements a rigorous **Mark-Compact Protocol**:
*   **ObjHeader Addressing**: Each heap object carries 8 bytes of metadata, enabling linear physical traversal.
*   **Multiple Anchor Protection (Rooting)**: Uses the `tmp_roots` array to protect intermediate variables during generation, avoiding the "Unborn-yet-Dead" GC trap.
*   **Physical Address Redirection**: Performs a four-phase compaction algorithm without extra pointer arrays, dynamically correcting all relative offset references.

## 18 Primal Atomic Instructions

The following core instructions are fully implemented:
*   `set`: Definition and retrieval of variable dimensions.
*   `puts`: Projecting information to the outside world via the HAL layer.
*   `expr`: Ternary integer arithmetic and logical comparisons.
*   `if` / `while`: High-dimensional logical branches and loops.
*   `proc`: Creation of new execution boundaries (procedures).
*   `incr`: Unidirectional evolution of variables.
*   `return`: Backtracking of boundary energy.
*   `exit`: Complete termination of the interpreter universe.

## Build and Verification

```bash
# Start the industrial-grade automated build pipeline
# Includes: Libc-Free check (nm -u), full-coverage stress testing, recursion depth validation
bash build.sh
```

## Developer Warning

*   **Security Redline**: All automated test runs must be preceded by `timeout 10s` to prevent dimensional hangs due to script logic errors.
*   **Alignment Requirement**: All memory allocations strictly enforce 8-byte alignment to match the bus access requirements of high-performance MCUs.

---

*"Only by understanding the price of reincarnation can one master the eternal script." — Tclsh.v2 Specification*
