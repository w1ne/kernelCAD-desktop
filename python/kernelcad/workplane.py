"""
CadQuery-style fluent API for kernelCAD.

Usage:
    from kernelcad import Workplane

    part = (
        Workplane("XY")
        .rect(80, 60)
        .extrude(20)
        .fillet(3)
        .shell(2)
        .exportStep("bracket.step")
    )
"""

from typing import Optional, List
from .engine import Engine


class Workplane:
    """Fluent CAD modeling API — chain operations to build geometry."""

    def __init__(self, plane: str = "XY", engine: Optional[Engine] = None):
        self._engine = engine or Engine()
        self._engine.new_document()
        self._plane = plane
        self._sketch_id: Optional[str] = None
        self._body_id: Optional[str] = None
        self._point_ids: List[str] = []
        self._line_ids: List[str] = []

    @property
    def engine(self) -> Engine:
        return self._engine

    @property
    def body_id(self) -> Optional[str]:
        return self._body_id

    # ── Sketch creation ──────────────────────────────────────────────────

    def _ensure_sketch(self):
        if self._sketch_id is None:
            result = self._engine.create_sketch(self._plane)
            self._sketch_id = result["sketchId"]

    # ── 2D Drawing (sketch) ──────────────────────────────────────────────

    def rect(self, width: float, height: float, centered: bool = True) -> "Workplane":
        """Draw a rectangle on the sketch plane."""
        self._ensure_sketch()
        if centered:
            x1, y1 = -width / 2, -height / 2
            x2, y2 = width / 2, height / 2
        else:
            x1, y1 = 0, 0
            x2, y2 = width, height

        result = self._engine.sketch_add_rectangle(self._sketch_id, x1, y1, x2, y2)
        self._point_ids = [p["id"] for p in result.get("pointIds", [])]
        self._line_ids = [l["id"] for l in result.get("lineIds", [])]
        self._engine.sketch_solve(self._sketch_id)
        return self

    def circle(self, radius: float) -> "Workplane":
        """Draw a circle centered at origin on the sketch plane."""
        self._ensure_sketch()
        cp = self._engine.sketch_add_point(self._sketch_id, 0, 0)
        self._engine.sketch_add_circle(self._sketch_id, cp["pointId"], radius)
        self._engine.sketch_solve(self._sketch_id)
        return self

    def line(self, x1: float, y1: float, x2: float, y2: float) -> "Workplane":
        """Draw a line on the sketch plane."""
        self._ensure_sketch()
        p1 = self._engine.sketch_add_point(self._sketch_id, x1, y1)
        p2 = self._engine.sketch_add_point(self._sketch_id, x2, y2)
        self._engine.sketch_add_line(self._sketch_id, p1["pointId"], p2["pointId"])
        self._engine.sketch_solve(self._sketch_id)
        return self

    def polygon(self, points: List[tuple]) -> "Workplane":
        """Draw a closed polygon from a list of (x, y) tuples."""
        self._ensure_sketch()
        pt_ids = []
        for x, y in points:
            r = self._engine.sketch_add_point(self._sketch_id, x, y)
            pt_ids.append(r["pointId"])

        for i in range(len(pt_ids)):
            j = (i + 1) % len(pt_ids)
            self._engine.sketch_add_line(self._sketch_id, pt_ids[i], pt_ids[j])

        self._engine.sketch_solve(self._sketch_id)
        return self

    # ── 3D Operations ────────────────────────────────────────────────────

    def extrude(self, distance: float, symmetric: bool = False) -> "Workplane":
        """Extrude the current sketch profile."""
        if self._sketch_id:
            result = self._engine.extrude(
                self._sketch_id, distance, symmetric=symmetric
            )
            self._body_id = result.get("bodyId", self._body_id)
            self._sketch_id = None  # sketch consumed
        return self

    def revolve(self, angle: float = 360) -> "Workplane":
        """Revolve the current sketch profile around the Z axis."""
        if self._sketch_id:
            result = self._engine._send(
                "revolve", sketchId=self._sketch_id, angle=angle
            )
            self._body_id = result.get("bodyId", self._body_id)
            self._sketch_id = None
        return self

    # ── Modifications ────────────────────────────────────────────────────

    def fillet(self, radius: float) -> "Workplane":
        """Fillet all edges of the current body."""
        if self._body_id:
            self._engine.fillet(self._body_id, radius)
        return self

    def chamfer(self, distance: float) -> "Workplane":
        """Chamfer all edges of the current body."""
        if self._body_id:
            self._engine.chamfer(self._body_id, distance)
        return self

    def shell(self, thickness: float) -> "Workplane":
        """Shell the current body (hollow it out)."""
        if self._body_id:
            self._engine.shell(self._body_id, thickness)
        return self

    def mirror(self, nx: float = 1, ny: float = 0, nz: float = 0) -> "Workplane":
        """Mirror the current body about a plane through origin."""
        if self._body_id:
            result = self._engine.mirror(self._body_id, nx, ny, nz)
            self._body_id = result.get("bodyId", self._body_id)
        return self

    def pattern(self, count: int, angle: float = 360) -> "Workplane":
        """Circular pattern of the current body around Z axis."""
        if self._body_id:
            result = self._engine.circular_pattern(self._body_id, count, angle)
            self._body_id = result.get("bodyId", self._body_id)
        return self

    # ── Primitives (skip sketch) ─────────────────────────────────────────

    def box(self, dx: float, dy: float, dz: float) -> "Workplane":
        """Create a box primitive directly."""
        result = self._engine.create_box(dx, dy, dz)
        self._body_id = result.get("bodyId")
        return self

    def cylinder(self, radius: float, height: float) -> "Workplane":
        """Create a cylinder primitive."""
        result = self._engine.create_cylinder(radius, height)
        self._body_id = result.get("bodyId")
        return self

    def sphere(self, radius: float) -> "Workplane":
        """Create a sphere primitive."""
        result = self._engine.create_sphere(radius)
        self._body_id = result.get("bodyId")
        return self

    def torus(self, major_radius: float, minor_radius: float) -> "Workplane":
        """Create a torus primitive."""
        result = self._engine.create_torus(major_radius, minor_radius)
        self._body_id = result.get("bodyId")
        return self

    # ── Queries ──────────────────────────────────────────────────────────

    def properties(self) -> dict:
        """Get physical properties of the current body."""
        if self._body_id:
            return self._engine.get_properties(self._body_id)
        return {}

    def faces(self) -> int:
        """Count faces of the current body."""
        if self._body_id:
            return self._engine.face_count(self._body_id)
        return 0

    def edges(self) -> int:
        """Count edges of the current body."""
        if self._body_id:
            return self._engine.edge_count(self._body_id)
        return 0

    # ── Export ────────────────────────────────────────────────────────────

    def exportStep(self, path: str) -> "Workplane":
        """Export to STEP format."""
        self._engine.export_step(path)
        return self

    def exportStl(self, path: str) -> "Workplane":
        """Export to STL format."""
        self._engine.export_stl(path)
        return self

    def save(self, path: str) -> "Workplane":
        """Save as .kcd (native format with parametric history)."""
        self._engine.save(path)
        return self
