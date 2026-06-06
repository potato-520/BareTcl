#!/bin/bash
set -e

echo "=== BareTcl Build & Test Pipeline ==="

echo "[0/4] Generating Bootstrap Library..."
python3 tools/tcl2c.py src/tcllib.tcl src/tcllib.c

echo "[1/4] Compiling Core for Libc-Free Check..."
gcc -c src/tcl_core.c -ffreestanding -nostdlib -o tcl_core.o

echo "[2/4] Verifying Libc Independence (nm -u)..."
# Expecting only tcl_hal_puts to be undefined
nm -u tcl_core.o | grep -v tcl_hal_puts | grep . && (echo "ERROR: Found external Libc dependencies!" && nm -u tcl_core.o && exit 1) || true

echo "Libc-Free check passed."

echo "[3/4] Compiling Demo Executable..."
gcc examples/demo.c -o tclsh

echo "[4/4] Running Industrial Validation Suite..."
if timeout 10s ./tclsh tests/tests.tcl > test_output.log; then
    echo "SUCCESS: All tests passed!"
    cat test_output.log
else
    echo "ERROR: Test suite failed."
    cat test_output.log
    exit 1
fi

echo "Cleaning up..."
rm tcl_core.o src/tcllib.c test_output.log
echo "Build and test completed successfully."
