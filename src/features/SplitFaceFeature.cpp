#include "SplitFaceFeature.h"
#include "../kernel/OCCTKernel.h"
#include "../sketch/SketchToShape.h"
#include "../sketch/Sketch.h"
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <vector>
#include <string>
#include <sstream>

namespace features {

SplitFaceFeature::SplitFaceFeature(std::string id, SplitFaceParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape SplitFaceFeature::execute(kernel::OCCTKernel& kernel,
                                        const TopoDS_Shape& targetShape,
                                        const sketch::Sketch* sketch) const
{
    TopoDS_Shape wireShape;
    if (sketch && !m_params.profileId.empty()) {
        // Parse comma-separated curve IDs
        std::vector<std::string> curveIds;
        std::string remaining = m_params.profileId;
        while (!remaining.empty()) {
            size_t commaPos = remaining.find(',');
            if (commaPos == std::string::npos) {
                curveIds.push_back(remaining);
                break;
            }
            curveIds.push_back(remaining.substr(0, commaPos));
            remaining = remaining.substr(commaPos + 1);
        }
        wireShape = sketch::profileToWire(*sketch, curveIds);
    }
    return kernel.splitFace(targetShape, m_params.faceIndex, wireShape);
}

} // namespace features
