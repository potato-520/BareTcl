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
# Use timeout to prevent infinite loops and capture all output
if timeout 15s ./tclsh tests/tests.tcl > test_output.log 2>&1; then
    # Check if the log contains the final 100% success conclusion
    if grep -q "结论: 所有测试在标准 Tcl 环境下均已通过" test_output.log; then
        echo "SUCCESS: All tests passed!"
        cat test_output.log
    else
        echo "ERROR: Test suite output did not contain success conclusion."
        cat test_output.log
        exit 1
    fi
else
    echo "ERROR: Test suite crashed or timed out."
    cat test_output.log
    exit 1
fi

echo "Cleaning up..."
rm tcl_core.o src/tcllib.c test_output.log
echo "Build and test completed successfully."
