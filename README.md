# Tclsh.v2 - Ultra-compact Stackless Tcl Interpreter for Embedded Systems

Tclsh.v2 is a high-reliability, minimal-footprint Tcl interpreter kernel designed for bare-metal MCU environments. It features a completely stackless state-machine execution model and a static-arena-based memory management system with compacting GC.

## Key Features
- **Zero-Libc Dependency**: Pure C implementation without standard library requirements.
- **Absolute Stackless Execution**: Prevents stack overflow in resource-constrained environments.
- **Static Arena Memory**: Deterministic memory usage using a single block of SRAM.
- **Bootstrap Expansion**: Core functionality built on 18 atomic instructions.
- **Industrial Grade**: Designed for mission-critical applications on MCUs like RH850/U2A.

## Documentation
Refer to [design.md](./design.md) for full technical specifications, architectural constraints, and the core instruction set.

## Project Structure
- `tcl_core.c`: Core interpreter kernel.
- `demo.c`: Interactive shell for Linux testing.
- `tcllib.tcl`: Bootstrap expansion library.
- `tests.tcl`: Comprehensive test suite.
- `build.sh`: Automated build and verification pipeline.

## License
Apache License 2.0 - see [LICENSE](./LICENSE) for details.
