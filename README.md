# kernelCAD Desktop

Open-source parametric CAD application built with C++17, Qt6, and OpenCASCADE Technology (OCCT).

![License](https://img.shields.io/badge/license-MIT-blue)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![Lines of Code](https://img.shields.io/badge/lines-46K+-green)

## Features

### Modeling
- **29 feature types**: Extrude, Revolve, Fillet, Chamfer, Sweep, Loft, Shell, Mirror, Rectangular/Circular/Path Pattern, Coil, Hole, Thread, Draft, Scale, Thicken, Combine, SplitBody, OffsetFaces, DeleteFace, ReplaceFace, ReverseNormal, Move
- **Primitives**: Box, Cylinder, Sphere
- **Boolean operations**: Union, Cut, Intersect
- **Physical properties**: Volume, surface area, mass, center of gravity, inertia tensor (via BRepGProp)
- **Materials**: 11 preset materials (Steel, Aluminum, Brass, Copper, Plastic, Wood, Glass, Rubber, Gold, Titanium, Carbon Fiber) with per-body and per-face assignment

### Sketch System
- **Entities**: Points, Lines, Circles, Arcs, Ellipses, B-Splines
- **16 interactive tools**: Line, Rectangle, Center Rectangle, Circle, 3-Point Circle, Arc, 3-Point Arc, Ellipse, Polygon, Slot, Spline, Trim, Offset, Fillet, Chamfer, Project Edge
- **18 constraint types**: Coincident, Parallel, Perpendicular, Tangent, Distance, Angle, Radius, Horizontal, Vertical, Equal, Symmetric, Midpoint, Concentric, Fix, PointOnLine, PointOnCircle, FixedAngle, AngleBetween
- **Newton-Raphson constraint solver** (1085 lines) with analytical Jacobian, drag mode, and convergence control
- **Auto-constraints**: Horizontal/Vertical auto-applied when lines are within 3 degrees of axis
- **Dimension tool** (D key): Click entities to add parametric dimensions with expression support
- **Constraint tool** (K key): Auto-infers constraint type from selected entity pair
- **Live drag**: Grab any point, circle edge, or arc edge and drag — solver runs in real-time
- **Profile detection**: Automatic closed-loop extraction for downstream features
- **Per-curve color coding**: Blue = under-constrained, gray = fully constrained, orange = construction, red = error

### Sketch Workflow (matching Fusion 360)
1. **Choose a plane**: Click an origin plane (XY/XZ/YZ) or any flat face on a body
2. Camera **auto-rotates** to face the sketch plane; bodies go **semi-transparent**
3. **Draw geometry** with snap indicators, grid snapping, live dimension display
4. **Auto-constrain** H/V as you draw; add dimensions (D) and constraints (K)
5. **Finish sketch** (Escape) — camera restores, bodies return to full opacity
6. Status bar suggests: *"Sketch has N profiles. Press E to extrude."*

### Parametric Engine
- **Timeline**: Linear feature history with drag-to-reorder (dependency-graph validated), suppress/unsuppress, rollback marker, timeline groups (collapsible, group suppress)
- **Insert at marker**: New features insert at the rollback position, not at the end — edit history mid-stream
- **Feature editing**: Click any feature in the tree to see its manipulator; drag to edit live with preview
- **Stable naming**: Geometry-based face/edge signature matching with split-face tracking (`splitIndex`) — references survive recomputation
- **Dependency graph**: DAG with Kahn's topological sort, BFS dirty propagation, incremental recompute with input hashing (skip unchanged features)
- **Error recovery**: Failed features restore last-good geometry; downstream features continue with valid bodies; errored features tinted red in viewport
- **Expression parser**: Recursive-descent with arithmetic, `sin/cos/sqrt/pow/min/max`, `PI/E` constants, parameter references, unit stripping (`mm`, `deg`, `rad`, `cm`, `in`)
- **Parameter table**: Dockable panel to define named parameters (`width=100`, `hole_d=10`) — type expressions like `width/2 + 5` in feature property fields
- **Circular dependency detection** with BFS cycle check and automatic dependent propagation

### Assembly
- **Component/Occurrence hierarchy**: Multi-part designs with nested components
- **7 joint types**: Rigid, Revolute, Slider, Cylindrical, PinSlot, Planar, Ball — with DOF limits and Rodrigues rotation math
- **Face-to-face joint creation**: Click two faces to auto-create an aligned joint (J key)
- **Insert .kcd as component**: Import existing designs as assembly components
- **Ctrl+drag positioning**: Drag occurrences in the viewport to reposition
- **Exploded view**: Slider-controlled explosion (0-100%)
- **Interference detection**: Boolean intersection check on all body pairs

### Rendering
- **OpenGL 3.3 Core** with Blinn-Phong shading, per-vertex normals, and alpha transparency
- **4 view modes**: Solid with Edges, Solid, Wireframe, Hidden Line (Ctrl+1/2/3)
- **Per-body rendering**: Distinct colors per body, visibility toggles in feature tree, material-driven coloring
- **Origin planes**: Semi-transparent XY (blue), XZ (red), YZ (green) planes with hover highlight — click to create sketch
- **Origin axes**: RGB axis lines (X=red, Y=green, Z=blue) with white origin point
- **ViewCube**: Interactive orientation cube with click-to-snap standard views
- **Standard views**: Front/Back/Left/Right/Top/Bottom/Isometric (Numpad 1/3/7/0, Ctrl+Numpad for reverse)
- **Perspective/Orthographic toggle**: Numpad 5
- **Smooth camera transitions**: 300ms ease-in-out animation on view changes
- **Grid**: 5mm minor / 25mm major grid with distance-based alpha fade
- **Section planes**: X/Y/Z clipping with slider control (Shift+X/Y/Z)
- **GPU color-picking**: Offscreen FBO for face/edge selection with depth readback
- **Sketch overlay**: Passive sketches visible as thin lines in model mode; active sketch with full grid, snap indicators, rubber-band preview
- **Live preview**: Semi-transparent ghost geometry during parameter editing (50ms debounce)
- **Viewport manipulators**: Distance arrow for Extrude, radius handle for Fillet — drag to edit values directly in 3D

### Selection
- **GPU picking**: Click to select faces/edges/bodies with world-position unproject
- **Hover pre-selection**: Highlight on mouseover
- **Multi-select**: Shift+click to add to selection
- **Filter modes**: All / Faces / Edges / Bodies (keys 1-4)
- **Selection-driven commands**: Select edges first, then press F — auto-applies fillet with default radius
- **Toolbar hover filter**: Hovering over "Fillet" button temporarily filters to edges

### UI
- **Dark theme**: Full QPalette + Fusion stylesheet across all panels
- **Tabbed ribbon toolbar**: SOLID / SKETCH / ASSEMBLY tabs with grouped icon buttons (40+ programmatic QPainter icons)
- **Quick-access bar**: New, Open, Save, Undo, Redo (16px icons above the ribbon)
- **Marking menu**: Right-click and hold for radial context menu (8 sectors, context-sensitive)
- **Command palette**: Ctrl+K fuzzy search across 40+ commands with shortcut display
- **Feature Tree**: Hierarchical browser with component tree, body visibility checkboxes, per-type colored icons, in-place rename, context menu (Delete, Suppress, Rename, Set Material)
- **Timeline Panel**: Compact 34x34 feature-type icons, bright blue rollback marker, group brackets, drag insertion indicator, rich HTML tooltips
- **Properties Panel**: Editable forms for all 20 feature types with expression text fields (`width/2`) and live preview
- **Parameter Table**: Dockable panel for named parameters (Name/Expression/Value/Unit/Comment)
- **Right-click context menus**: Face/edge/body-specific actions in viewport
- **Keyboard shortcuts**: E(xtrude), F(illet), H(ole), J(oint), D(imension), K(constraint), M(easure), I(measure), X(construction toggle), Delete, Numpad views
- **Measure tool**: Distance and angle measurement via BRepExtrema
- **Direct interaction**: Single-click feature in tree shows manipulator immediately — no "enter edit mode" required
- **Auto-save**: 5-minute periodic auto-save with crash recovery

### File I/O
- **Native format**: `.kcd` (JSON) with full parametric history — all 29 feature types, sketches, joints, materials, timeline groups, parameters
- **Import**: STEP (`.step`, `.stp`), IGES (`.igs`, `.iges`)
- **Export**: STEP, STL, **3MF** (modern 3D printing format with units + ZIP packaging)
- **2D Drawings**: HLR-projected Front/Top/Right/Isometric views with title block — export to **PDF** and **SVG**
- **Recent files**: File > Recent remembers last 10 opened/saved files (persisted via QSettings)
- **Preferences**: Edit > Preferences for units, grid, auto-save interval, display, sketch settings

### Undo/Redo
- Full **command pattern** with undo/redo stacks
- **38+ undoable commands** covering every feature type, deletion, suppression, joints, and timeline navigation
- Dynamic menu text: *"Undo Add Extrude"*, *"Redo Delete Feature"*

## Building

### Prerequisites

- CMake 3.20+
- Qt6 (Core, Widgets, OpenGLWidgets, Gui, Test)
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
src/                           160 files, 46K lines
├── app/             (2)       Application entry point, dark theme setup
├── kernel/         (12)       OCCT wrapper, BRepModel, BRepQuery, EntityAttribute,
│                              StableReference, Appearance, IconFactory
├── document/       (32)       Document, Timeline (groups, rollback), DependencyGraph,
│                              ParameterStore (expression engine), CommandHistory (38+ commands),
│                              Serializer (1900 lines), Component/Occurrence, JointSolver,
│                              PreviewEngine, AutoSave, CommandInput, InteractiveCommands
├── features/       (60)       29 feature types + Joint + 3 construction types
├── sketch/          (8)       6 entity types, 18 constraints, Newton-Raphson solver,
│                              SketchToShape, profile detection
└── ui/             (30)       Viewport3D (3000+ lines), FeatureTree, TimelinePanel,
                               PropertiesPanel, ParameterTablePanel, SketchEditor (16 tools),
                               SelectionManager, MeasureTool, MarkingMenu, CommandPalette,
                               CommandDialog, ViewportManipulator, JointCreator, IconFactory
```

### Key Design Decisions

- **Pimpl pattern** on OCCTKernel to isolate OCCT headers from the rest of the codebase
- **Command pattern** for undo/redo — every user action is a reversible Command object
- **Geometry-based stable naming** with split-face tracking (`splitIndex`) instead of OCCT TNaming
- **Recursive-descent expression parser** — zero external dependencies (no exprtk/muParser)
- **GPU color-picking** instead of CPU raycasting — fast and accurate for complex meshes
- **Direct interaction model** — single-click shows manipulators, no modal "edit mode" required
- **Programmatic icons** via QPainter — no image assets needed, all 40+ icons drawn in code
- **Insert-at-marker** timeline — new features go where the rollback bar is, enabling mid-history editing

## License

MIT
