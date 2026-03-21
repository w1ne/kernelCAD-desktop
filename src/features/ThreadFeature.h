#pragma once
#include "Feature.h"
#include "../kernel/StableReference.h"
#include <string>
#include <vector>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

enum class ThreadType {
    MetricCoarse,    // M6, M8, M10, etc.
    MetricFine,      // M6x0.75, M8x1, etc.
    UNC,             // Unified National Coarse
    UNF              // Unified National Fine
};

struct ThreadParams {
    std::string targetBodyId;
    int cylindricalFaceIndex = -1;  // -1 = auto-detect first cylindrical face
    ThreadType threadType = ThreadType::MetricCoarse;
    double pitch = 1.0;          // mm
    double depth = 0.3;          // radial cut depth mm
    bool isInternal = true;      // hole thread vs shaft thread
    bool isRightHanded = true;
    bool isModeled = false;      // true = physical geometry, false = cosmetic only
    bool isFullLength = true;
    double threadLength = 0;     // if not full length, length of threaded section
    double threadOffset = 0;     // offset from edge

    /// Stable geometric signature for the cylindrical face.
    /// Empty if auto-detect mode (cylindricalFaceIndex == -1).
    std::vector<kernel::FaceSignature> faceSignatures;
};

class ThreadFeature : public Feature
{
public:
    explicit ThreadFeature(std::string id, ThreadParams params);

    FeatureType type() const override { return FeatureType::Thread; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Thread"; }

    ThreadParams&       params()       { return m_params; }
    const ThreadParams& params() const { return m_params; }

    /// Execute thread on the target shape and return the modified shape.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape) const;

private:
    std::string  m_id;
    ThreadParams m_params;
};

} // namespace features
