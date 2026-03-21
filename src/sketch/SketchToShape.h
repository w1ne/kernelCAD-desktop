#pragma once
#include <vector>
#include <string>

class TopoDS_Face;
class TopoDS_Wire;

namespace sketch {
class Sketch;
}

namespace sketch {

/// Convert a sketch profile (closed loop of curve IDs) into an OCCT face.
/// The sketch's coordinate transform is used to place the wire in 3D.
TopoDS_Face profileToFace(const Sketch& sketch, const std::vector<std::string>& profileCurveIds);

/// Convert a single closed loop of sketch curves into a 3D wire.
TopoDS_Wire profileToWire(const Sketch& sketch, const std::vector<std::string>& profileCurveIds);

} // namespace sketch
