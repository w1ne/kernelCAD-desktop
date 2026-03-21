#include "JointSolver.h"
#include "Component.h"
#include "../features/Joint.h"

namespace document {

/// Search all components for an occurrence with the given ID.
static Component::Occurrence* findOccurrenceGlobal(
    ComponentRegistry& components, const std::string& occId)
{
    for (const auto& compId : components.componentIds()) {
        auto* comp = components.findComponent(compId);
        if (!comp) continue;
        auto* occ = comp->findOccurrence(occId);
        if (occ) return occ;
    }
    return nullptr;
}

void JointSolver::solve(ComponentRegistry& components,
                        const std::vector<std::shared_ptr<features::Feature>>& joints)
{
    for (const auto& feat : joints) {
        if (!feat || feat->type() != features::FeatureType::Joint)
            continue;

        auto* joint = static_cast<features::Joint*>(feat.get());
        const auto& jp = joint->params();

        // Skip suppressed or locked joints
        if (jp.isSuppressed)
            continue;

        // Find the two occurrences
        auto* occ2 = findOccurrenceGlobal(components, jp.occurrenceTwoId);
        if (!occ2)
            continue;

        // Do not move grounded occurrences
        if (occ2->isGrounded)
            continue;

        // Compute the joint transform and apply to occurrence two
        double matrix[16];
        joint->computeTransform(matrix);

        // If occurrence one exists and has a non-identity transform, compose:
        // occ2.transform = occ1.transform * jointMotion
        auto* occ1 = findOccurrenceGlobal(components, jp.occurrenceOneId);
        if (occ1) {
            // Multiply occ1.transform * jointMatrix -> occ2.transform
            // Both are column-major 4x4
            double result[16];
            for (int col = 0; col < 4; ++col) {
                for (int row = 0; row < 4; ++row) {
                    double sum = 0.0;
                    for (int k = 0; k < 4; ++k)
                        sum += occ1->transform[row + k * 4] * matrix[k + col * 4];
                    result[row + col * 4] = sum;
                }
            }
            for (int i = 0; i < 16; ++i)
                occ2->transform[i] = result[i];
        } else {
            // No occurrence one found -- just apply the joint transform directly
            for (int i = 0; i < 16; ++i)
                occ2->transform[i] = matrix[i];
        }
    }
}

} // namespace document
