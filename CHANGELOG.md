# Changelog

## v0.1.0 — Alpha Release (2026-03-24)

First public release of kernelCAD Desktop.

### Modeling (36 feature types)
- **Primitives**: Box, Cylinder, Sphere, Torus, Pipe
- **Sketch-based**: Extrude (4 extent types), Revolve, Sweep, Loft
- **Edge/Face operations**: Fillet (multi-attempt fallback), Chamfer, Shell, Draft, Press/Pull
- **Patterns**: Mirror, Rectangular, Circular, Path Pattern
- **Advanced**: Coil, Hole (counterbore/countersink), Thread, Scale, Thicken, Move
- **Boolean**: Combine (join/cut/intersect), Split Body, Split Face
- **Surface**: Patch, Stitch, Unstitch, Delete Face, Replace Face, Reverse Normal, Rib, Web
- **Construction**: Offset Plane, Construction Axis, Construction Point

### Sketch System
- 6 entity types: Point, Line, Circle, Arc, Ellipse, B-Spline
- 18 constraint types with Levenberg-Marquardt solver
- 16 interactive drawing tools
- Auto-dimensions, snap/inference lines, inline dimension editing
- Line chaining, auto-close profile, point merge on drag
- DXF and SVG import
- Construction mode toggle (X key)

### Parametric Engine
- Timeline with drag-to-reorder, suppress, rollback marker
- Feature editing via double-click (rollback + dialog + recompute)
- Stable naming (geometry-based face/edge signature matching)
- Dependency graph with topological sort and dirty propagation
- Expression parser (arithmetic, functions, parameter references, units)
- Command transaction safety (atomic rollback on failure)

### UI (Fusion 360-style)
- Tabbed ribbon toolbar (SOLID/SKETCH/ASSEMBLY) with icon-only buttons
- ToolRegistry: 120+ tools registered as single source of truth
- Floating command dialogs (Extrude, Fillet, Chamfer, Shell, Revolve, Press/Pull)
- Floating SketchPalette with collapsible sections
- Feature tree with tabs (Model/Bodies/Sketches/Components)
- Timeline panel with compact feature cards
- ViewCube with click-to-snap standard views
- Dark theme with Fusion-style color scheme
- Right-click context menus (face/edge/body/empty)
- Command palette (Ctrl+K)
- Keyboard shortcuts matching Fusion 360

### Selection
- GPU color-picking with hover pre-selection
- Selection-driven commands (select edges → F → fillet)
- Filter modes (All/Faces/Edges/Bodies)
- Tangent chain auto-select for fillet

### Rendering
- OpenGL 3.3 Core with Blinn-Phong shading
- 4 view modes: Solid+Edges, Solid, Wireframe, Hidden Line
- Dynamic near/far clip planes
- Grid + origin axes with distance-based fade
- Section planes (X/Y/Z clipping)
- Sketch overlay with dimension annotations

### File I/O
- Native format: `.kcd` (JSON with full parametric history)
- Import: STEP, IGES, STL (as solid body), DXF/SVG (into sketch)
- Export: STEP (all bodies), STL (all bodies)
- 2D Drawings: PDF/SVG with auto-dimensions (Front/Top/Right/Isometric)

### Scripting & Plugins
- `kernelcad-cli`: 50+ JSON commands over stdin/stdout
- Python API: CadQuery-style fluent interface
- Plugin system: Python plugins in `~/.kernelcad/plugins/`
- 5 bundled plugins (Gear Generator, Enclosure, Thread Insert, Git Sync)
- LLM agent interface: `--schema`, `help`, `state`, `getMesh`

### Robustness
- OCCT operations wrapped in Standard_Failure catch (40+ operations)
- Fillet/Chamfer multi-attempt fallback (4 strategies)
- Boolean operations: BOPAlgo_BOP primary + BRepAlgoAPI fallback
- Auto-save with crash recovery
- 119 tests (9 unit + 75 integration + 35 stress)

### Architecture
- 193 files, 52K lines of C++17
- Clean layer separation (kernel → features → document → ui)
- Zero circular dependencies
- Pimpl pattern on OCCTKernel
- Command pattern for undo/redo (40+ commands)
