#include "MoveFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>

namespace features {

MoveFeature::MoveFeature(std::string id, MoveParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape MoveFeature::execute(kernel::OCCTKernel& kernel,
                                   const TopoDS_Shape& targetShape) const
{
    switch (m_params.mode) {
    case MoveMode::TranslateXYZ:
        return kernel.translate(targetShape, m_params.dx, m_params.dy, m_params.dz);

    case MoveMode::Rotate:
        return kernel.rotate(targetShape,
                             m_params.axisOx, m_params.axisOy, m_params.axisOz,
                             m_params.axisDx, m_params.axisDy, m_params.axisDz,
                             m_params.angleDeg);

    case MoveMode::FreeTransform:
        return kernel.transform(targetShape, m_params.matrix);
    }

    return targetShape; // unreachable, but silences warnings
}

} // namespace features
