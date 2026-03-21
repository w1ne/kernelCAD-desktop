#pragma once
#include "Feature.h"
#include "../sketch/Sketch.h"
#include <memory>

namespace features {

struct SketchParams {
    std::string planeId;  // "XY", "XZ", "YZ", or a face ID
    // Plane origin and axes are set when the sketch is created
    double originX = 0, originY = 0, originZ = 0;
    double xDirX = 1, xDirY = 0, xDirZ = 0;
    double yDirX = 0, yDirY = 1, yDirZ = 0;
};

class SketchFeature : public Feature
{
public:
    explicit SketchFeature(std::string id, SketchParams params);

    FeatureType type() const override { return FeatureType::Sketch; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Sketch"; }

    sketch::Sketch&       sketch()       { return *m_sketch; }
    const sketch::Sketch& sketch() const { return *m_sketch; }

    const SketchParams& params() const { return m_params; }

private:
    std::string m_id;
    SketchParams m_params;
    std::unique_ptr<sketch::Sketch> m_sketch;
};

} // namespace features
