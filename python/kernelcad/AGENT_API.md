# kernelCAD Agent API Reference

You are a CAD modeling agent. You create 3D mechanical parts by sending JSON commands to the kernelCAD engine.

## Protocol

Send one JSON command per line to stdin. Read one JSON response per line from stdout.

**Command format:**
```json
{"cmd": "commandName", "id": 1, ...parameters}
```

**Success response:**
```json
{"id": 1, "ok": true, "result": {...}}
```

**Error response:**
```json
{"id": 1, "ok": false, "error": "description of what went wrong"}
```

## Workflow

Every CAD model follows this pattern:

```
1. newDocument          → start fresh
2. createSketch         → create a 2D sketch on a plane
3. sketchAddRectangle   → draw 2D geometry in the sketch
4. sketchSolve          → solve constraints
5. extrude              → push the 2D profile into 3D
6. fillet/chamfer/shell  → modify the 3D body
7. exportStep           → save the result
```

## Important Rules

- **Body IDs are auto-generated.** After `createBox`, the response tells you the `bodyId`. Use it for subsequent operations.
- **Sketch IDs are auto-generated.** After `createSketch`, use the returned `sketchId`.
- **Point/Line IDs are auto-generated.** After `sketchAddPoint`, use the returned `pointId`.
- **Always call `sketchSolve` after adding geometry** to a sketch before extruding.
- **Extrude needs a sketch with a closed profile.** Use `sketchDetectProfiles` to verify.
- **Fillet/Chamfer need a `bodyId`.** Get it from the previous operation's response.
- **IDs increment per session:** sketch_1, sketch_2, body_1, body_2, etc.

## Commands

### Document

| Command | Parameters | Returns | Description |
|---------|-----------|---------|-------------|
| `newDocument` | — | `{}` | Start a new empty document |
| `save` | `path` (string) | `{}` | Save to .kcd file |
| `load` | `path` (string) | `{}` | Load from .kcd file |
| `importStep` | `path` (string) | `{count, bodyIds}` | Import STEP file |
| `exportStep` | `path` (string) | `{}` | Export all bodies to STEP |
| `exportStl` | `path` (string) | `{}` | Export all bodies to STL |
| `undo` | — | `{}` | Undo last operation |
| `redo` | — | `{}` | Redo last undone operation |
| `recompute` | — | `{}` | Force recompute all features |

### Primitives (skip sketch, create body directly)

| Command | Parameters | Returns | Description |
|---------|-----------|---------|-------------|
| `createBox` | `dx`, `dy`, `dz` (mm) | `{featureId, bodyId}` | Rectangular box |
| `createCylinder` | `radius`, `height` (mm) | `{featureId, bodyId}` | Cylinder |
| `createSphere` | `radius` (mm) | `{featureId, bodyId}` | Sphere |
| `createTorus` | `majorRadius`, `minorRadius` (mm) | `{featureId, bodyId}` | Torus (donut) |
| `createPipe` | `outerRadius`, `innerRadius`, `height` (mm) | `{featureId, bodyId}` | Hollow tube |

### Sketch

| Command | Parameters | Returns | Description |
|---------|-----------|---------|-------------|
| `createSketch` | `plane` ("XY", "XZ", "YZ") | `{sketchId, featureId}` | Create empty sketch |
| `sketchAddPoint` | `sketchId`, `x`, `y` | `{pointId}` | Add a point |
| `sketchAddLine` | `sketchId`, `startPointId`, `endPointId` | `{lineId}` | Add a line between two points |
| `sketchAddRectangle` | `sketchId`, `x1`, `y1`, `x2`, `y2` | `{pointIds, lineIds}` | Add rectangle (4 points, 4 lines) |
| `sketchAddCircle` | `sketchId`, `centerPointId`, `radius` | `{circleId}` | Add circle |
| `sketchAddArc` | `sketchId`, `centerPointId`, `startPointId`, `endPointId` | `{arcId}` | Add arc |
| `sketchAddConstraint` | `sketchId`, `type`, `entity1`, `entity2`, `value` | `{constraintId}` | Add constraint |
| `sketchSolve` | `sketchId` | `{status, freeDOF, iterations, residual}` | Solve constraints |
| `sketchDetectProfiles` | `sketchId` | `{profiles: [[entityIds]]}` | Find closed loops |

**Constraint types:** `Coincident`, `Horizontal`, `Vertical`, `Parallel`, `Perpendicular`, `Tangent`, `Equal`, `Distance`, `Radius`, `Fix`

### Features (modify bodies)

| Command | Parameters | Returns | Description |
|---------|-----------|---------|-------------|
| `extrude` | `sketchId`, `distance` (mm), optional: `symmetric` (bool) | `{featureId, bodyId}` | Extrude sketch profile into 3D |
| `fillet` | `bodyId`, `radius` (mm), optional: `edgeIds` (array) | `{featureId, bodyId}` | Round edges |
| `chamfer` | `bodyId`, `distance` (mm), optional: `edgeIds` (array) | `{featureId, bodyId}` | Bevel edges |
| `shell` | `bodyId`, `thickness` (mm) | `{featureId, bodyId}` | Hollow out body |
| `mirror` | `bodyId`, `planeNormalX/Y/Z` | `{featureId, bodyId}` | Mirror about plane |
| `circularPattern` | `bodyId`, `count`, `angle` (deg) | `{featureId, bodyId}` | Repeat around axis |
| `rectangularPattern` | `bodyId`, `countX`, `spacingX`, `countY`, `spacingY` | `{featureId, bodyId}` | Grid array |
| `hole` | `bodyId`, `x`, `y`, `z`, `dx`, `dy`, `dz`, `diameter`, `depth` | `{featureId, bodyId}` | Drill hole |
| `combine` | `targetBodyId`, `toolBodyId`, `operation` (0=join,1=cut,2=intersect) | `{featureId, bodyId}` | Boolean operation |

### Queries

| Command | Parameters | Returns | Description |
|---------|-----------|---------|-------------|
| `listBodies` | — | `{bodyIds: [{id}]}` | List all body IDs |
| `listFeatures` | — | `{features: [{id, name, type, isSuppressed}]}` | List timeline |
| `getProperties` | `bodyId` | `{volume, surfaceArea, mass, cogX/Y/Z, bbox...}` | Physical properties |
| `faceCount` | `bodyId` | `{count}` | Number of faces |
| `edgeCount` | `bodyId` | `{count}` | Number of edges |

### Timeline

| Command | Parameters | Returns | Description |
|---------|-----------|---------|-------------|
| `setMarker` | `position` (int) | `{}` | Move rollback marker |
| `suppress` | `featureId` | `{}` | Toggle suppress |
| `deleteFeature` | `featureId` | `{}` | Remove feature |

## Examples

### Example 1: Simple bracket
```json
{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddRectangle","id":3,"sketchId":"sketch_1","x1":0,"y1":0,"x2":80,"y2":60}
{"cmd":"sketchSolve","id":4,"sketchId":"sketch_1"}
{"cmd":"extrude","id":5,"sketchId":"sketch_1","distance":15}
{"cmd":"fillet","id":6,"bodyId":"body_2","radius":3}
{"cmd":"shell","id":7,"bodyId":"body_2","thickness":2}
{"cmd":"exportStep","id":8,"path":"bracket.step"}
```

### Example 2: Flanged cylinder
```json
{"cmd":"newDocument","id":1}
{"cmd":"createCylinder","id":2,"radius":20,"height":50}
{"cmd":"createBox","id":3,"dx":60,"dy":60,"dz":5}
{"cmd":"combine","id":4,"targetBodyId":"body_1","toolBodyId":"body_2","operation":0}
{"cmd":"fillet","id":5,"bodyId":"body_1","radius":5}
{"cmd":"exportStep","id":6,"path":"flange.step"}
```

### Example 3: L-bracket from polygon
```json
{"cmd":"newDocument","id":1}
{"cmd":"createSketch","id":2,"plane":"XY"}
{"cmd":"sketchAddPoint","id":3,"sketchId":"sketch_1","x":0,"y":0}
{"cmd":"sketchAddPoint","id":4,"sketchId":"sketch_1","x":60,"y":0}
{"cmd":"sketchAddPoint","id":5,"sketchId":"sketch_1","x":60,"y":20}
{"cmd":"sketchAddPoint","id":6,"sketchId":"sketch_1","x":20,"y":20}
{"cmd":"sketchAddPoint","id":7,"sketchId":"sketch_1","x":20,"y":40}
{"cmd":"sketchAddPoint","id":8,"sketchId":"sketch_1","x":0,"y":40}
{"cmd":"sketchAddLine","id":9,"sketchId":"sketch_1","startPointId":"pt_1","endPointId":"pt_2"}
{"cmd":"sketchAddLine","id":10,"sketchId":"sketch_1","startPointId":"pt_2","endPointId":"pt_3"}
{"cmd":"sketchAddLine","id":11,"sketchId":"sketch_1","startPointId":"pt_3","endPointId":"pt_4"}
{"cmd":"sketchAddLine","id":12,"sketchId":"sketch_1","startPointId":"pt_4","endPointId":"pt_5"}
{"cmd":"sketchAddLine","id":13,"sketchId":"sketch_1","startPointId":"pt_5","endPointId":"pt_6"}
{"cmd":"sketchAddLine","id":14,"sketchId":"sketch_1","startPointId":"pt_6","endPointId":"pt_1"}
{"cmd":"sketchSolve","id":15,"sketchId":"sketch_1"}
{"cmd":"extrude","id":16,"sketchId":"sketch_1","distance":15}
{"cmd":"fillet","id":17,"bodyId":"body_2","radius":2}
{"cmd":"exportStep","id":18,"path":"l_bracket.step"}
```

### Example 4: Query and validate
```json
{"cmd":"newDocument","id":1}
{"cmd":"createBox","id":2,"dx":100,"dy":50,"dz":25}
{"cmd":"getProperties","id":3,"bodyId":"body_1"}
{"cmd":"faceCount","id":4,"bodyId":"body_1"}
{"cmd":"edgeCount","id":5,"bodyId":"body_1"}
```
Expected: volume≈125000, faces=6, edges=24

## Common Errors and Fixes

| Error | Cause | Fix |
|-------|-------|-----|
| `Body not found: body_X` | Wrong body ID | Use the bodyId from the previous command's response |
| `Sketch not found` | Wrong sketch ID | Use the sketchId from createSketch response |
| `No closed profiles found` | Sketch has open geometry | Ensure all lines form a closed loop, call sketchSolve first |
| `Fillet failed: radius too large` | Radius exceeds edge length | Use a smaller radius |
| `Nothing to undo` | No previous operations | Only call undo after making changes |

## Python API (Alternative)

```python
from kernelcad import Workplane

part = (
    Workplane("XY")
    .rect(80, 60)
    .extrude(20)
    .fillet(3)
    .shell(2)
    .exportStep("bracket.step")
)

print(f"Volume: {part.properties()['volume']:.0f} mm³")
```
