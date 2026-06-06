# BareTcl Design Specification (v5.0)

## 1. Introduction

### 1.1 System Objectives
The core objective of this project is to develop a highly reliable, ultra-compact, and fully stackless Tcl script interpreter core (BareTcl). Designed specifically for industrial-grade bare-metal microcontroller (MCU) environments (such as Renesas RH850/U2A), it aims to provide highly portable dynamic scripting capabilities while ensuring deterministic execution in extremely resource-constrained environments.

---

## 2. Architectural Constraints

### 2.1 Zero-Libc Dependency
Core compilation must use `-ffreestanding -nostdlib`. Linking to `<stdio.h>`, `<stdlib.h>`, or `<string.h>` is prohibited.

### 2.2 Static Arena Allocation
The system utilizes a single, contiguous static memory pool (Static Arena) for its entire lifecycle.
*   **Allocation Model**: Managed via dual cursors (`p_top` and `t_bot`), separating variable data from execution contexts (Frames).
*   **Physical Isolation**: Any dynamic heap allocation (`malloc`/`free`) is forbidden.

### 2.3 Absolute Stackless FSM
The core engine `tcl_eval` must not use recursive calls. Nested logic is achieved through explicit context pushing.

---

## 3. The BareTcl Shell Component

To support advanced interactive capabilities in bare-metal environments, BareTcl includes a specialized **Line Editor Shell**.

### 3.1 Architecture
The shell is implemented as a standalone state machine (`src/baretcl_shell.c`) that processes raw bytes from the UART. It maintains its own buffer and history without external memory allocation.

### 3.2 ANSI Escape State Machine
The shell parses standard ANSI escape sequences to provide professional CLI features:
*   **Navigation**: Up/Down for command history, Left/Right for in-line cursor movement.
*   **Editing**: Backspace and Delete handling via terminal-aware control sequences.
*   **Multi-line**: Intelligent brace tracking (`{}`) to automatically toggle between direct execution and line-continuation modes.

---

## 4. Memory Management & GC

### 4.1 Arena Structure
*   **Low Address Area (`p_top`)**: Stores variable names, values, and `TclVar` structures.
*   **High Address Area (`t_bot`)**: Stores execution frames and the call chain.

### 4.2 Compacting GC Protocol
1.  **Marking**: Traverses global and local variables to mark reachable objects.
2.  **Compacting**: Slide-compacts live data blocks toward the start of the Arena.
3.  **Relocation**: Updates relative offsets to maintain physical consistency.

---

## 5. Atomic Instruction Set
The kernel implements 18 fundamental instructions (e.g., `set`, `proc`, `if`, `while`, `eval`, `catch`). Higher-level constructs are implemented in the Tcl layer.

---

## 6. Engineering & Quality Control

### 6.1 Libc-Free Verification
The `build.sh` script enforces a zero-external-symbol policy via `nm -u` analysis.

### 6.2 Industrial Validation
BareTcl is validated against recursive algorithms (Fibonacci, 8-Queens) and GC stress tests (10,000+ object churn).

---

## 7. Project Organization

| Directory/File | Description |
| :--- | :--- |
| `src/` | **Engine Core**: `tcl_core.c`, `extcmd.c`, `baretcl_shell.c`, `tcllib.tcl`. |
| `tests/` | **Validation Suite**: Industrial stress and coverage scripts. |
| `tools/` | **Build Tools**: Bootstrap cross-compilation scripts. |
| `examples/` | **Integration Examples**: Linux-based demo showing terminal Raw Mode. |
| `docs/` | **Documentation Center**: Multi-lingual design and user guides. |

---

## 8. Conclusion
BareTcl builds an impenetrable barrier for industrial-grade execution through its physical Arena, stackless FSM, and integrated smart shell.
