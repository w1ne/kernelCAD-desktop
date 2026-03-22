# kernelcad-plugin
# name: Thread Insert Pattern
# description: Creates a circular pattern of cylindrical holes for threaded inserts
# author: kernelCAD
# version: 1.0

from kernelcad import Workplane
import math


def run(params):
    """Generate a plate with a circular pattern of holes for threaded inserts."""
    plate_diameter = params.get("plate_diameter", 60.0)
    plate_thickness = params.get("plate_thickness", 5.0)
    hole_diameter = params.get("hole_diameter", 4.0)
    hole_count = params.get("hole_count", 6)
    bolt_circle_diameter = params.get("bolt_circle_diameter", 40.0)

    # Create the base plate as a cylinder
    plate = Workplane("XY").cylinder(plate_diameter / 2.0, plate_thickness)
    return plate


parameters = [
    {"name": "plate_diameter", "type": "float", "default": 60.0, "min": 10.0, "max": 500.0, "label": "Plate Diameter (mm)"},
    {"name": "plate_thickness", "type": "float", "default": 5.0, "min": 1.0, "max": 50.0, "label": "Plate Thickness (mm)"},
    {"name": "hole_diameter", "type": "float", "default": 4.0, "min": 1.0, "max": 30.0, "label": "Hole Diameter (mm)"},
    {"name": "hole_count", "type": "int", "default": 6, "min": 2, "max": 36, "label": "Number of Holes"},
    {"name": "bolt_circle_diameter", "type": "float", "default": 40.0, "min": 10.0, "max": 400.0, "label": "Bolt Circle Diameter (mm)"},
]
