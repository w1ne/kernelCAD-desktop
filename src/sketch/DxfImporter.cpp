#include "DxfImporter.h"
#include <fstream>
#include <string>
#include <vector>
#include <cctype>
#include <cmath>

namespace sketch {

namespace {

/// Trim trailing whitespace and carriage returns.
void trimRight(std::string& s) {
    while (!s.empty() && (std::isspace(static_cast<unsigned char>(s.back()))))
        s.pop_back();
}

/// Trim leading whitespace.
void trimLeft(std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
        ++start;
    if (start > 0)
        s.erase(0, start);
}

void trim(std::string& s) {
    trimRight(s);
    trimLeft(s);
}

} // anonymous namespace

DxfImportResult DxfImporter::importFile(const std::string& path) {
    DxfImportResult result;
    std::ifstream file(path);
    if (!file.is_open())
        return result;

    std::string line;

    // Skip to ENTITIES section
    bool inEntities = false;
    std::string entityType;
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    double cx = 0, cy = 0, radius = 0, startAngle = 0, endAngle = 0;

    // LWPOLYLINE accumulates vertex pairs
    std::vector<std::pair<double, double>> polyVertices;
    double polyLastX = 0, polyLastY = 0;
    bool polyHasVertex = false;
    int polylineClosed = 0;  // flag 70, bit 1 = closed

    auto finalizeEntity = [&]() {
        if (entityType == "LINE") {
            result.lines.push_back({x1, y1, x2, y2});
        } else if (entityType == "CIRCLE") {
            result.circles.push_back({cx, cy, radius});
        } else if (entityType == "ARC") {
            result.arcs.push_back({cx, cy, radius, startAngle, endAngle});
        } else if (entityType == "LWPOLYLINE") {
            // Flush the last pending vertex
            if (polyHasVertex)
                polyVertices.push_back({polyLastX, polyLastY});

            // Convert polyline vertices to lines
            for (size_t i = 0; i + 1 < polyVertices.size(); ++i) {
                result.lines.push_back({
                    polyVertices[i].first, polyVertices[i].second,
                    polyVertices[i + 1].first, polyVertices[i + 1].second
                });
            }
            // Close the polyline if the closed flag is set
            if (polylineClosed && polyVertices.size() >= 2) {
                result.lines.push_back({
                    polyVertices.back().first, polyVertices.back().second,
                    polyVertices.front().first, polyVertices.front().second
                });
            }
        }
    };

    auto resetEntity = [&]() {
        x1 = y1 = x2 = y2 = cx = cy = radius = startAngle = endAngle = 0;
        polyVertices.clear();
        polyLastX = polyLastY = 0;
        polyHasVertex = false;
        polylineClosed = 0;
    };

    while (std::getline(file, line)) {
        trim(line);

        if (line == "ENTITIES") { inEntities = true; continue; }
        if (line == "ENDSEC" && inEntities) {
            // Finalize any in-progress entity
            finalizeEntity();
            inEntities = false;
            break;
        }
        if (!inEntities) continue;

        // Parse group code
        int groupCode = 0;
        try { groupCode = std::stoi(line); } catch (...) { continue; }

        // Read the value line
        if (!std::getline(file, line)) break;
        trim(line);

        if (groupCode == 0) {
            // New entity -- finalize the previous one
            finalizeEntity();
            entityType = line;
            resetEntity();
            continue;
        }

        // Parse numeric value
        double val = 0;
        try { val = std::stod(line); } catch (...) { continue; }

        if (entityType == "LINE") {
            if (groupCode == 10) x1 = val;
            else if (groupCode == 20) y1 = val;
            else if (groupCode == 11) x2 = val;
            else if (groupCode == 21) y2 = val;
        } else if (entityType == "CIRCLE") {
            if (groupCode == 10) cx = val;
            else if (groupCode == 20) cy = val;
            else if (groupCode == 40) radius = val;
        } else if (entityType == "ARC") {
            if (groupCode == 10) cx = val;
            else if (groupCode == 20) cy = val;
            else if (groupCode == 40) radius = val;
            else if (groupCode == 50) startAngle = val;
            else if (groupCode == 51) endAngle = val;
        } else if (entityType == "LWPOLYLINE") {
            if (groupCode == 70) {
                polylineClosed = static_cast<int>(val) & 1;
            } else if (groupCode == 10) {
                // A new X coordinate means a new vertex is starting.
                // Flush the previous vertex if any.
                if (polyHasVertex)
                    polyVertices.push_back({polyLastX, polyLastY});
                polyLastX = val;
                polyLastY = 0;
                polyHasVertex = true;
            } else if (groupCode == 20) {
                polyLastY = val;
            }
        }
    }

    return result;
}

} // namespace sketch
