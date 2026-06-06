#!/bin/bash
set -e

echo "=== Tclsh.v2 Build & Test Pipeline ==="

echo "[0/4] Generating Bootstrap Library..."
python3 tcl2c.py tcllib.tcl tcllib.c

echo "[1/4] Compiling Core for Libc-Free Check..."
gcc -c tcl_core.c -ffreestanding -nostdlib -o tcl_core.o

echo "[2/4] Verifying Libc Independence (nm -u)..."
# Expecting only tcl_hal_puts to be undefined
UNDEF_SYMBOLS=$(nm -u tcl_core.o)
echo "$UNDEF_SYMBOLS"
if echo "$UNDEF_SYMBOLS" | grep -v "tcl_hal_puts" | grep -q "[a-zA-Z]"; then
    echo "ERROR: Found unauthorized external symbols!"
    exit 1
fi
echo "Libc-Free check passed."

echo "[3/4] Compiling Executable..."
gcc -DTCL_DEBUG demo.c -o tclsh

echo "[4/4] Running Full Coverage Test Suite (with timeout)..."
# Remove debug prints in tcl_core.c before final test? 
# We'll just run it as is.
timeout 10s ./tclsh tests.tcl > test_output.log
if grep -q "ALL TESTS PASSED" test_output.log; then
    echo "SUCCESS: All tests passed!"
    cat test_output.log
else
    echo "ERROR: Test suite failed."
    cat test_output.log
    exit 1
fi

echo "Cleaning up..."
rm tcl_core.o test_output.log
echo "Build and test completed successfully."
