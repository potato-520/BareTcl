# BareTcl Design Specification (v5.0)

## 1. Introduction

### 1.1 System Objectives
The core objective of this project is to develop a highly reliable, ultra-compact, and fully stackless Tcl script interpreter core (BareTcl). Designed specifically for industrial-grade bare-metal microcontroller (MCU) environments (such as Renesas RH850/U2A), it aims to provide highly portable dynamic scripting capabilities while ensuring deterministic execution in extremely resource-constrained environments.

### 1.2 Design Vision
By stripping away all dependencies on modern operating systems (OS) and the standard C library (Libc), BareTcl builds a robust script execution environment within minimal SRAM. The system must eliminate the risks of stack overflow and out-of-memory (OOM) errors through its own memory governance mechanisms and finite state machine (FSM) model.

---

## 2. Architectural Constraints

To ensure absolute stability in embedded environments, the system must strictly adhere to four core hard constraints:

### 2.1 Zero-Libc Dependency
Core compilation must use `-ffreestanding -nostdlib`. Linking to `<stdio.h>`, `<stdlib.h>`, or `<string.h>` is strictly prohibited. All fundamental string operations and memory management must be handled by internal private libraries (e.g., `t_slen`, `t_mcpy`).

### 2.2 Static Arena Allocation
The system utilizes a single, contiguous static memory pool (Static Arena) for its entire lifecycle.
*   **Allocation Model**: Resource allocation is managed via dual cursors (`p_top` and `t_bot`), separating variable data from execution contexts (Frames).
*   **Physical Isolation**: Any dynamic heap allocation (`malloc`/`free`) is forbidden.

### 2.3 Fixed-Length Data Structures
Execution contexts (`TclFrame`) and variable definitions (`TclVar`) use fixed-length C structures. All internal references must use **relative offsets** based on the Arena's start address. Absolute physical pointers are prohibited to support memory compaction.

### 2.4 Absolute Stackless FSM
The core execution engine `tcl_eval` must not use recursive calls. The system is implemented as a finite state machine driven by a `while(1)` loop and a multi-dimensional `switch(state)`. All nested logic (e.g., command substitution, proc calls) is achieved through explicit context pushing and state suspension.

---

## 3. Core Execution Engine

### 3.1 5-State FSM Model
The interpreter's logic is flattened into five primary states handling the instruction lifecycle:
1.  **TOKENIZE**: Parses raw strings, identifying command and argument boundaries.
2.  **EXPAND**: Handles variable substitution (`$`) and command substitution (`[...]`).
3.  **EXECUTE**: Locates the instruction handler and executes it.
4.  **RESUME**: Restores the parent context and processes return results after a sub-context completes.
5.  **CLEANUP**: Reclaims temporary resources and handles exception bubbling.

---

## 4. Memory Management & GC

### 4.1 Arena Structure
*   **Low Address Area (`p_top`)**: Stores variable names, values, and `TclVar` structures.
*   **High Address Area (`t_bot`)**: Stores execution frames and the call chain.

### 4.2 Compacting GC Protocol
To prevent fragmentation and OOM, the system implements a compacting garbage collector:
1.  **Trigger**: Synchronous GC is triggered when `tcl_alc_p` cannot find sufficient contiguous space.
2.  **Marking**: Traverses the global variable table and the current execution chain to mark all reachable `TclVar` nodes and their associated strings.
3.  **Compacting**: Performs slide compacting, shifting live data blocks toward the start of the Arena.
4.  **Relocation**: Updates all `name` and `val` offsets and `next` pointers within `TclVar` nodes to maintain physical consistency.

---

## 5. Atomic Instruction Set

The kernel implements only 18 atomic instructions as a bootstrap foundation. Higher-level constructs (like `for`) are implemented in Tcl.

| Instruction | Architectural Role |
| :--- | :--- |
| `set` | Variable read/write with strict existence checks. |
| `proc` | Command registration, defining new scopes. |
| `if` / `while` | Basic control flow via FSM state transitions. |
| `expr` | Arithmetic and logic engine (supports lazy evaluation). |
| `return` / `error` | Scope termination and result/error propagation. |
| `break` / `continue` | Loop control status codes. |
| `eval` / `catch` | Dynamic code execution and exception trapping. |
| `upvar` / `uplevel` | Cross-scope memory access and execution. |
| `list` / `lindex` | List encapsulation and zero-allocation parsing. |
| `llength` / `lrange` | List length and slice extraction. |
| `unset` | Logical variable destruction. |

---

## 6. Engineering & Quality Control

### 6.1 Libc-Free Verification
The `build.sh` script performs an automated symbols check:
*   `gcc -c tcl_core.c -ffreestanding -nostdlib`
*   `nm -u tcl_core.o`
*   **Acceptance Criteria**: The output must be empty. Any external symbols (e.g., `memset`) result in an immediate build failure.

### 6.2 Industrial Validation
BareTcl is validated against a rigorous test suite, including:
*   Recursive algorithms (Fibonacci, Hanoi, 8-Queens).
*   GC stress tests (10,000+ object churn in 64KB Arena).
*   Nested command substitution and exception bubbling.

---

## 7. Basic Type System

To ensure consistency across architectures (RH850, ARM, x86), BareTcl strictly uses internal fixed-width types:

| Type | Physical Meaning |
| :--- | :--- |
| `tcl_u8` | Unsigned 8-bit integer (bytes/chars). |
| `tcl_i32` | Signed 32-bit integer (default Tcl integer). |
| `tcl_u32` | Unsigned 32-bit integer (offsets/counters). |
| `tcl_ptr` | Platform-dependent pointer-width integer. |

---

## 8. Project Organization

| Directory/File | Description |
| :--- | :--- |
| `src/` | **Engine Core**: `tcl_core.c` (FSM), `extcmd.c` (Extensions), `tcllib.tcl` (Bootstrap). |
| `tests/` | **Validation Suite**: `tests.tcl` and other stress/coverage scripts. |
| `tools/` | **Build Tools**: `tcl2c.py` for bootstrap cross-compilation. |
| `examples/` | **Integration Examples**: `demo.c` showing Linux-based integration. |
| `docs/` | **Documentation**: Multi-lingual design specs and user guides. |
| `build.sh` | **Automation**: Unified build and verification pipeline. |

---

## 9. Conclusion
BareTcl builds an impenetrable barrier for industrial-grade execution through its physical Arena, stackless FSM, and strict 18-rule atomic core.
