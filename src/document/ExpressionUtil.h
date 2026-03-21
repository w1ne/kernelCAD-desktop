#pragma once
// ---------------------------------------------------------------------------
// ExpressionUtil.h -- header-only utilities for parsing dimension expressions
// like "50 mm", "10 deg", "width/2 + 1 cm".
//
// Strips a trailing unit suffix, evaluates the numeric/expression part via
// ParameterStore::evaluate(), and optionally converts to internal units
// (millimetres for distances, radians for angles).
// ---------------------------------------------------------------------------

#include "ParameterStore.h"
#include <string>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace document {

// ---- Unit conversion factors ------------------------------------------------

enum class UnitKind { Distance, Angle, None };

struct UnitInfo {
    const char* suffix;
    UnitKind    kind;
    double      toBase;  // multiply by this to get base unit (mm or rad)
};

// Order matters: longer suffixes must come before shorter ones that are
// prefixes (e.g. "cm" before "m", "deg" before "d").
inline constexpr UnitInfo kKnownUnits[] = {
    { "mm",  UnitKind::Distance, 1.0 },
    { "cm",  UnitKind::Distance, 10.0 },
    { "in",  UnitKind::Distance, 25.4 },
    { "m",   UnitKind::Distance, 1000.0 },
    { "deg", UnitKind::Angle,    M_PI / 180.0 },
    { "rad", UnitKind::Angle,    1.0 },
};

// ---- Helpers ----------------------------------------------------------------

namespace detail {

inline std::string trimRight(const std::string& s) {
    auto end = s.find_last_not_of(" \t\r\n");
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

/// Try to strip a unit suffix from the end of `expr`.
/// On success, fills `factor` and shortens `expr` in-place.
/// Returns true if a unit was found.
inline bool stripUnitSuffix(std::string& expr, double& factor) {
    std::string trimmed = trimRight(expr);
    for (const auto& u : kKnownUnits) {
        std::string suf(u.suffix);
        if (trimmed.size() > suf.size()) {
            size_t pos = trimmed.size() - suf.size();
            if (trimmed.compare(pos, suf.size(), suf) == 0) {
                // Make sure the character before the suffix is not a letter
                // or underscore (avoid matching e.g. "param" ending in "m").
                // Digits and operators/spaces are fine.
                char before = trimmed[pos - 1];
                if (std::isalpha(static_cast<unsigned char>(before)) || before == '_')
                    continue;
                factor = u.toBase;
                // Remove the suffix (and any trailing whitespace before it)
                expr = trimRight(trimmed.substr(0, pos));
                return true;
            }
        }
    }
    factor = 1.0;
    return false;
}

} // namespace detail

// ---- Public API -------------------------------------------------------------

/// Parse a distance expression such as "50 mm", "width/2 cm", "10".
/// The result is always in millimetres.  If no unit suffix is present the
/// value is returned as-is (assumed to be in mm already).
inline double parseDistanceExpr(const std::string& expr,
                                const ParameterStore& params) {
    std::string work = expr;
    double factor = 1.0;
    detail::stripUnitSuffix(work, factor);
    double value = params.evaluate(work);
    return value * factor;
}

/// Parse an angle expression such as "45 deg", "PI/4 rad", "offset * 2 deg".
/// The result is always in radians.  If no unit suffix is present the value is
/// returned as-is (assumed to be in radians already).
inline double parseAngleExpr(const std::string& expr,
                             const ParameterStore& params) {
    std::string work = expr;
    double factor = 1.0;
    detail::stripUnitSuffix(work, factor);
    double value = params.evaluate(work);
    return value * factor;
}

/// Generic version: strips any recognised unit suffix, applies the conversion
/// factor, and returns the result.
inline double parseDimensionExpr(const std::string& expr,
                                 const ParameterStore& params) {
    std::string work = expr;
    double factor = 1.0;
    detail::stripUnitSuffix(work, factor);
    double value = params.evaluate(work);
    return value * factor;
}

} // namespace document
