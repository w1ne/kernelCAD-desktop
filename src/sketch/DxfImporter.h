#pragma once
#include <string>
#include <vector>

namespace sketch {

/// Result of importing a DXF file into sketch geometry.
struct DxfImportResult {
    struct Line { double x1, y1, x2, y2; };
    struct Circle { double cx, cy, radius; };
    struct Arc { double cx, cy, radius, startAngle, endAngle; };

    std::vector<Line> lines;
    std::vector<Circle> circles;
    std::vector<Arc> arcs;
};

/// Imports basic 2D entities from a DXF file (LINE, CIRCLE, ARC, LWPOLYLINE).
class DxfImporter {
public:
    /// Parse a DXF file and return the extracted geometry.
    static DxfImportResult importFile(const std::string& path);
};

} // namespace sketch
