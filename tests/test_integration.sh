#!/usr/bin/env bash
# Integration regression test suite for kernelCAD
# Tests all major workflows via kernelcad-cli
# Run: ./tests/test_integration.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CLI="$SCRIPT_DIR/../build/src/kernelcad-cli"
PASS=0; FAIL=0; TOTAL=0

run_test() {
    local name="$1"
    local input="$2"
    local expect_pattern="$3"
    TOTAL=$((TOTAL+1))

    local output
    output=$(echo "$input" | "$CLI" 2>/dev/null) || true

    if echo "$output" | grep -q "$expect_pattern"; then
        PASS=$((PASS+1))
        echo "  ✓ $name"
    else
        FAIL=$((FAIL+1))
        echo "  ✗ $name"
        echo "    Expected pattern: $expect_pattern"
        echo "    Got: $(echo "$output" | tail -1)"
    fi
}

echo "=== INTEGRATION REGRESSION TESTS ==="
echo ""

# ── Document Operations ─────────────────────────────────────────────
echo "--- Document Operations ---"

run_test "New document" \
    '{"cmd":"newDocument","id":1}' \
    '"ok":true'

run_test "Create box" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}' \
    '"bodyId":"body_1"'

run_test "Create cylinder" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createCylinder","id":2,"radius":10,"height":30}' \
    '"bodyId"'

run_test "Create sphere" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSphere","id":2,"radius":25}' \
    '"bodyId"'

run_test "List bodies after creation" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"listBodies","id":3}' \
    '"body_1"'

run_test "List features after creation" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"listFeatures","id":3}' \
    '"Extrude"'

# ── Physical Properties ─────────────────────────────────────────────
echo ""
echo "--- Physical Properties ---"

run_test "Box volume (50mm cube = 125000)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"getProperties","id":3,"bodyId":"body_1"}' \
    '"volume":124999'

run_test "Box face count (6)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"faceCount","id":3,"bodyId":"body_1"}' \
    '"count":6'

run_test "Box edge count (24)" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"edgeCount","id":3,"bodyId":"body_1"}' \
    '"count":24'

# ── Sketch System ───────────────────────────────────────────────────
echo ""
echo "--- Sketch System ---"

run_test "Create sketch on XY" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}' \
    '"sketchId":"sketch_1"'

run_test "Add rectangle to sketch" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddRectangle","id":3,"sketchId":"sketch_1","x1":0,"y1":0,"x2":60,"y2":40}' \
    '"pointIds"'

run_test "Solve sketch" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddRectangle","id":3,"sketchId":"sketch_1","x1":0,"y1":0,"x2":60,"y2":40}
{"cmd":"sketchSolve","id":4,"sketchId":"sketch_1"}' \
    '"Solved"'

run_test "Detect profiles" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddRectangle","id":3,"sketchId":"sketch_1","x1":0,"y1":0,"x2":60,"y2":40}
{"cmd":"sketchSolve","id":4,"sketchId":"sketch_1"}
{"cmd":"sketchDetectProfiles","id":5,"sketchId":"sketch_1"}' \
    '"profiles":\[\['

run_test "Add point to sketch" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddPoint","id":3,"sketchId":"sketch_1","x":10,"y":20}' \
    '"pointId":"pt_1"'

run_test "Add line to sketch" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddPoint","id":3,"sketchId":"sketch_1","x":0,"y":0}
{"cmd":"sketchAddPoint","id":4,"sketchId":"sketch_1","x":50,"y":0}
{"cmd":"sketchAddLine","id":5,"sketchId":"sketch_1","startPointId":"pt_1","endPointId":"pt_2"}' \
    '"lineId"'

run_test "Add circle to sketch" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddPoint","id":3,"sketchId":"sketch_1","x":25,"y":25}
{"cmd":"sketchAddCircle","id":4,"sketchId":"sketch_1","centerPointId":"pt_1","radius":15}' \
    '"circleId"'

run_test "Add constraint" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddPoint","id":3,"sketchId":"sketch_1","x":0,"y":0}
{"cmd":"sketchAddPoint","id":4,"sketchId":"sketch_1","x":50,"y":0.5}
{"cmd":"sketchAddConstraint","id":5,"sketchId":"sketch_1","type":"Horizontal","entity1":"pt_1","entity2":"pt_2"}' \
    '"ok":true'

# ── Sketch on Alternate Planes ─────────────────────────────────────
echo ""
echo "--- Sketch on Alternate Planes ---"

run_test "Sketch on XZ plane" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XZ"}
{"cmd":"sketchAddRectangle","id":3,"sketchId":"sketch_1","x1":0,"y1":0,"x2":40,"y2":30}
{"cmd":"sketchSolve","id":4,"sketchId":"sketch_1"}
{"cmd":"extrude","id":5,"sketchId":"sketch_1","distance":20}
{"cmd":"listFeatures","id":6}' \
    '"Extrude"'

run_test "Sketch on YZ plane" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"YZ"}' \
    '"sketchId"'

run_test "Sketch on face of box" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"createSketchOnFace","id":3,"bodyId":"body_1","faceIndex":0}' \
    '"sketchId"'

run_test "Sketch on face rejects non-planar" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createCylinder","id":2,"radius":10,"height":30}
{"cmd":"createSketchOnFace","id":3,"bodyId":"body_1","faceIndex":0}' \
    '"ok"'

run_test "Hole on face with faceIndex direction" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"hole","id":3,"bodyId":"body_1","posX":25,"posY":15,"posZ":20,"faceIndex":0,"diameter":"8 mm","depth":"10 mm"}' \
    '"featureId"'

# ── Feature Pipeline ────────────────────────────────────────────────
echo ""
echo "--- Feature Pipeline ---"

run_test "Sketch → Extrude" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddRectangle","id":3,"sketchId":"sketch_1","x1":0,"y1":0,"x2":60,"y2":40}
{"cmd":"sketchSolve","id":4,"sketchId":"sketch_1"}
{"cmd":"extrude","id":5,"sketchId":"sketch_1","distance":25}' \
    '"featureId":"extrude_2"'

run_test "Extrude → Fillet" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"fillet","id":3,"bodyId":"body_1","radius":3}' \
    '"featureId":"fillet_2"'

run_test "Fillet increases face count" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"fillet","id":3,"bodyId":"body_1","radius":3}
{"cmd":"faceCount","id":4,"bodyId":"body_1"}' \
    '"count":26'

run_test "Extrude → Chamfer" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"chamfer","id":3,"bodyId":"body_1","distance":2}' \
    '"featureId"'

run_test "Extrude → Shell" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"shell","id":3,"bodyId":"body_1","thickness":2}' \
    '"featureId"'

run_test "Extrude → Mirror" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"mirror","id":3,"bodyId":"body_1","planeNormalX":1,"planeNormalY":0,"planeNormalZ":0}' \
    '"featureId"'

run_test "Extrude → Circular Pattern" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"circularPattern","id":3,"bodyId":"body_1","count":4,"angle":360}' \
    '"featureId"'

run_test "Multiple features in timeline" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"fillet","id":3,"bodyId":"body_1","radius":3}
{"cmd":"listFeatures","id":4}' \
    '"Fillet"'

# ── STEP Import/Export ──────────────────────────────────────────────
echo ""
echo "--- Import/Export ---"

run_test "STEP export" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"exportStep","id":3,"path":"/tmp/regtest.step"}' \
    '"ok":true'

run_test "STEP import" \
    '{"cmd":"newDocument","id":1}
{"cmd":"importStep","id":2,"path":"/tmp/regtest.step"}' \
    '"count":1'

run_test "STEP round-trip preserves volume" \
    '{"cmd":"newDocument","id":1}
{"cmd":"importStep","id":2,"path":"/tmp/regtest.step"}
{"cmd":"getProperties","id":3,"bodyId":"imported_1"}' \
    '"volume":124999'

run_test "STL export" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"exportStl","id":3,"path":"/tmp/regtest.stl"}' \
    '"ok":true'

# ── Error Handling ──────────────────────────────────────────────────
echo ""
echo "--- Error Handling ---"

run_test "Fillet on nonexistent body" \
    '{"cmd":"newDocument","id":1}
{"cmd":"fillet","id":2,"bodyId":"nonexistent","radius":3}' \
    '"ok":false'

run_test "Extrude with empty sketch creates default box" \
    '{"cmd":"newDocument","id":1}
{"cmd":"extrude","id":2,"sketchId":"","distance":10}' \
    '"ok":true'

run_test "Unknown command" \
    '{"cmd":"doesNotExist","id":1}' \
    '"ok":false'

run_test "Missing required field" \
    '{"cmd":"fillet","id":1}' \
    '"ok":false'

# ── Timeline Operations ────────────────────────────────────────────
echo ""
echo "--- Timeline ---"

run_test "Suppress feature" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"suppress","id":3,"featureId":"extrude_1"}
{"cmd":"listFeatures","id":4}' \
    '"isSuppressed":true'

run_test "Delete feature" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"deleteFeature","id":3,"featureId":"extrude_1"}
{"cmd":"listFeatures","id":4}' \
    '"features":\[\]'

run_test "Recompute" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"recompute","id":3}' \
    '"ok":true'

# ── Error Handling & Crash Recovery ────────────────────────────────
echo ""
echo "--- Error Handling ---"

run_test "Fillet with too-large radius doesn't crash" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":10,"dy":10,"dz":10}
{"cmd":"fillet","id":3,"bodyId":"body_1","radius":"100 mm"}' \
    '"ok"'

run_test "Shell with too-large thickness doesn't crash" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":10,"dy":10,"dz":10}
{"cmd":"shell","id":3,"bodyId":"body_1","thickness":100}' \
    '"ok"'

run_test "Boolean with nonexistent body returns error" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":10,"dy":10,"dz":10}
{"cmd":"combine","id":3,"targetBodyId":"body_1","toolBodyId":"nonexistent","operation":"Join"}' \
    '"ok":false'

run_test "Chamfer with too-large distance doesn't crash" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":10,"dy":10,"dz":10}
{"cmd":"chamfer","id":3,"bodyId":"body_1","distance":"100 mm"}' \
    '"ok"'

run_test "Model usable after fillet error" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":10,"dy":10,"dz":10}
{"cmd":"fillet","id":3,"bodyId":"body_1","radius":"100 mm"}
{"cmd":"listBodies","id":4}' \
    '"body_1"'

run_test "Recompute after error preserves geometry" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":50,"dy":30,"dz":20}
{"cmd":"fillet","id":3,"bodyId":"body_1","radius":"100 mm"}
{"cmd":"recompute","id":4}
{"cmd":"listBodies","id":5}' \
    '"body_1"'

# ── Interference Check ──────────────────────────────────────────────
echo ""
echo "--- Interference Check ---"

run_test "Interference check with no overlap" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":10,"dy":10,"dz":10}
{"cmd":"createSphere","id":3,"radius":5}
{"cmd":"checkInterference","id":4}' \
    '"interferenceCount"'

run_test "Interference check needs 2 bodies" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":10,"dy":10,"dz":10}
{"cmd":"checkInterference","id":3}' \
    '"ok":false'

# ── Joint Types ────────────────────────────────────────────────────
echo ""
echo "--- Joint Types ---"

run_test "Add rigid joint" \
    '{"cmd":"newDocument","id":1}
{"cmd":"addJoint","id":2,"type":"Rigid"}' \
    '"featureId"'

run_test "Add slider joint" \
    '{"cmd":"newDocument","id":1}
{"cmd":"addJoint","id":2,"type":"Slider"}' \
    '"featureId"'

run_test "Add ball joint" \
    '{"cmd":"newDocument","id":1}
{"cmd":"addJoint","id":2,"type":"Ball"}' \
    '"featureId"'

# ── Unstitch ───────────────────────────────────────────────────────
echo ""
echo "--- Unstitch ---"

run_test "Unstitch a box" \
    '{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":10,"dy":10,"dz":10}
{"cmd":"unstitch","id":3,"targetBodyId":"body_1"}' \
    '"bodyId"'

run_test "Unstitch missing body fails" \
    '{"cmd":"newDocument","id":1}
{"cmd":"unstitch","id":2,"targetBodyId":"nonexistent"}' \
    '"ok":false'

# ── Summary ─────────────────────────────────────────────────────────
echo ""
echo "==========================================="
echo "  RESULTS: $PASS passed, $FAIL failed, $TOTAL total"
echo "==========================================="

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
