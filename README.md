# kernelCAD Desktop

Open-source parametric CAD application built with C++17, Qt6, and OpenCASCADE Technology (OCCT).

![License](https://img.shields.io/badge/license-MIT-blue)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)
![Lines of Code](https://img.shields.io/badge/lines-31K+-green)

## Features

### Modeling
- **25+ feature types**: Extrude, Revolve, Fillet, Chamfer, Sweep, Loft, Shell, Mirror, Rectangular/Circular/Path Pattern, Coil, Hole, Thread, Draft, Scale, Thicken, Combine, SplitBody, OffsetFaces, DeleteFace, ReplaceFace, Move
- **Primitives**: Box, Cylinder, Sphere
- **Boolean operations**: Union, Cut, Intersect
- **Physical properties**: Volume, surface area, mass, center of gravity, inertia tensor

### Sketch System
- **Entities**: Points, Lines, Circles, Arcs, B-Splines
- **18 constraint types**: Coincident, Parallel, Perpendicular, Tangent, Distance, Angle, Radius, Horizontal, Vertical, Equal, Symmetric, Midpoint, Concentric, Fix, and more
- **Levenberg-Marquardt constraint solver** with damping, step limiting, redundancy detection, and multi-restart convergence
- **Sketch operations**: Trim, Extend, Offset
- **Interactive drawing**: Click-to-draw with snap/inference lines (horizontal, vertical, midpoint) and dimension annotations
- **Profile detection**: Automatic closed-loop extraction for extrusion

### Parametric Engine
- **Timeline**: Linear feature history with drag-to-reorder, suppress/unsuppress, rollback marker
- **Feature editing**: Double-click any feature to roll back, edit parameters, and recompute forward
- **Stable naming**: Geometry-based face/edge signature matching — references survive recomputation
- **Dependency graph**: DAG with topological sort and dirty propagation
- **Expression parser**: Recursive-descent with arithmetic, `sin/cos/sqrt/pow/min/max`, `PI/E` constants, parameter references, unit stripping (`mm`, `deg`, `rad`, `cm`, `in`)
- **Circular dependency detection** with BFS cycle check

### Rendering
- **OpenGL 3.3 Core** with Blinn-Phong shading and per-vertex normals
- **4 view modes**: Solid with Edges, Solid, Wireframe, Hidden Line
- **ViewCube**: Interactive orientation cube with click-to-snap
- **Standard views**: Front/Back/Left/Right/Top/Bottom/Isometric (Numpad 1-7)
- **Grid + origin axes**: 10mm minor / 50mm major grid, RGB axes, distance-based alpha fade
- **Section planes**: X/Y/Z clipping with slider control
- **GPU color-picking**: Offscreen FBO for face/edge selection with depth readback

### Selection
- **GPU picking**: Click to select faces/edges/bodies with world-position unproject
- **Hover pre-selection**: Orange highlight on mouseover
- **Multi-select**: Shift+click to add to selection
- **Filter modes**: All / Faces / Edges / Bodies (keys 1-4)
- **Selection-driven commands**: Select edges → click Fillet → fillets those specific edges

### UI
- **Dark theme** with consistent styling across all panels
- **Toolbar**: New, Open, Save, Import, Box, Cylinder, Sphere, Sketch, Extrude, Revolve, Fillet, Chamfer, Shell, Mirror, Draft, Undo, Redo
- **Feature Tree**: Hierarchical browser with context menu (Delete, Suppress)
- **Timeline Panel**: Drag-to-reorder entries, draggable rollback marker, dimmed/suppressed styling
- **Properties Panel**: Reads/writes actual feature parameters with bidirectional binding
- **Right-click context menus**: Face/edge/body-specific actions
- **Keyboard shortcuts**: E(xtrude), F(illet), S(ketch), H(ole), W(ireframe), Delete, etc.
- **Measure tool**: Distance and angle measurement

### File I/O
- **Native format**: `.kcd` (JSON) with full parametric history
- **Import**: STEP (`.step`, `.stp`), IGES (`.igs`, `.iges`)
- **Export**: STEP (all bodies), STL (all bodies)

### Undo/Redo
- Full **command pattern** with undo/redo stacks for all operations
- 30+ undoable commands covering every feature type, deletion, suppression, and timeline navigation

## Building

### Prerequisites

- CMake 3.20+
- Qt6 (Core, Widgets, OpenGLWidgets, Gui)
- OpenCASCADE 7.6+ (OCCT)
- C++17 compiler (GCC 11+, Clang 14+, MSVC 2019+)

### Ubuntu/Debian

```bash
sudo apt install qt6-base-dev libqt6opengl6-dev libqt6openglwidgets6 \
    libocct-foundation-dev libocct-modeling-data-dev \
    libocct-modeling-algorithms-dev libocct-data-exchange-dev \
    libocct-visualization-dev libtbb-dev
```

### Build

```bash
cmake -B build
cmake --build build -j$(nproc)
```

### Run

```bash
./build/src/kernelCAD
```

### Tests

```bash
cd build && ctest --output-on-failure
```

4 test suites: SketchSolver, Timeline, DependencyGraph, ParameterStore.

## Architecture

```
src/
├── kernel/          OCCT wrapper (primitives, booleans, features, tessellation, stable naming)
├── document/        Document model (timeline, dependency graph, parameters, commands, serializer)
├── features/        25+ feature types with execute() and parameter structs
├── sketch/          2D sketch system (entities, constraints, LM solver, profile detection)
├── ui/              Qt6 GUI (viewport, tree, timeline, properties, sketch editor, measure tool)
└── app/             Application entry point and dark theme setup
```

### Key Design Decisions

- **Pimpl pattern** on OCCTKernel to isolate OCCT headers from the rest of the codebase
- **Command pattern** for undo/redo — every user action is a reversible Command object
- **Geometry-based stable naming** instead of OCCT TNaming — simpler, portable, works across serialization
- **Recursive-descent expression parser** — zero external dependencies (no exprtk/muParser)
- **GPU color-picking** instead of CPU raycasting — fast and accurate for complex meshes

## License

MIT
