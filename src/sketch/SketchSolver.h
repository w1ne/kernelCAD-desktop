#pragma once
#include "SketchEntity.h"
#include "SketchConstraint.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <functional>
#include <random>

namespace sketch {

enum class SolveStatus {
    Solved,
    Failed,
    OverConstrained
};

struct SolveResult {
    SolveStatus status = SolveStatus::Failed;
    int iterations = 0;
    double residual = 0.0;
    std::string message;   // diagnostic info (e.g. which constraints are redundant)
};

/// Newton-Raphson constraint solver for 2D sketch geometry.
/// Uses dense matrices with Gaussian elimination (LU) -- suitable for
/// small-to-medium sketches (< ~200 parameters).
class SketchSolver {
public:
    SketchSolver() = default;

    /// Load entities and constraints; call solve() afterwards.
    void setEntities(
        std::unordered_map<std::string, SketchPoint>& points,
        std::unordered_map<std::string, SketchLine>& lines,
        std::unordered_map<std::string, SketchCircle>& circles,
        std::unordered_map<std::string, SketchArc>& arcs,
        std::unordered_map<std::string, SketchEllipse>* ellipses = nullptr);

    void setConstraints(const std::unordered_map<std::string, SketchConstraint>& constraints);

    /// Run Newton-Raphson iteration.
    SolveResult solve();

    /// Add a temporary soft drag constraint (weighted) that pulls a point
    /// toward (tx, ty). Cleared after each solve().
    void setDragTarget(const std::string& pointId, double tx, double ty, double weight = 1000.0);
    void clearDragTarget();

    /// Query DOF information after setEntities / setConstraints.
    int totalDOF() const;          // total mutable parameters
    int constrainedDOF() const;    // sum of dofRemoved across constraints
    int freeDOF() const;           // totalDOF - constrainedDOF

private:
    // ---------- data references (non-owning) ----------
    std::unordered_map<std::string, SketchPoint>*  m_points  = nullptr;
    std::unordered_map<std::string, SketchLine>*   m_lines   = nullptr;
    std::unordered_map<std::string, SketchCircle>* m_circles = nullptr;
    std::unordered_map<std::string, SketchArc>*    m_arcs    = nullptr;
    std::unordered_map<std::string, SketchEllipse>* m_ellipses = nullptr;
    const std::unordered_map<std::string, SketchConstraint>* m_constraints = nullptr;

    // ---------- solver parameters ----------
    static constexpr double kConvergenceTol = 1e-10;
    static constexpr double kStagnationTol  = 1e-12;
    static constexpr int    kMaxIterations  = 100;
    static constexpr double kMaxStepSize    = 50.0;   // mm — max parameter change per iteration
    static constexpr double kLambdaInit     = 1e-3;   // initial LM damping factor
    static constexpr double kLambdaScale    = 10.0;   // LM damping scale factor
    static constexpr int    kMaxLMRetries   = 5;      // max retries per LM iteration
    static constexpr int    kMaxRestarts    = 3;      // perturbed restart attempts

    // ---------- drag ----------
    bool        m_hasDrag = false;
    std::string m_dragPointId;
    double      m_dragTx = 0.0, m_dragTy = 0.0;
    double      m_dragWeight = 1000.0;

    // ---------- parameter layout ----------
    int m_numParams = 0;     // size of state vector X
    int m_numEquations = 0;  // size of residual vector F

    void buildParamLayout();
    int  countEquations() const;

    // ---------- state / residual / Jacobian ----------
    void packState(std::vector<double>& X) const;
    void unpackState(const std::vector<double>& X);

    void buildResidual(const std::vector<double>& X, std::vector<double>& F) const;
    void buildJacobian(const std::vector<double>& X, std::vector<std::vector<double>>& J) const;

    // ---------- per-constraint equation builders ----------
    // Each appends its residual rows to F starting at eqIdx, and fills
    // corresponding rows of J.  Returns number of equations emitted.
    int emitCoincident       (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitPointOnLine      (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitPointOnCircle    (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitDistance          (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitDistancePointLine(const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitHorizontal       (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitVertical         (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitParallel         (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitPerpendicular    (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitTangent          (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitEqual            (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitSymmetric        (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitMidpoint         (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitConcentric       (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitFixedAngle       (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitAngleBetween     (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitRadius           (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;
    int emitFix              (const SketchConstraint& c, const std::vector<double>& X, std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const;

    // ---------- helpers ----------
    size_t ptOff(const std::string& id) const;   // paramOffset of a SketchPoint
    size_t circRadOff(const std::string& id) const;
    size_t arcRadOff(const std::string& id) const;

    /// Pre-solve validation: skip degenerate entities / duplicate constraints.
    /// Returns diagnostic messages.
    std::string validateGeometry() const;

    /// Check for over-constrained (redundant) constraints via J^T*J diagonal.
    /// Returns true if redundancy detected; fills `message` with details.
    bool detectRedundancy(const std::vector<std::vector<double>>& J,
                          int m, int n, std::string& message) const;

    /// Core LM solve loop (single attempt from current X).
    /// Returns true if converged.
    bool solveLM(std::vector<double>& X, SolveResult& result);

    // Gaussian elimination solver: solves A * x = b  (n x n).
    // Returns false if singular.
    static bool solveLinear(std::vector<std::vector<double>>& A,
                            std::vector<double>& b, int n);
};

} // namespace sketch
