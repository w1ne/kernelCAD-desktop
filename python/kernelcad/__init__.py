"""
kernelcad — Python API for parametric CAD modeling.

Uses the kernelCAD engine (C++/OCCT) via JSON-RPC over stdin/stdout.

Usage:
    from kernelcad import Workplane

    result = (
        Workplane("XY")
        .rect(80, 60)
        .extrude(20)
        .fillet(3)
        .exportStep("part.step")
    )
"""

from .workplane import Workplane
from .engine import Engine

__version__ = "0.1.0"
__all__ = ["Workplane", "Engine"]
