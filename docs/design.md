# Tclsh.v2 Miniature Core Software Design Specification (v5.1)

[中文版](./design.zh_CN.md) | [日本語版](./design.ja.md)

## 1. Introduction

### 1.1 System Goals
The core objective of this project is to develop a highly reliable, extremely minimalist, and entirely stackless Tcl script interpreter core (Tclsh.v2). Designed specifically for industrial-grade bare-metal Microcontroller Unit (MCU) environments (such as Renesas RH850/U2A), this core aims to provide highly portable dynamic scripting capabilities while ensuring deterministic execution in extremely resource-constrained settings.

### 1.2 Design Vision
By stripping away all dependencies on modern Operating Systems (OS) and the Standard C Library (Libc), we build a robust script execution environment within a minimal SRAM footprint. The system must employ its own memory governance mechanism and Finite State Machine (FSM) model to completely eliminate the risks of stack overflow and memory exhaustion (OOM).

---

## 2. Architectural Constraints

To ensure absolute stability in embedded environments, the system must strictly adhere to the following four hard constraints:

### 2.1 Zero-Libc Dependency
The core must be compiled with the `-nostdlib` option. Linking to `<stdio.h>`, `<stdlib.h>`, or `<string.h>` is strictly prohibited. All fundamental string operations (such as length calculation and copying) and memory management must be handled by private libraries implemented within the core (e.g., `t_slen`, `t_mcpy`).

### 2.2 Static Arena Allocation
The system utilizes a single, contiguous static memory pool (Static Arena) for full lifecycle management.
*   **Allocation Model**: Resources are allocated via bidirectional cursors (`p_top` and `t_bot`), managing variable data and execution contexts (Frames) respectively.
*   **Physical Isolation**: Any form of dynamic heap memory allocation (`malloc`/`free`) is prohibited.

### 2.3 Fixed-Length Data Structures
Execution contexts (`TclFrame`) and variable definitions (`TclVar`) use fixed-length C structures. All internal references must use **relative offsets** based on the physical starting address of the Arena. Absolute physical memory pointers are prohibited to support future memory compaction.

### 2.4 Absolute Stackless FSM
The core execution engine `tcl_eval` is strictly forbidden from using recursive calls. The system must be implemented as a Finite State Machine (FSM) driven by a `while(1)` loop and a multi-dimensional `switch(state)`. All nested logic (e.g., command substitution, subroutine calls) is realized through explicit context pushing (Frame Pushing) and state suspension.

---

## 3. Core Execution Engine

### 3.1 5-State FSM Model
The interpreter's core parsing logic is flattened into five primary states to handle the instruction stream lifecycle:
1.  **TOKENIZE**: Parses the raw string, identifying command and argument boundaries.
2.  **EXPAND**: Handles variable substitution (`$`) and command substitution (`[...]`).
3.  **EXECUTE**: Locates the instruction handler function and executes it.
4.  **RESUME**: After a sub-context completes execution, restores the parent's execution context and processes the return result.
5.  **CLEANUP**: Reclaims temporary resources and handles exception status code bubbling.

### 3.2 Asynchronous Handling of Command Substitution
To prevent stack crashes caused by deep nesting, when a command substitution `[...]` is encountered, the current context is marked as `ST_SUSPEND`, and the sub-command is wrapped into a new `TclFrame` and pushed into the Arena. The main loop restarts parsing the sub-context until a result is returned.

### 3.3 Alignment with Standard Tcl
In subsequent refactoring, the system must gradually phase out non-standard "dialect" behaviors:
*   **Conditional Evaluation Refactoring**: `if` and `while` must support standard Tcl expression syntax (i.e., `if {$a < 10}`), rather than just script blocks.
*   **Command Completion**: Implement standard commands such as `string compare` and `string length` to replace internal functions like `t_scmp`.
*   **Error Handling Alignment**: Error messages and status codes must comply with standard Tcl specifications.

---

## 4. Memory Management & Compacting GC

### 4.1 Memory Pool Structure
*   **Lower Address Area (`p_top`)**: Stores variable names, values, and `TclVar` structures.
*   **Higher Address Area (`t_bot`)**: Stores execution stack frames (`TclFrame`) and the call chain.

### 4.2 Compacting Garbage Collection (GC) Protocol
Due to the string-based nature of Tcl, frequent variable updates lead to fragmentation and space exhaustion in the `p_top` area. The system introduces a "Samsara Law" reclamation mechanism:
1.  **Trigger Mechanism**: Synchronous GC is triggered when `tcl_alc_p` fails to allocate contiguous space.
2.  **Marking Phase**: Performs a deep traversal of the global variable table and all active local variables in the execution chain, marking all reachable `TclVar` structures and their associated string offsets.
3.  **Compacting Phase**: Executes physical address shifting (Slide Compacting), squeezing surviving data blocks toward the start of the Arena.
4.  **Relocation Phase**: Since relative offsets are used, the GC process must synchronously update all `name`, `val` pointers, and `next` chain pointers within `TclVar` to ensure physical consistency of references.

### 4.3 Top-Level Variable Lifecycle and Global Scope Layout

In BareTcl's stackless framework, the interpreter uses `TclFrame` to maintain the call stack. The top-level frame is popped and logically released (the `t_bot` cursor is rolled back) upon normal completion of each `tcl_eval` run. To ensure that top-level variables persist across `tcl_eval` calls in interactive mode, a dedicated global variable management mechanism is established:

1. **Top-Level Environment Determination and Global Mounting**
   During variable creation/update (`tcl_set_var`), the system traces back through the logical scope chain. If the root frame found does not have a procedure flag (`!(root_frame->flags & FRAME_IS_PROC)`), the context is deemed top-level (e.g., global scripts or shared local scope commands like `if`/`while`/`eval`). In this case, the target frame is set to `TCL_NULL`, causing the variable to be linked into the global variable table `g_vars`.

2. **Lifecycle Persistence**
   Variables mounted in `g_vars` persist throughout the interpreter's lifetime and do not belong to any specific `TclFrame`. These variables remain safe after `tcl_eval` finishes and the top-level frame is popped. During GC, the marking step starts from `context->g_vars`, ensuring global variables are correctly moved and rebound during compaction and relocation.

3. **Cross-Scope Reference (upvar #0) Optimization**
   The absolute level indicator `#0` in `upvar` represents the global scope. When executing `upvar #0`, the system bypasses the physical call frame chain and directly designates the target frame as `TCL_NULL`. This associates the new local alias directly with the global variable table `g_vars`, eliminating physical dependency on potentially released top-level frames.

---

## 5. 3-Tier Instruction Architecture

To maintain extreme core purity and portability, instructions are strictly divided into three tiers:

1.  **Core Atoms**: Implemented internally in `tcl_core.c`. These include only absolute core operations related to FSM logic, memory management, and variable scoping. **No commands interacting with the system environment, hardware I/O, or OS are included.**
2.  **Environment Extensions**: Implemented in `extcmd.c`. Commands requiring C support but related to the environment (e.g., output, exit, hardware interaction) belong here and are injected via `tcl_register_c_cmd`.
3.  **Bootstrap Library**: Implemented in `tcllib.tcl`. Advanced logic that can be composed from Core Atoms (e.g., `for`, `foreach`) must be implemented entirely via Tcl script bootstrapping; implementation at the C layer is prohibited.

### 5.1 Core Primitive Instruction Set (tcl_core.c)
The following instructions form the minimal and complete logical foundation of BareTcl:

| Instruction | Description and Architectural Role |
| :--- | :--- |
| `set` | Basic variable read/write with strict existence checks. |
| `proc` | Command registration, defining new function scopes. |
| `if` / `while` | Basic control flow implemented via FSM jumps. |
| `expr` | Integer arithmetic and logical expression engine. |
| `return` | Explicit boundary termination and data return. |
| `break` / `continue` | Loop control status code propagation. |
| `eval` | Re-parsing and execution for dynamic code injection. |
| `list` | String-to-list encapsulation. |
| `catch` / `error` | Exception defense mechanism supporting capture and propagation. |
| `upvar` / `uplevel` | Cross-scope memory access and execution (key to global scope). |
| `llength` / `lindex` | Basic list operations emphasizing zero-allocation parsing. |
| `unset` | Variable destruction to assist memory management. |

### 5.2 Environment Extensions (extcmd.c)
These commands are OS-dependent and must exist as extensions outside the core:

| Instruction | Description and Architectural Role |
| :--- | :--- |
| `puts` | Hardware Abstraction Layer (HAL) output, usually mapped to UART or stdout. |
| `exit` | Terminates the FSM and exits the program (or reboots the MCU). |

---

## 6. Risk Mitigation

1.  **Lazy Evaluation Issues**: The `expr` command must support native parsing of expressions enclosed in braces `{}` to prevent premature variable substitution by the outer parser.
2.  **Silent Failure Risks**: Any variable lookup failure or logical error must generate a `TCL_ERROR`. Returning empty values or failing silently is prohibited to ensure the effectiveness of the `catch` command.
3.  **Memory Black Holes**: When handling long string concatenations (e.g., `lappend`), discarded intermediate blocks must be cleaned up promptly. Forced GC should be triggered if necessary to prevent SRAM overflow.

---

## 7. Project Structure & Engineering

The project consists of the following core components to ensure integrity from the low-level core to high-level bootstrap logic and automated verification:

| Filename | Type | Description |
| :--- | :--- | :--- |
| `tcl_core.c` | C Source | **Interpreter Core**: Contains the type system, FSM, Arena management, and 12 core atoms. Zero Libc dependency. |
| `extcmd.c` | C Source | **Environment Extensions**: Implements platform/OS-specific commands like `puts` and `exit`. |
| `baretcl_shell.c` | C Source | **Interactive REPL Line Editor**: Provides zero-allocation, lightweight line editing and history management. |
| `demo.c` | C Source | **Test Shell**: Integrates the core, extensions, and shell via `#include` for interactive REPL and file execution on Linux. |
| `design.md` | Doc | **Design Specification**: Defines architecture, hard constraints, and standards. |
| `tcllib.tcl` | Tcl Script | **Bootstrap Library**: Implements `for`, `foreach`, `switch`, etc., using only Core Atoms. |
| `tcl2c.py` | Python Script | **Conversion Tool**: Converts `tcllib.tcl` into a C byte array for automatic loading during initialization. |
| `tests.tcl` | Tcl Script | **Validation Suite**: Automated regression tests aligned with standard Tcl. |
| `build.sh` | Shell Script | **CI/CD**: Handles code generation, compilation, and automated test verification. |

---

## 8. C API & Embedding Interface

To seamlessly integrate the core into bare-metal environments, the following standardized C interfaces are provided:

### 8.1 System Initialization
*   **`void tcl_init(void *arena, int size)`**:
    *   Receives an externally allocated static SRAM buffer (Arena), initializes cursors, and preloads core atoms.
*   **`void tcl_register_c_cmd(const char *name, Tcl_CmdProc proc)`**:
    *   Allows registration of new atomic or extension commands at the C level.

### 8.2 Execution and Interaction
*   **`int tcl_eval(const char *script)`**:
    *   Starts FSM parsing and execution. Returns `TCL_OK`, `TCL_ERROR`, or `TCL_EXIT`.
*   **`const char *tcl_get_result(void)`**:
    *   Retrieves the final result string after FSM completion.

### 8.3 Watchdog & Multi-tasking Yield Hook
To adapt BareTcl to preemptive multi-tasking operating systems (e.g., FreeRTOS) or watchdog-controlled (WDT) environments, the core provides a step-level micro-delay hook:
*   **`TCL_YIELD_HOOK()`**:
    *   **Function**: Invoked at high frequency at the beginning of the FSM main loop in `tcl_core.c`.
    *   **Default Configuration**: If undefined, it resolves to a no-op `((void)0)`, causing zero performance overhead or footprint impact on PC and general compilation. On embedded platforms with concurrent tasks (such as ESP32), it can be defined in the platform wrapper to execute `vTaskDelay` (e.g., every 1000 FSM iterations) to allow the OS to feed the watchdog and handle real-time tasks smoothly.

### 8.4 Interactive REPL Terminal Configuration
In the [baretcl_shell.c](file:///home/chenming/BareTcl/src/baretcl_shell.c) module, to accommodate non-standard raw serial text streams that do not support ANSI escape codes (e.g., Arduino IDE Serial Monitor), a compatibility control variable is provided:
*   **`baretcl_use_ansi`** (Global control variable):
    *   **Value = 1** (Default): Enables full ANSI escape sequence rendering for cursor movement, inline insert/delete, and terminal clearing.
    *   **Value = 0** (Fallback mode): Disables all `\x1b` ANSI escapes and falls back to a clean physical backspace and space sequence (`\b \b`) to delete characters, preventing control code garbage output on basic monitors.

---

## 9. Basic Type System

To ensure physical consistency across different architectures (e.g., RH850, ARM, x86), the project prohibits the direct use of native C types (e.g., `int`, `long`, `char`). Instead, fixed-width types are used:

| Custom Type | Physical Meaning | Note |
| :--- | :--- | :--- |
| `tcl_u8` | Unsigned 8-bit integer | For byte streams and characters |
| `tcl_i8` | Signed 8-bit integer | For small offsets |
| `tcl_u16` | Unsigned 16-bit integer | For short offsets |
| `tcl_i16` | Signed 16-bit integer | - |
| `tcl_u32` | Unsigned 32-bit integer | For Arena cursors and large counts |
| `tcl_i32` | Signed 32-bit integer | **Default integer type in Tcl scripts** |
| `tcl_ptr` | Platform-dependent pointer width | For address calculation (minimal use) |

---

## 10. Conclusion

Tclsh.v2 establishes a theoretically insurmountable industrial-grade barrier through physical memory Arenas, a stackless FSM, and strict primitive rules. By decoupling the logical core from the physical environment, BareTcl achieves perfect portability. Future development will focus on standard Tcl alignment and the physical implementation of compacting GC.
