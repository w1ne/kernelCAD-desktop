#!/bin/bash
# run_all.sh — Run all GUI tests, report pass/fail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TESTS=(
    "$SCRIPT_DIR/test_s_key_palette.py"
    "$SCRIPT_DIR/test_sketch_workflow.py"
    "$SCRIPT_DIR/test_toolbar_layout.py"
)

PASS=0
FAIL=0
FAILED_TESTS=()

echo "=== kernelCAD GUI test suite ==="
for test in "${TESTS[@]}"; do
    name=$(basename "$test" .py)
    echo ""
    echo "── $name ──"
    if "$SCRIPT_DIR/gui_runner.sh" "$test" > "/tmp/${name}.log" 2>&1; then
        result=$(grep -oE '[0-9]+ passed, [0-9]+ failed' "/tmp/${name}.log" | tail -1)
        echo "  $result"
        if grep -q '0 failed' "/tmp/${name}.log"; then
            PASS=$((PASS + 1))
        else
            FAIL=$((FAIL + 1))
            FAILED_TESTS+=("$name")
        fi
    else
        echo "  EXIT FAIL"
        FAIL=$((FAIL + 1))
        FAILED_TESTS+=("$name")
    fi
done

echo ""
echo "=== Summary: $PASS/$((PASS + FAIL)) tests passed ==="
if [ $FAIL -gt 0 ]; then
    echo "Failed: ${FAILED_TESTS[@]}"
    exit 1
fi
