#pragma once
#include <vector>
#include <memory>

namespace features { class Feature; }

namespace document {

class ComponentRegistry;

/// A simple solver that applies joint transforms to component occurrences.
/// For each Joint feature, it looks up the two occurrences, calls
/// computeTransform(), and applies the result to occurrence two's transform.
class JointSolver {
public:
    /// Apply all joint constraints, updating occurrence transforms.
    static void solve(ComponentRegistry& components,
                      const std::vector<std::shared_ptr<features::Feature>>& joints);
};

} // namespace document
