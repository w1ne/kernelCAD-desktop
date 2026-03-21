#include "Joint.h"
#include <cmath>
#include <algorithm>

namespace features {

// ── 4x4 matrix helpers (column-major) ────────────────────────────────────────

static void mat4Identity(double m[16])
{
    for (int i = 0; i < 16; ++i) m[i] = 0.0;
    m[0] = m[5] = m[10] = m[15] = 1.0;
}

/// Multiply A * B -> out (all column-major 4x4).
static void mat4Mul(const double A[16], const double B[16], double out[16])
{
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k)
                sum += A[row + k * 4] * B[k + col * 4];
            out[row + col * 4] = sum;
        }
    }
}

/// Build a translation matrix.
static void mat4Translate(double m[16], double tx, double ty, double tz)
{
    mat4Identity(m);
    m[12] = tx;
    m[13] = ty;
    m[14] = tz;
}

/// Build a rotation matrix around an arbitrary axis (must be normalized).
/// Uses Rodrigues' rotation formula.
static void mat4RotateAxis(double m[16], double ax, double ay, double az, double angle)
{
    double c = std::cos(angle);
    double s = std::sin(angle);
    double t = 1.0 - c;

    mat4Identity(m);
    m[0]  = t * ax * ax + c;
    m[1]  = t * ax * ay + s * az;
    m[2]  = t * ax * az - s * ay;

    m[4]  = t * ax * ay - s * az;
    m[5]  = t * ay * ay + c;
    m[6]  = t * ay * az + s * ax;

    m[8]  = t * ax * az + s * ay;
    m[9]  = t * ay * az - s * ax;
    m[10] = t * az * az + c;
}

/// Normalize a 3D vector in-place. Returns length.
static double normalize3(double& x, double& y, double& z)
{
    double len = std::sqrt(x * x + y * y + z * z);
    if (len > 1e-15) {
        x /= len; y /= len; z /= len;
    }
    return len;
}

/// Cross product: out = a x b
static void cross3(double ax, double ay, double az,
                   double bx, double by, double bz,
                   double& ox, double& oy, double& oz)
{
    ox = ay * bz - az * by;
    oy = az * bx - ax * bz;
    oz = ax * by - ay * bx;
}

// ── Joint implementation ─────────────────────────────────────────────────────

Joint::Joint(std::string id, JointParams params)
    : m_id(std::move(id))
    , m_params(std::move(params))
{}

FeatureType Joint::type() const { return FeatureType::Joint; }

std::string Joint::id() const { return m_id; }

std::string Joint::name() const
{
    const char* typeNames[] = {
        "Rigid Joint", "Revolute Joint", "Slider Joint",
        "Cylindrical Joint", "Pin-Slot Joint", "Planar Joint", "Ball Joint"
    };
    int idx = static_cast<int>(m_params.jointType);
    if (idx >= 0 && idx < 7)
        return typeNames[idx];
    return "Joint";
}

JointParams& Joint::params() { return m_params; }
const JointParams& Joint::params() const { return m_params; }

int Joint::dofCount() const
{
    switch (m_params.jointType) {
    case JointType::Rigid:       return 0;
    case JointType::Revolute:    return 1;
    case JointType::Slider:      return 1;
    case JointType::Cylindrical: return 2;
    case JointType::PinSlot:     return 2;
    case JointType::Planar:      return 3;
    case JointType::Ball:        return 3;
    }
    return 0;
}

void Joint::setDOFValue(int dofIndex, double value)
{
    // Clamp helper
    auto clamp = [](double v, const JointLimits& lim) -> double {
        if (lim.hasMin && v < lim.minValue) v = lim.minValue;
        if (lim.hasMax && v > lim.maxValue) v = lim.maxValue;
        return v;
    };

    switch (m_params.jointType) {
    case JointType::Rigid:
        break; // no DOFs
    case JointType::Revolute:
        if (dofIndex == 0) m_params.rotationValue = clamp(value, m_params.rotationLimits);
        break;
    case JointType::Slider:
        if (dofIndex == 0) m_params.translationValue = clamp(value, m_params.translationLimits);
        break;
    case JointType::Cylindrical:
        if (dofIndex == 0) m_params.rotationValue = clamp(value, m_params.rotationLimits);
        if (dofIndex == 1) m_params.translationValue = clamp(value, m_params.translationLimits);
        break;
    case JointType::PinSlot:
        if (dofIndex == 0) m_params.rotationValue = clamp(value, m_params.rotationLimits);
        if (dofIndex == 1) m_params.translationValue = clamp(value, m_params.translationLimits);
        break;
    case JointType::Planar:
        if (dofIndex == 0) m_params.translationValue = clamp(value, m_params.translationLimits);
        if (dofIndex == 1) m_params.translation2Value = value;
        if (dofIndex == 2) m_params.rotationValue = clamp(value, m_params.rotationLimits);
        break;
    case JointType::Ball:
        if (dofIndex == 0) m_params.rotationValue = clamp(value, m_params.rotationLimits);
        if (dofIndex == 1) m_params.rotation2Value = value;
        if (dofIndex == 2) m_params.rotation3Value = value;
        break;
    }
}

double Joint::getDOFValue(int dofIndex) const
{
    switch (m_params.jointType) {
    case JointType::Rigid:
        return 0.0;
    case JointType::Revolute:
        return (dofIndex == 0) ? m_params.rotationValue : 0.0;
    case JointType::Slider:
        return (dofIndex == 0) ? m_params.translationValue : 0.0;
    case JointType::Cylindrical:
        if (dofIndex == 0) return m_params.rotationValue;
        if (dofIndex == 1) return m_params.translationValue;
        return 0.0;
    case JointType::PinSlot:
        if (dofIndex == 0) return m_params.rotationValue;
        if (dofIndex == 1) return m_params.translationValue;
        return 0.0;
    case JointType::Planar:
        if (dofIndex == 0) return m_params.translationValue;
        if (dofIndex == 1) return m_params.translation2Value;
        if (dofIndex == 2) return m_params.rotationValue;
        return 0.0;
    case JointType::Ball:
        if (dofIndex == 0) return m_params.rotationValue;
        if (dofIndex == 1) return m_params.rotation2Value;
        if (dofIndex == 2) return m_params.rotation3Value;
        return 0.0;
    }
    return 0.0;
}

void Joint::computeTransform(double outMatrix[16]) const
{
    if (m_params.isLocked || m_params.isSuppressed) {
        mat4Identity(outMatrix);
        return;
    }

    const auto& g1 = m_params.geometryOne;
    const auto& g2 = m_params.geometryTwo;

    // Primary axis of geometry one (normalized)
    double pax = g1.primaryAxisX, pay = g1.primaryAxisY, paz = g1.primaryAxisZ;
    normalize3(pax, pay, paz);

    // Secondary axis of geometry one (normalized)
    double sax = g1.secondaryAxisX, say = g1.secondaryAxisY, saz = g1.secondaryAxisZ;
    normalize3(sax, say, saz);

    // Normal = primary x secondary
    double nax, nay, naz;
    cross3(pax, pay, paz, sax, say, saz, nax, nay, naz);
    normalize3(nax, nay, naz);

    // Step 1: Translate to geometryOne origin
    double T1[16];
    mat4Translate(T1, g1.originX, g1.originY, g1.originZ);

    // Step 3: Translate back from geometryTwo origin
    double T2inv[16];
    mat4Translate(T2inv, -g2.originX, -g2.originY, -g2.originZ);

    // Step 2: Build the joint motion matrix
    double motion[16];
    mat4Identity(motion);

    switch (m_params.jointType) {

    case JointType::Rigid:
        // Identity -- occurrences are fixed at joint geometry alignment
        mat4Identity(motion);
        break;

    case JointType::Revolute: {
        // Rotate around primaryAxis by rotationValue
        mat4RotateAxis(motion, pax, pay, paz, m_params.rotationValue);
        break;
    }

    case JointType::Slider: {
        // Translate along primaryAxis by translationValue
        mat4Translate(motion,
                      pax * m_params.translationValue,
                      pay * m_params.translationValue,
                      paz * m_params.translationValue);
        break;
    }

    case JointType::Cylindrical: {
        // Rotation around primaryAxis + translation along same axis
        double rot[16], trans[16];
        mat4RotateAxis(rot, pax, pay, paz, m_params.rotationValue);
        mat4Translate(trans,
                      pax * m_params.translationValue,
                      pay * m_params.translationValue,
                      paz * m_params.translationValue);
        mat4Mul(trans, rot, motion);
        break;
    }

    case JointType::PinSlot: {
        // Rotation around primaryAxis + translation along secondaryAxis
        double rot[16], trans[16];
        mat4RotateAxis(rot, pax, pay, paz, m_params.rotationValue);
        mat4Translate(trans,
                      sax * m_params.translationValue,
                      say * m_params.translationValue,
                      saz * m_params.translationValue);
        mat4Mul(trans, rot, motion);
        break;
    }

    case JointType::Planar: {
        // Translation in primaryAxis/secondaryAxis plane + rotation around normal
        double trans[16], rot[16];
        mat4Translate(trans,
                      pax * m_params.translationValue + sax * m_params.translation2Value,
                      pay * m_params.translationValue + say * m_params.translation2Value,
                      paz * m_params.translationValue + saz * m_params.translation2Value);
        mat4RotateAxis(rot, nax, nay, naz, m_params.rotationValue);
        mat4Mul(trans, rot, motion);
        break;
    }

    case JointType::Ball: {
        // Full 3D rotation using Euler angles:
        //   rotation around primaryAxis, then secondaryAxis, then normal
        double r1[16], r2[16], r3[16], tmp[16];
        mat4RotateAxis(r1, pax, pay, paz, m_params.rotationValue);
        mat4RotateAxis(r2, sax, say, saz, m_params.rotation2Value);
        mat4RotateAxis(r3, nax, nay, naz, m_params.rotation3Value);
        mat4Mul(r2, r1, tmp);
        mat4Mul(r3, tmp, motion);
        break;
    }

    } // switch

    // Final transform = T1 * motion * T2inv
    double tmp[16];
    mat4Mul(T1, motion, tmp);
    mat4Mul(tmp, T2inv, outMatrix);
}

} // namespace features
