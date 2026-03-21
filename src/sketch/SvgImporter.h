#pragma once
#include <string>
#include <vector>

namespace sketch {

/// Result of importing an SVG file into sketch geometry.
struct SvgImportResult {
    struct Line { double x1, y1, x2, y2; };
    struct Circle { double cx, cy, radius; };
    struct Rect { double x, y, width, height; };

    std::vector<Line> lines;
    std::vector<Circle> circles;
    std::vector<Rect> rects;
};

/// Imports basic 2D primitives from an SVG file (<line>, <circle>, <rect>).
/// This is a simplified parser that handles common SVG elements, not the full spec.
class SvgImporter {
public:
    /// Parse an SVG file and return the extracted geometry.
    static SvgImportResult importFile(const std::string& path);
};

} // namespace sketch
