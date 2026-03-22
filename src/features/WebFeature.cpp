#include "WebFeature.h"
#include "../kernel/OCCTKernel.h"
#include "../sketch/SketchToShape.h"
#include "../sketch/Sketch.h"
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <vector>
#include <string>

namespace features {

WebFeature::WebFeature(std::string id, WebParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape WebFeature::execute(kernel::OCCTKernel& kernel,
                                  const TopoDS_Shape& body,
                                  const sketch::Sketch* sketch) const
{
    TopoDS_Shape profileShape;
    if (sketch && !m_params.profileId.empty()) {
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
        profileShape = sketch::profileToFace(*sketch, curveIds);
    }
    return kernel.web(body, profileShape, m_params.thickness, m_params.depth,
                      m_params.count, m_params.spacing);
}

} // namespace features
