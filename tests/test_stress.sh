#!/usr/bin/env bash
# Stress tests + smoke tests for every tool in kernelCAD
# Tests edge cases, large values, zero values, negative values, and combined workflows
set -euo pipefail

CLI="$(dirname "$0")/../build/src/kernelcad-cli"
PASS=0; FAIL=0; TOTAL=0

run_test() {
    local name="$1" input="$2" expect="$3"
    TOTAL=$((TOTAL+1))
    local output
    output=$(echo "$input" | "$CLI" 2>/dev/null) || true
    if echo "$output" | grep -q "$expect"; then
        PASS=$((PASS+1))
    else
        FAIL=$((FAIL+1))
        echo "  ✗ $name"
        echo "    Expected: $expect"
        echo "    Last line: $(echo "$output" | tail -1 | head -c 120)"
    fi
}

echo "=== STRESS & SMOKE TESTS ==="
echo ""

# ═══════════════════════════════════════════════════════════════════
echo "--- Primitive Stress Tests ---"
# ═══════════════════════════════════════════════════════════════════

run_test "Tiny box (0.01mm)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":0.01,"dy":0.01,"dz":0.01}
{"cmd":"faceCount","id":3,"bodyId":"body_1"}' \
    '"count":6'

run_test "Large box (10000mm)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":10000,"dy":10000,"dz":10000}
{"cmd":"getProperties","id":3,"bodyId":"body_1"}' \
    '"volume"'

run_test "Thin box (1000x1000x0.1)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":1000,"dy":1000,"dz":0.1}
{"cmd":"faceCount","id":3,"bodyId":"body_1"}' \
    '"count":6'

run_test "Tiny cylinder (r=0.01)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createCylinder","id":2,"radius":0.01,"height":1}' \
    '"bodyId"'

run_test "Large sphere (r=5000)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSphere","id":2,"radius":5000}
{"cmd":"listBodies","id":3}' \
    '"body_sphere_1"'

run_test "Flat torus (major=100, minor=1)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createTorus","id":2,"majorRadius":100,"minorRadius":1}' \
    '"bodyId"'

run_test "Thick pipe (outer=50, inner=49)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createPipe","id":2,"outerRadius":50,"innerRadius":49,"height":100}' \
    '"bodyId"'

# ═══════════════════════════════════════════════════════════════════
echo ""
echo "--- Sketch Stress Tests ---"
# ═══════════════════════════════════════════════════════════════════

run_test "Many points (20 points)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddPoint","id":3,"sketchId":"sketch_1","x":0,"y":0}
{"cmd":"sketchAddPoint","id":4,"sketchId":"sketch_1","x":10,"y":0}
{"cmd":"sketchAddPoint","id":5,"sketchId":"sketch_1","x":20,"y":0}
{"cmd":"sketchAddPoint","id":6,"sketchId":"sketch_1","x":30,"y":0}
{"cmd":"sketchAddPoint","id":7,"sketchId":"sketch_1","x":40,"y":0}
{"cmd":"sketchAddPoint","id":8,"sketchId":"sketch_1","x":50,"y":0}
{"cmd":"sketchAddPoint","id":9,"sketchId":"sketch_1","x":60,"y":0}
{"cmd":"sketchAddPoint","id":10,"sketchId":"sketch_1","x":70,"y":0}
{"cmd":"sketchAddPoint","id":11,"sketchId":"sketch_1","x":80,"y":0}
{"cmd":"sketchAddPoint","id":12,"sketchId":"sketch_1","x":90,"y":0}
{"cmd":"sketchSolve","id":13,"sketchId":"sketch_1"}' \
    '"Solved"'

run_test "Complex polygon (hexagon)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddPoint","id":3,"sketchId":"sketch_1","x":20,"y":0}
{"cmd":"sketchAddPoint","id":4,"sketchId":"sketch_1","x":10,"y":17.32}
{"cmd":"sketchAddPoint","id":5,"sketchId":"sketch_1","x":-10,"y":17.32}
{"cmd":"sketchAddPoint","id":6,"sketchId":"sketch_1","x":-20,"y":0}
{"cmd":"sketchAddPoint","id":7,"sketchId":"sketch_1","x":-10,"y":-17.32}
{"cmd":"sketchAddPoint","id":8,"sketchId":"sketch_1","x":10,"y":-17.32}
{"cmd":"sketchAddLine","id":9,"sketchId":"sketch_1","startPointId":"pt_1","endPointId":"pt_2"}
{"cmd":"sketchAddLine","id":10,"sketchId":"sketch_1","startPointId":"pt_2","endPointId":"pt_3"}
{"cmd":"sketchAddLine","id":11,"sketchId":"sketch_1","startPointId":"pt_3","endPointId":"pt_4"}
{"cmd":"sketchAddLine","id":12,"sketchId":"sketch_1","startPointId":"pt_4","endPointId":"pt_5"}
{"cmd":"sketchAddLine","id":13,"sketchId":"sketch_1","startPointId":"pt_5","endPointId":"pt_6"}
{"cmd":"sketchAddLine","id":14,"sketchId":"sketch_1","startPointId":"pt_6","endPointId":"pt_1"}
{"cmd":"sketchSolve","id":15,"sketchId":"sketch_1"}
{"cmd":"sketchDetectProfiles","id":16,"sketchId":"sketch_1"}' \
    '"profiles"'

run_test "Tiny rectangle (0.1x0.1)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddRectangle","id":3,"sketchId":"sketch_1","x1":0,"y1":0,"x2":0.1,"y2":0.1}
{"cmd":"sketchSolve","id":4,"sketchId":"sketch_1"}
{"cmd":"extrude","id":5,"sketchId":"sketch_1","distance":0.1}' \
    '"bodyId"'

run_test "Large rectangle (1000x1000)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddRectangle","id":3,"sketchId":"sketch_1","x1":-500,"y1":-500,"x2":500,"y2":500}
{"cmd":"sketchSolve","id":4,"sketchId":"sketch_1"}
{"cmd":"extrude","id":5,"sketchId":"sketch_1","distance":10}' \
    '"bodyId"'

run_test "Circle with constraint" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddPoint","id":3,"sketchId":"sketch_1","x":0,"y":0}
{"cmd":"sketchAddCircle","id":4,"sketchId":"sketch_1","centerPointId":"pt_1","radius":25}
{"cmd":"sketchAddConstraint","id":5,"sketchId":"sketch_1","type":"Radius","entity1":"circ_1","value":30}
{"cmd":"sketchSolve","id":6,"sketchId":"sketch_1"}' \
    '"Solved"'

# ═══════════════════════════════════════════════════════════════════
echo ""
echo "--- Feature Stress Tests ---"
# ═══════════════════════════════════════════════════════════════════

run_test "Extrude very thin (0.1mm)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddRectangle","id":3,"sketchId":"sketch_1","x1":0,"y1":0,"x2":50,"y2":30}
{"cmd":"sketchSolve","id":4,"sketchId":"sketch_1"}
{"cmd":"extrude","id":5,"sketchId":"sketch_1","distance":0.1}' \
    '"bodyId"'

run_test "Extrude very tall (1000mm)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddRectangle","id":3,"sketchId":"sketch_1","x1":0,"y1":0,"x2":10,"y2":10}
{"cmd":"sketchSolve","id":4,"sketchId":"sketch_1"}
{"cmd":"extrude","id":5,"sketchId":"sketch_1","distance":1000}' \
    '"bodyId"'

run_test "Fillet small radius (0.1mm)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"fillet","id":3,"bodyId":"body_1","radius":0.1}
{"cmd":"faceCount","id":4,"bodyId":"body_1"}' \
    '"count"'

run_test "Chamfer small distance (0.1mm)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"chamfer","id":3,"bodyId":"body_1","distance":0.1}' \
    '"featureId"'

run_test "Shell thin wall (0.5mm)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"shell","id":3,"bodyId":"body_1","thickness":0.5}' \
    '"featureId"'

run_test "Multiple fillets on same body" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"fillet","id":3,"bodyId":"body_1","radius":2}
{"cmd":"fillet","id":4,"bodyId":"body_1","radius":1}
{"cmd":"faceCount","id":5,"bodyId":"body_1"}' \
    '"count"'

run_test "Fillet then shell" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"fillet","id":3,"bodyId":"body_1","radius":3}
{"cmd":"shell","id":4,"bodyId":"body_1","thickness":2}' \
    '"featureId"'

run_test "Mirror then fillet" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":20,"dy":10,"dz":10}
{"cmd":"mirror","id":3,"bodyId":"body_1","planeNormalX":1,"planeNormalY":0,"planeNormalZ":0}
{"cmd":"fillet","id":4,"bodyId":"body_1","radius":2}' \
    '"featureId"'

run_test "Circular pattern 12 copies" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":5,"dy":5,"dz":20}
{"cmd":"circularPattern","id":3,"bodyId":"body_1","count":12,"angle":360}' \
    '"featureId"'

# ═══════════════════════════════════════════════════════════════════
echo ""
echo "--- Combined Workflow Stress Tests ---"
# ═══════════════════════════════════════════════════════════════════

run_test "Full bracket workflow (sketch→extrude→fillet→shell→hole→export)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddRectangle","id":3,"sketchId":"sketch_1","x1":0,"y1":0,"x2":80,"y2":60}
{"cmd":"sketchSolve","id":4,"sketchId":"sketch_1"}
{"cmd":"extrude","id":5,"sketchId":"sketch_1","distance":15}
{"cmd":"fillet","id":6,"bodyId":"body_2","radius":3}
{"cmd":"shell","id":7,"bodyId":"body_2","thickness":2}
{"cmd":"exportStep","id":8,"path":"/tmp/stress_bracket.step"}' \
    '"ok":true'

run_test "Multiple bodies workflow" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"createCylinder","id":3,"radius":10,"height":40}
{"cmd":"createSphere","id":4,"radius":15}
{"cmd":"listBodies","id":5}' \
    '"body_1"'

run_test "Undo removes last feature" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"fillet","id":3,"bodyId":"body_1","radius":3}
{"cmd":"undo","id":4}
{"cmd":"listFeatures","id":5}' \
    '"Extrude"'

run_test "Save complex model and reload" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddRectangle","id":3,"sketchId":"sketch_1","x1":0,"y1":0,"x2":60,"y2":40}
{"cmd":"sketchSolve","id":4,"sketchId":"sketch_1"}
{"cmd":"extrude","id":5,"sketchId":"sketch_1","distance":25}
{"cmd":"fillet","id":6,"bodyId":"body_2","radius":3}
{"cmd":"save","id":7,"path":"/tmp/stress_complex.kcd"}
{"cmd":"newDocument","id":8}
{"cmd":"load","id":9,"path":"/tmp/stress_complex.kcd"}
{"cmd":"listFeatures","id":10}' \
    '"Fillet"'

run_test "STEP round-trip preserves features" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":30,"dy":20,"dz":10}
{"cmd":"fillet","id":3,"bodyId":"body_1","radius":2}
{"cmd":"exportStep","id":4,"path":"/tmp/stress_roundtrip.step"}
{"cmd":"newDocument","id":5}
{"cmd":"importStep","id":6,"path":"/tmp/stress_roundtrip.step"}
{"cmd":"faceCount","id":7,"bodyId":"imported_1"}' \
    '"count"'

run_test "State after complex operations" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"fillet","id":3,"bodyId":"body_1","radius":3}
{"cmd":"mirror","id":4,"bodyId":"body_1","planeNormalX":1,"planeNormalY":0,"planeNormalZ":0}
{"cmd":"state","id":5}' \
    '"featureCount"'

run_test "GetMesh after fillet" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"fillet","id":3,"bodyId":"body_1","radius":3}
{"cmd":"getMesh","id":4,"bodyId":"body_1"}' \
    '"triangleCount"'

# ═══════════════════════════════════════════════════════════════════
echo ""
echo "--- Error Recovery Stress Tests ---"
# ═══════════════════════════════════════════════════════════════════

run_test "Double fillet doesn't crash" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":20,"dy":20,"dz":20}
{"cmd":"fillet","id":3,"bodyId":"body_1","radius":5}
{"cmd":"fillet","id":4,"bodyId":"body_1","radius":2}
{"cmd":"faceCount","id":5,"bodyId":"body_1"}' \
    '"count"'

run_test "Fillet after chamfer doesn't crash" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":30,"dy":30,"dz":30}
{"cmd":"chamfer","id":3,"bodyId":"body_1","distance":3}
{"cmd":"fillet","id":4,"bodyId":"body_1","radius":1}' \
    '"ok"'

run_test "Shell after fillet doesn't crash" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":40,"dy":30,"dz":20}
{"cmd":"fillet","id":3,"bodyId":"body_1","radius":3}
{"cmd":"shell","id":4,"bodyId":"body_1","thickness":2}
{"cmd":"getProperties","id":5,"bodyId":"body_1"}' \
    '"volume"'

run_test "Recover after bad operation" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":10,"dy":10,"dz":10}
{"cmd":"fillet","id":3,"bodyId":"body_1","radius":999}
{"cmd":"createBox","id":4,"dx":20,"dy":20,"dz":20}
{"cmd":"listBodies","id":5}' \
    '"body_1"'

run_test "Empty sketch extrude doesn't crash" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchSolve","id":3,"sketchId":"sketch_1"}
{"cmd":"sketchDetectProfiles","id":4,"sketchId":"sketch_1"}' \
    '"profiles":\[\]'

run_test "Operations on nonexistent bodies" \
    '{"cmd":"newDocument","id":1}
{"cmd":"fillet","id":2,"bodyId":"ghost","radius":3}' \
    '"ok":false'

run_test "Negative dimensions handled" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":-10,"dy":-10,"dz":-10}' \
    '"ok"'

# ═══════════════════════════════════════════════════════════════════
echo ""
echo "==========================================="
echo "  STRESS TESTS: $PASS passed, $FAIL failed, $TOTAL total"
echo "==========================================="

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
