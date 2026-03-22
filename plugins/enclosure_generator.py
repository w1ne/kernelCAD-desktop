# kernelcad-plugin
# name: Enclosure Generator
# description: Creates a rectangular box enclosure with rounded edges and a shell
# author: kernelCAD
# version: 1.0

from kernelcad import Workplane


def run(params):
    """Generate a shelled box enclosure."""
    width = params.get("width", 100.0)
    depth = params.get("depth", 60.0)
    height = params.get("height", 30.0)
    wall = params.get("wall_thickness", 2.0)
    fillet_r = params.get("fillet_radius", 3.0)

    enclosure = (
        Workplane("XY")
        .rect(width, depth)
        .extrude(height)
        .fillet(fillet_r)
        .shell(wall)
    )
    return enclosure


parameters = [
    {"name": "width", "type": "float", "default": 100.0, "min": 10.0, "max": 500.0, "label": "Width (mm)"},
    {"name": "depth", "type": "float", "default": 60.0, "min": 10.0, "max": 500.0, "label": "Depth (mm)"},
    {"name": "height", "type": "float", "default": 30.0, "min": 5.0, "max": 300.0, "label": "Height (mm)"},
    {"name": "wall_thickness", "type": "float", "default": 2.0, "min": 0.5, "max": 20.0, "label": "Wall Thickness (mm)"},
    {"name": "fillet_radius", "type": "float", "default": 3.0, "min": 0.5, "max": 20.0, "label": "Fillet Radius (mm)"},
]
