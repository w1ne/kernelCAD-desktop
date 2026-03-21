#include "SketchFeature.h"

namespace features {

SketchFeature::SketchFeature(std::string id, SketchParams params)
    : m_id(std::move(id))
    , m_params(std::move(params))
    , m_sketch(std::make_unique<sketch::Sketch>())
{
    // Configure the sketch's coordinate plane from the params
    m_sketch->setPlane(
        m_params.originX, m_params.originY, m_params.originZ,
        m_params.xDirX,   m_params.xDirY,   m_params.xDirZ,
        m_params.yDirX,   m_params.yDirY,   m_params.yDirZ
    );
}

} // namespace features
