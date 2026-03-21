#pragma once
#include "Feature.h"
#include <string>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

enum class HoleType { Simple, Counterbore, Countersink };
enum class HoleTapType { Simple, Clearance, Tapped, TaperTapped };

struct HoleParams {
    std::string targetBodyId;
    HoleType holeType = HoleType::Simple;

    // Position and direction
    double posX = 0, posY = 0, posZ = 0;
    double dirX = 0, dirY = 0, dirZ = -1;  // default: -Z (into top face)

    // Dimensions
    std::string diameterExpr = "10 mm";
    std::string depthExpr = "0";            // 0 = through-all
    double tipAngleDeg = 118.0;             // drill tip angle

    // Counterbore
    std::string cboreDiameterExpr = "16 mm";
    std::string cboreDepthExpr = "5 mm";

    // Countersink
    std::string csinkDiameterExpr = "20 mm";
    double csinkAngleDeg = 90.0;

    // Thread (future)
    HoleTapType tapType = HoleTapType::Simple;
};

class HoleFeature : public Feature
{
public:
    explicit HoleFeature(std::string id, HoleParams params);

    FeatureType type() const override { return FeatureType::Hole; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Hole"; }

    HoleParams&       params()       { return m_params; }
    const HoleParams& params() const { return m_params; }

    /// Execute hole on the target shape and return the modified shape.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape) const;

private:
    std::string m_id;
    HoleParams  m_params;

    /// Parse a dimension expression like "10 mm" and return the numeric value.
    static double parseExpr(const std::string& expr, double fallback);
};

} // namespace features
