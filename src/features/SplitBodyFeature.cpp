#include "SplitBodyFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>

namespace features {

SplitBodyFeature::SplitBodyFeature(std::string id, SplitBodyParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape SplitBodyFeature::execute(kernel::OCCTKernel& kernel,
                                        const TopoDS_Shape& target) const
{
    // Build a large planar face from the plane parameters
    gp_Pnt origin(m_params.planeOx, m_params.planeOy, m_params.planeOz);
    gp_Dir normal(m_params.planeNx, m_params.planeNy, m_params.planeNz);
    gp_Pln plane(origin, normal);

    // Create a large bounded face (10000 x 10000 mm) so the plane fully intersects
    BRepBuilderAPI_MakeFace mkFace(plane, -5000.0, 5000.0, -5000.0, 5000.0);
    mkFace.Build();
    if (!mkFace.IsDone())
        return target;

    return kernel.splitBody(target, mkFace.Shape());
}

TopoDS_Shape SplitBodyFeature::execute(kernel::OCCTKernel& kernel,
                                        const TopoDS_Shape& target,
                                        const TopoDS_Shape& tool) const
{
    return kernel.splitBody(target, tool);
}

} // namespace features
