# kernelcad-plugin
# name: Spur Gear Generator
# description: Creates parametric spur gears with involute-approximation tooth profiles
# author: kernelCAD
# version: 1.0

from kernelcad import Workplane
import math


def run(params):
    """Generate a spur gear as an extruded polygon."""
    teeth = params.get("teeth", 20)
    module = params.get("module", 2.0)
    thickness = params.get("thickness", 10.0)

    pitch_radius = module * teeth / 2.0
    samples_per_tooth = 4
    total_samples = teeth * samples_per_tooth
    points = []
    for i in range(total_samples):
        angle = 2.0 * math.pi * i / total_samples
        # Alternating addendum / dedendum to approximate tooth shape
        if (i % samples_per_tooth) < (samples_per_tooth // 2):
            r = pitch_radius + module * 0.5
        else:
            r = pitch_radius - module * 0.5
        points.append((r * math.cos(angle), r * math.sin(angle)))

    gear = Workplane("XY").polygon(points).extrude(thickness)
    return gear


parameters = [
    {"name": "teeth", "type": "int", "default": 20, "min": 6, "max": 200, "label": "Number of Teeth"},
    {"name": "module", "type": "float", "default": 2.0, "min": 0.5, "max": 20.0, "label": "Module (mm)"},
    {"name": "thickness", "type": "float", "default": 10.0, "min": 1.0, "max": 500.0, "label": "Thickness (mm)"},
]
