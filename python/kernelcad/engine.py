"""
Low-level engine interface — communicates with kernelcad-cli via JSON over stdin/stdout.
"""

import json
import subprocess
import os
from typing import Any, Dict, List, Optional


class Engine:
    """Manages a kernelcad-cli subprocess and sends JSON commands."""

    def __init__(self, cli_path: Optional[str] = None):
        if cli_path is None:
            # Try to find kernelcad-cli relative to this file
            pkg_dir = os.path.dirname(os.path.abspath(__file__))
            candidates = [
                os.path.join(pkg_dir, "..", "..", "build", "src", "kernelcad-cli"),
                os.path.join(pkg_dir, "..", "..", "build", "kernelcad-cli"),
                "kernelcad-cli",  # hope it's on PATH
            ]
            for c in candidates:
                if os.path.isfile(c) and os.access(c, os.X_OK):
                    cli_path = c
                    break
            if cli_path is None:
                raise FileNotFoundError(
                    "Cannot find kernelcad-cli. Build the project first or pass cli_path="
                )

        self._proc = subprocess.Popen(
            [cli_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        self._id = 0

    def __del__(self):
        self.close()

    def close(self):
        if self._proc and self._proc.poll() is None:
            self._proc.stdin.close()
            self._proc.wait(timeout=5)

    def _send(self, cmd: str, **kwargs) -> Dict[str, Any]:
        self._id += 1
        msg = {"cmd": cmd, "id": self._id, **kwargs}
        line = json.dumps(msg)
        self._proc.stdin.write(line + "\n")
        self._proc.stdin.flush()

        # Read lines until we get a JSON response (skip OCCT debug output)
        while True:
            resp_line = self._proc.stdout.readline()
            if not resp_line:
                raise RuntimeError("kernelcad-cli process died")
            resp_line = resp_line.strip()
            if not resp_line:
                continue
            if resp_line.startswith("{"):
                break  # found JSON
            # else: skip non-JSON output (OCCT statistics, warnings, etc.)

        resp = json.loads(resp_line)
        if not resp.get("ok", False):
            raise RuntimeError(f"kernelcad error: {resp.get('error', 'unknown')}")
        return resp.get("result", {})

    # ── Document ─────────────────────────────────────────────────────────
    def new_document(self):
        return self._send("newDocument")

    def save(self, path: str):
        return self._send("save", path=path)

    def load(self, path: str):
        return self._send("load", path=path)

    def import_step(self, path: str) -> Dict:
        return self._send("importStep", path=path)

    def export_step(self, path: str):
        return self._send("exportStep", path=path)

    def export_stl(self, path: str):
        return self._send("exportStl", path=path)

    # ── Primitives ───────────────────────────────────────────────────────
    def create_box(self, dx: float, dy: float, dz: float) -> Dict:
        return self._send("createBox", dx=dx, dy=dy, dz=dz)

    def create_cylinder(self, radius: float, height: float) -> Dict:
        return self._send("createCylinder", radius=radius, height=height)

    def create_sphere(self, radius: float) -> Dict:
        return self._send("createSphere", radius=radius)

    def create_torus(self, major_radius: float, minor_radius: float) -> Dict:
        return self._send("createTorus", majorRadius=major_radius, minorRadius=minor_radius)

    def create_pipe(self, outer_radius: float, inner_radius: float, height: float) -> Dict:
        return self._send("createPipe", outerRadius=outer_radius, innerRadius=inner_radius, height=height)

    # ── Sketch ───────────────────────────────────────────────────────────
    def create_sketch(self, plane: str = "XY") -> Dict:
        return self._send("createSketch", plane=plane)

    def sketch_add_point(self, sketch_id: str, x: float, y: float) -> Dict:
        return self._send("sketchAddPoint", sketchId=sketch_id, x=x, y=y)

    def sketch_add_line(self, sketch_id: str, start_id: str, end_id: str) -> Dict:
        return self._send("sketchAddLine", sketchId=sketch_id, startPointId=start_id, endPointId=end_id)

    def sketch_add_rectangle(self, sketch_id: str, x1: float, y1: float, x2: float, y2: float) -> Dict:
        return self._send("sketchAddRectangle", sketchId=sketch_id, x1=x1, y1=y1, x2=x2, y2=y2)

    def sketch_add_circle(self, sketch_id: str, center_id: str, radius: float) -> Dict:
        return self._send("sketchAddCircle", sketchId=sketch_id, centerPointId=center_id, radius=radius)

    def sketch_add_constraint(self, sketch_id: str, type: str, e1: str, e2: str = "", value: float = 0) -> Dict:
        return self._send("sketchAddConstraint", sketchId=sketch_id, type=type, entity1=e1, entity2=e2, value=value)

    def sketch_solve(self, sketch_id: str) -> Dict:
        return self._send("sketchSolve", sketchId=sketch_id)

    def sketch_detect_profiles(self, sketch_id: str) -> Dict:
        return self._send("sketchDetectProfiles", sketchId=sketch_id)

    # ── Features ─────────────────────────────────────────────────────────
    def extrude(self, sketch_id: str, distance: float, **kwargs) -> Dict:
        return self._send("extrude", sketchId=sketch_id, distance=distance, **kwargs)

    def fillet(self, body_id: str, radius: float, edge_ids: Optional[List[int]] = None) -> Dict:
        params = {"bodyId": body_id, "radius": radius}
        if edge_ids is not None:
            params["edgeIds"] = edge_ids
        return self._send("fillet", **params)

    def chamfer(self, body_id: str, distance: float, edge_ids: Optional[List[int]] = None) -> Dict:
        params = {"bodyId": body_id, "distance": distance}
        if edge_ids is not None:
            params["edgeIds"] = edge_ids
        return self._send("chamfer", **params)

    def shell(self, body_id: str, thickness: float) -> Dict:
        return self._send("shell", bodyId=body_id, thickness=thickness)

    def mirror(self, body_id: str, nx: float = 1, ny: float = 0, nz: float = 0) -> Dict:
        return self._send("mirror", bodyId=body_id, planeNormalX=nx, planeNormalY=ny, planeNormalZ=nz)

    def circular_pattern(self, body_id: str, count: int, angle: float = 360) -> Dict:
        return self._send("circularPattern", bodyId=body_id, count=count, angle=angle)

    # ── Queries ──────────────────────────────────────────────────────────
    def list_bodies(self) -> List[str]:
        result = self._send("listBodies")
        return [b["id"] for b in result.get("bodyIds", [])]

    def list_features(self) -> List[Dict]:
        result = self._send("listFeatures")
        return result.get("features", [])

    def get_properties(self, body_id: str) -> Dict:
        return self._send("getProperties", bodyId=body_id)

    def face_count(self, body_id: str) -> int:
        return self._send("faceCount", bodyId=body_id)["count"]

    def edge_count(self, body_id: str) -> int:
        return self._send("edgeCount", bodyId=body_id)["count"]

    # ── Timeline ─────────────────────────────────────────────────────────
    def undo(self):
        return self._send("undo")

    def redo(self):
        return self._send("redo")

    def suppress(self, feature_id: str):
        return self._send("suppress", featureId=feature_id)

    def delete_feature(self, feature_id: str):
        return self._send("deleteFeature", featureId=feature_id)

    def recompute(self):
        return self._send("recompute")
