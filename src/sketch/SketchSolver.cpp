#include "SketchSolver.h"
#include <algorithm>
#include <cmath>
#include <cassert>
#include <numeric>
#include <random>
#include <sstream>
#include <set>

namespace sketch {

// ============================================================================
//  Public interface
// ============================================================================

void SketchSolver::setEntities(
    std::unordered_map<std::string, SketchPoint>& points,
    std::unordered_map<std::string, SketchLine>& lines,
    std::unordered_map<std::string, SketchCircle>& circles,
    std::unordered_map<std::string, SketchArc>& arcs,
    std::unordered_map<std::string, SketchEllipse>* ellipses)
{
    m_points  = &points;
    m_lines   = &lines;
    m_circles = &circles;
    m_arcs    = &arcs;
    m_ellipses = ellipses;
    buildParamLayout();
}

void SketchSolver::setConstraints(
    const std::unordered_map<std::string, SketchConstraint>& constraints)
{
    m_constraints = &constraints;
    m_numEquations = countEquations();
}

void SketchSolver::setDragTarget(const std::string& pointId, double tx, double ty, double weight)
{
    m_hasDrag = true;
    m_dragPointId = pointId;
    m_dragTx = tx;
    m_dragTy = ty;
    m_dragWeight = weight;
}

void SketchSolver::clearDragTarget()
{
    m_hasDrag = false;
}

int SketchSolver::totalDOF() const { return m_numParams; }

int SketchSolver::constrainedDOF() const
{
    if (!m_constraints) return 0;
    int sum = 0;
    for (auto& [id, c] : *m_constraints)
        sum += c.dofRemoved;
    return sum;
}

int SketchSolver::freeDOF() const { return totalDOF() - constrainedDOF(); }

// ============================================================================
//  Parameter layout
// ============================================================================

void SketchSolver::buildParamLayout()
{
    size_t off = 0;
    for (auto& [id, pt] : *m_points) {
        if (pt.isFixed) continue;
        pt.paramOffset = off;
        off += 2;  // x, y
    }
    for (auto& [id, circ] : *m_circles) {
        circ.radiusParamOffset = off;
        off += 1;
    }
    for (auto& [id, arc] : *m_arcs) {
        arc.radiusParamOffset = off;
        off += 1;
    }
    if (m_ellipses) {
        for (auto& [id, ell] : *m_ellipses) {
            ell.majorRadiusParamOffset = off;
            off += 1;
            ell.minorRadiusParamOffset = off;
            off += 1;
        }
    }
    m_numParams = static_cast<int>(off);
}

int SketchSolver::countEquations() const
{
    if (!m_constraints) return 0;
    int n = 0;
    for (auto& [id, c] : *m_constraints)
        n += c.dofRemoved;
    // drag adds 2 equations
    if (m_hasDrag) n += 2;
    return n;
}

// ============================================================================
//  State pack / unpack
// ============================================================================

void SketchSolver::packState(std::vector<double>& X) const
{
    X.resize(m_numParams, 0.0);
    for (auto& [id, pt] : *m_points) {
        if (pt.isFixed) continue;
        X[pt.paramOffset]     = pt.x;
        X[pt.paramOffset + 1] = pt.y;
    }
    for (auto& [id, circ] : *m_circles) {
        X[circ.radiusParamOffset] = circ.radius;
    }
    for (auto& [id, arc] : *m_arcs) {
        X[arc.radiusParamOffset] = arc.radius;
    }
    if (m_ellipses) {
        for (auto& [id, ell] : *m_ellipses) {
            X[ell.majorRadiusParamOffset] = ell.majorRadius;
            X[ell.minorRadiusParamOffset] = ell.minorRadius;
        }
    }
}

void SketchSolver::unpackState(const std::vector<double>& X)
{
    for (auto& [id, pt] : *m_points) {
        if (pt.isFixed) continue;
        pt.x = X[pt.paramOffset];
        pt.y = X[pt.paramOffset + 1];
    }
    for (auto& [id, circ] : *m_circles) {
        circ.radius = X[circ.radiusParamOffset];
    }
    for (auto& [id, arc] : *m_arcs) {
        arc.radius = X[arc.radiusParamOffset];
    }
    if (m_ellipses) {
        for (auto& [id, ell] : *m_ellipses) {
            ell.majorRadius = X[ell.majorRadiusParamOffset];
            ell.minorRadius = X[ell.minorRadiusParamOffset];
        }
    }
}

// ============================================================================
//  Helpers
// ============================================================================

size_t SketchSolver::ptOff(const std::string& id) const
{
    return m_points->at(id).paramOffset;
}

size_t SketchSolver::circRadOff(const std::string& id) const
{
    return m_circles->at(id).radiusParamOffset;
}

size_t SketchSolver::arcRadOff(const std::string& id) const
{
    return m_arcs->at(id).radiusParamOffset;
}

// Helper: get point coords from X (handles fixed points whose params are not in X)
static void getPointXY(const std::unordered_map<std::string, SketchPoint>& pts,
                       const std::vector<double>& X,
                       const std::string& id, double& px, double& py)
{
    auto& pt = pts.at(id);
    if (pt.isFixed) {
        px = pt.x;
        py = pt.y;
    } else {
        px = X[pt.paramOffset];
        py = X[pt.paramOffset + 1];
    }
}

// ============================================================================
//  Gaussian elimination  (partial pivoting)
// ============================================================================

bool SketchSolver::solveLinear(std::vector<std::vector<double>>& A,
                               std::vector<double>& b, int n)
{
    // Forward elimination with partial pivoting
    for (int col = 0; col < n; ++col) {
        // Find pivot
        int pivotRow = col;
        double pivotVal = std::abs(A[col][col]);
        for (int row = col + 1; row < n; ++row) {
            double v = std::abs(A[row][col]);
            if (v > pivotVal) {
                pivotVal = v;
                pivotRow = row;
            }
        }
        if (pivotVal < 1e-14) return false;  // singular

        if (pivotRow != col) {
            std::swap(A[col], A[pivotRow]);
            std::swap(b[col], b[pivotRow]);
        }

        double diagInv = 1.0 / A[col][col];
        for (int row = col + 1; row < n; ++row) {
            double factor = A[row][col] * diagInv;
            for (int j = col + 1; j < n; ++j)
                A[row][j] -= factor * A[col][j];
            A[row][col] = 0.0;
            b[row] -= factor * b[col];
        }
    }

    // Back substitution
    for (int row = n - 1; row >= 0; --row) {
        double sum = b[row];
        for (int j = row + 1; j < n; ++j)
            sum -= A[row][j] * b[j];
        if (std::abs(A[row][row]) < 1e-14) return false;
        b[row] = sum / A[row][row];
    }
    return true;
}

// ============================================================================
//  Main solve loop
// ============================================================================

// ============================================================================
//  Degenerate geometry validation
// ============================================================================

std::string SketchSolver::validateGeometry() const
{
    std::ostringstream diag;

    // Check zero-length lines (start == end)
    if (m_lines) {
        for (const auto& [id, ln] : *m_lines) {
            auto itA = m_points->find(ln.startPointId);
            auto itB = m_points->find(ln.endPointId);
            if (itA != m_points->end() && itB != m_points->end()) {
                double dx = itA->second.x - itB->second.x;
                double dy = itA->second.y - itB->second.y;
                if (dx * dx + dy * dy < 1e-20) {
                    diag << "Zero-length line " << id << "; ";
                }
            }
        }
    }

    // Check zero-radius circles
    if (m_circles) {
        for (const auto& [id, circ] : *m_circles) {
            if (circ.radius < 1e-12) {
                diag << "Zero-radius circle " << id << "; ";
            }
        }
    }

    // Check zero-radius arcs
    if (m_arcs) {
        for (const auto& [id, arc] : *m_arcs) {
            if (arc.radius < 1e-12) {
                diag << "Zero-radius arc " << id << "; ";
            }
        }
    }

    // Check duplicate coincident constraints
    if (m_constraints) {
        std::set<std::pair<std::string, std::string>> coincidentPairs;
        for (const auto& [id, c] : *m_constraints) {
            if (c.type == ConstraintType::Coincident && c.entityIds.size() == 2) {
                auto key = std::minmax(c.entityIds[0], c.entityIds[1]);
                if (!coincidentPairs.insert(key).second) {
                    diag << "Duplicate coincident constraint " << id << "; ";
                }
            }
        }
    }

    return diag.str();
}

// ============================================================================
//  Redundancy detection via J^T*J diagonal analysis
// ============================================================================

bool SketchSolver::detectRedundancy(const std::vector<std::vector<double>>& J,
                                     int m, int n, std::string& message) const
{
    if (m <= n) return false;  // not over-determined by equation count

    // Build J^T * J and check diagonal
    std::vector<double> diagJtJ(n, 0.0);
    for (int i = 0; i < n; ++i)
        for (int k = 0; k < m; ++k)
            diagJtJ[i] += J[k][i] * J[k][i];

    bool hasRedundancy = false;
    std::ostringstream oss;
    for (int i = 0; i < n; ++i) {
        if (diagJtJ[i] < 1e-12) {
            hasRedundancy = true;
            oss << "Parameter " << i << " has degenerate constraint direction; ";
        }
    }

    // Also check if there are more equations than parameters
    if (m > n) {
        hasRedundancy = true;
        oss << "Over-constrained: " << m << " equations for " << n << " parameters; ";
    }

    message = oss.str();
    return hasRedundancy;
}

// ============================================================================
//  Levenberg-Marquardt core solve loop
// ============================================================================

bool SketchSolver::solveLM(std::vector<double>& X, SolveResult& result)
{
    int n = m_numParams;
    int m = m_numEquations;
    double lambda = kLambdaInit;
    double prevResidualSq = 1e30;

    for (int iter = 0; iter < kMaxIterations; ++iter) {
        result.iterations = iter + 1;

        // Compute residual
        std::vector<double> F(m, 0.0);
        buildResidual(X, F);

        double residualSq = 0.0;
        for (double fi : F) residualSq += fi * fi;
        double norm = std::sqrt(residualSq);
        result.residual = norm;

        // Check convergence
        if (norm < kConvergenceTol) {
            return true;
        }

        // Check stagnation
        if (iter > 0 && std::abs(prevResidualSq - residualSq) < kStagnationTol) {
            // Converged to a non-zero residual — check if it's acceptably small
            return (norm < kConvergenceTol * 100.0);
        }
        prevResidualSq = residualSq;

        // Build Jacobian
        std::vector<std::vector<double>> J(m, std::vector<double>(n, 0.0));
        buildJacobian(X, J);

        // Compute J^T * J  (n x n) and J^T * F  (n x 1)
        std::vector<std::vector<double>> JtJ(n, std::vector<double>(n, 0.0));
        std::vector<double> JtF(n, 0.0);
        for (int i = 0; i < n; ++i) {
            for (int k = 0; k < m; ++k) {
                JtF[i] += J[k][i] * F[k];
                for (int j = i; j < n; ++j)
                    JtJ[i][j] += J[k][i] * J[k][j];
            }
            // Mirror symmetric part
            for (int j = 0; j < i; ++j)
                JtJ[i][j] = JtJ[j][i];
        }

        // Levenberg-Marquardt with retries
        bool stepAccepted = false;
        for (int retry = 0; retry < kMaxLMRetries; ++retry) {
            // Copy JtJ and add damping: (J^T*J + lambda*I)
            std::vector<std::vector<double>> A = JtJ;
            for (int i = 0; i < n; ++i)
                A[i][i] += lambda;

            // RHS = -J^T * F
            std::vector<double> rhs(n);
            for (int i = 0; i < n; ++i)
                rhs[i] = -JtF[i];

            if (!solveLinear(A, rhs, n)) {
                lambda *= kLambdaScale;
                continue;
            }

            // rhs now contains the step dX — apply step size limiting
            double stepNorm = 0.0;
            for (int i = 0; i < n; ++i) stepNorm += rhs[i] * rhs[i];
            stepNorm = std::sqrt(stepNorm);
            if (stepNorm > kMaxStepSize) {
                double scale = kMaxStepSize / stepNorm;
                for (int i = 0; i < n; ++i) rhs[i] *= scale;
            }

            // Trial step
            std::vector<double> Xtrial(n);
            for (int i = 0; i < n; ++i)
                Xtrial[i] = X[i] + rhs[i];

            // Evaluate trial residual
            std::vector<double> Ftrial(m, 0.0);
            buildResidual(Xtrial, Ftrial);
            double trialResidualSq = 0.0;
            for (double fi : Ftrial) trialResidualSq += fi * fi;

            if (trialResidualSq < residualSq) {
                // Accept step, decrease damping
                X = Xtrial;
                lambda = std::max(lambda / kLambdaScale, 1e-12);
                stepAccepted = true;
                break;
            } else {
                // Reject step, increase damping
                lambda *= kLambdaScale;
            }
        }

        if (!stepAccepted) {
            // Could not find a decreasing step — give up this attempt
            return false;
        }
    }

    return false;  // did not converge within max iterations
}

// ============================================================================
//  Main solve entry point
// ============================================================================

SolveResult SketchSolver::solve()
{
    SolveResult result;
    if (!m_points || !m_constraints) {
        result.status = SolveStatus::Failed;
        return result;
    }

    // Recalculate equation count (drag may have changed)
    m_numEquations = countEquations();

    if (m_numParams == 0) {
        result.status = SolveStatus::Solved;
        result.residual = 0.0;
        clearDragTarget();
        return result;
    }

    // Validate geometry
    std::string diagMsg = validateGeometry();

    // Redundancy detection
    if (m_numEquations > m_numParams) {
        std::vector<double> Xcheck;
        packState(Xcheck);
        std::vector<std::vector<double>> Jcheck(m_numEquations, std::vector<double>(m_numParams, 0.0));
        buildJacobian(Xcheck, Jcheck);

        std::string redundancyMsg;
        if (detectRedundancy(Jcheck, m_numEquations, m_numParams, redundancyMsg)) {
            // Still attempt to solve via LM (least-squares), but flag it
            diagMsg += redundancyMsg;
        }
    }

    // Primary solve attempt
    std::vector<double> X;
    packState(X);
    std::vector<double> Xbest = X;

    bool converged = solveLM(X, result);

    if (converged) {
        unpackState(X);
        result.status = SolveStatus::Solved;
        result.message = diagMsg;
        clearDragTarget();
        return result;
    }

    // Save best result so far
    double bestResidual = result.residual;
    Xbest = X;

    // Retry from perturbed starting points (only for non-drag solves)
    if (!m_hasDrag) {
        std::mt19937 rng(42);  // deterministic seed for reproducibility
        std::uniform_real_distribution<double> dist(-1.0, 1.0);

        for (int restart = 0; restart < kMaxRestarts; ++restart) {
            // Reset to original positions with small perturbation
            std::vector<double> Xpert;
            packState(Xpert);
            for (int i = 0; i < m_numParams; ++i) {
                // Only perturb unfixed parameters
                Xpert[i] += dist(rng) * 0.5;  // small noise: +/- 0.5mm
            }

            SolveResult retryResult;
            if (solveLM(Xpert, retryResult)) {
                // Converged from perturbed start
                X = Xpert;
                result = retryResult;
                result.message = diagMsg;
                unpackState(X);
                result.status = SolveStatus::Solved;
                clearDragTarget();
                return result;
            }

            if (retryResult.residual < bestResidual) {
                bestResidual = retryResult.residual;
                Xbest = Xpert;
                result.iterations = retryResult.iterations;
            }
        }
    }

    // Did not converge — use best result found
    unpackState(Xbest);
    result.residual = bestResidual;
    result.message = diagMsg;
    result.status = (m_numEquations > m_numParams)
                        ? SolveStatus::OverConstrained
                        : SolveStatus::Failed;
    clearDragTarget();
    return result;
}

// ============================================================================
//  Residual & Jacobian builders  (dispatch to per-constraint emitters)
// ============================================================================

void SketchSolver::buildResidual(const std::vector<double>& X, std::vector<double>& F) const
{
    int eqIdx = 0;
    for (auto& [id, c] : *m_constraints) {
        int emitted = 0;
        switch (c.type) {
            case ConstraintType::Coincident:        emitted = emitCoincident(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::PointOnLine:       emitted = emitPointOnLine(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::PointOnCircle:     emitted = emitPointOnCircle(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::Distance:          emitted = emitDistance(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::DistancePointLine: emitted = emitDistancePointLine(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::Horizontal:        emitted = emitHorizontal(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::Vertical:          emitted = emitVertical(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::Parallel:          emitted = emitParallel(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::Perpendicular:     emitted = emitPerpendicular(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::Tangent:           emitted = emitTangent(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::Equal:             emitted = emitEqual(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::Symmetric:         emitted = emitSymmetric(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::Midpoint:          emitted = emitMidpoint(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::Concentric:        emitted = emitConcentric(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::FixedAngle:        emitted = emitFixedAngle(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::AngleBetween:      emitted = emitAngleBetween(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::Radius:            emitted = emitRadius(c, X, F, nullptr, eqIdx); break;
            case ConstraintType::Fix:               emitted = emitFix(c, X, F, nullptr, eqIdx); break;
        }
        eqIdx += emitted;
    }

    // Drag constraint (soft, weighted)
    if (m_hasDrag) {
        auto it = m_points->find(m_dragPointId);
        if (it != m_points->end() && !it->second.isFixed) {
            double px, py;
            getPointXY(*m_points, X, m_dragPointId, px, py);
            F[eqIdx]     = m_dragWeight * (px - m_dragTx);
            F[eqIdx + 1] = m_dragWeight * (py - m_dragTy);
        }
    }
}

void SketchSolver::buildJacobian(const std::vector<double>& X,
                                  std::vector<std::vector<double>>& J) const
{
    int eqIdx = 0;
    for (auto& [id, c] : *m_constraints) {
        // We reuse the emit functions but pass J so they fill in derivatives too.
        // The residual values written to a dummy F are ignored here.
        std::vector<double> dummyF(m_numEquations, 0.0);
        int emitted = 0;
        switch (c.type) {
            case ConstraintType::Coincident:        emitted = emitCoincident(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::PointOnLine:       emitted = emitPointOnLine(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::PointOnCircle:     emitted = emitPointOnCircle(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::Distance:          emitted = emitDistance(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::DistancePointLine: emitted = emitDistancePointLine(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::Horizontal:        emitted = emitHorizontal(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::Vertical:          emitted = emitVertical(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::Parallel:          emitted = emitParallel(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::Perpendicular:     emitted = emitPerpendicular(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::Tangent:           emitted = emitTangent(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::Equal:             emitted = emitEqual(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::Symmetric:         emitted = emitSymmetric(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::Midpoint:          emitted = emitMidpoint(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::Concentric:        emitted = emitConcentric(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::FixedAngle:        emitted = emitFixedAngle(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::AngleBetween:      emitted = emitAngleBetween(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::Radius:            emitted = emitRadius(c, X, dummyF, &J, eqIdx); break;
            case ConstraintType::Fix:               emitted = emitFix(c, X, dummyF, &J, eqIdx); break;
        }
        eqIdx += emitted;
    }

    // Drag Jacobian
    if (m_hasDrag) {
        auto it = m_points->find(m_dragPointId);
        if (it != m_points->end() && !it->second.isFixed) {
            size_t off = it->second.paramOffset;
            J[eqIdx][off]         = m_dragWeight;
            J[eqIdx + 1][off + 1] = m_dragWeight;
        }
    }
}

// ============================================================================
//  Constraint equation emitters
// ============================================================================
// Convention: entityIds layout is documented per constraint type.
// Each emitter writes residual values into F[eqIdx..] and, if J != nullptr,
// writes analytical partial derivatives into (*J)[eqIdx..][paramCol].
// Returns the number of equations emitted.

// Helper macros for readability
#define PT_XY(name, id) \
    double name##x, name##y; \
    getPointXY(*m_points, X, id, name##x, name##y)

#define PT_FIXED(id) (m_points->at(id).isFixed)
#define PT_OFF(id)   (m_points->at(id).paramOffset)

// ---------- Coincident: entityIds = [pointId1, pointId2] ----------
// f1 = x1 - x2,  f2 = y1 - y2
int SketchSolver::emitCoincident(const SketchConstraint& c, const std::vector<double>& X,
                                  std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    const auto& p1id = c.entityIds[0];
    const auto& p2id = c.entityIds[1];
    PT_XY(p1, p1id); PT_XY(p2, p2id);

    F[eqIdx]     = p1x - p2x;
    F[eqIdx + 1] = p1y - p2y;

    if (J) {
        if (!PT_FIXED(p1id)) {
            (*J)[eqIdx][PT_OFF(p1id)]       =  1.0;
            (*J)[eqIdx + 1][PT_OFF(p1id)+1] =  1.0;
        }
        if (!PT_FIXED(p2id)) {
            (*J)[eqIdx][PT_OFF(p2id)]       = -1.0;
            (*J)[eqIdx + 1][PT_OFF(p2id)+1] = -1.0;
        }
    }
    return 2;
}

// ---------- PointOnLine: entityIds = [pointId, lineId] ----------
// cross product: (px-ax)(by-ay) - (py-ay)(bx-ax) = 0
int SketchSolver::emitPointOnLine(const SketchConstraint& c, const std::vector<double>& X,
                                   std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    const auto& pid = c.entityIds[0];
    const auto& lid = c.entityIds[1];
    auto& line = m_lines->at(lid);
    PT_XY(p, pid); PT_XY(a, line.startPointId); PT_XY(b, line.endPointId);

    // f = (px-ax)*(by-ay) - (py-ay)*(bx-ax)
    double dax = px - ax, day = py - ay;
    double dbx = bx - ax, dby = by - ay;
    F[eqIdx] = dax * dby - day * dbx;

    if (J) {
        // df/dpx = by-ay,  df/dpy = -(bx-ax)
        if (!PT_FIXED(pid)) {
            (*J)[eqIdx][PT_OFF(pid)]     =  dby;
            (*J)[eqIdx][PT_OFF(pid) + 1] = -dbx;
        }
        // df/dax = -(by-ay) + (py-ay) = py - by
        // df/day = (bx-ax) - (px-ax)  = bx - px   ... wait, let me redo carefully.
        // f = (px-ax)(by-ay) - (py-ay)(bx-ax)
        // df/dax = -(by-ay) - (-(bx-ax)) ... no. Let's just differentiate term by term.
        // f = px*by - px*ay - ax*by + ax*ay - py*bx + py*ax + ay*bx - ay*ax
        // f = px*by - px*ay - ax*by + ax*ay - py*bx + py*ax + ay*bx - ay*ax
        // Simplify: ax*ay - ay*ax = 0.  So:
        // f = px*by - px*ay - ax*by - py*bx + py*ax + ay*bx
        // df/dax = -by + py = py - by
        // df/day = -px + bx = bx - px
        // df/dbx = -py + ay = ay - py
        // df/dby = px - ax
        if (!PT_FIXED(line.startPointId)) {
            (*J)[eqIdx][PT_OFF(line.startPointId)]     = py - by;   // df/dax
            (*J)[eqIdx][PT_OFF(line.startPointId) + 1] = bx - px;   // df/day
        }
        if (!PT_FIXED(line.endPointId)) {
            (*J)[eqIdx][PT_OFF(line.endPointId)]     = ay - py;     // df/dbx
            (*J)[eqIdx][PT_OFF(line.endPointId) + 1] = px - ax;     // df/dby
        }
    }
    return 1;
}

// ---------- PointOnCircle: entityIds = [pointId, circleId] ----------
// (px-cx)^2 + (py-cy)^2 - r^2 = 0
int SketchSolver::emitPointOnCircle(const SketchConstraint& c, const std::vector<double>& X,
                                     std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    const auto& pid = c.entityIds[0];
    const auto& cid = c.entityIds[1];
    auto& circ = m_circles->at(cid);
    PT_XY(p, pid); PT_XY(ctr, circ.centerPointId);
    double r = X[circ.radiusParamOffset];

    double dx = px - ctrx, dy = py - ctry;
    F[eqIdx] = dx * dx + dy * dy - r * r;

    if (J) {
        if (!PT_FIXED(pid)) {
            (*J)[eqIdx][PT_OFF(pid)]     = 2.0 * dx;
            (*J)[eqIdx][PT_OFF(pid) + 1] = 2.0 * dy;
        }
        if (!PT_FIXED(circ.centerPointId)) {
            (*J)[eqIdx][PT_OFF(circ.centerPointId)]     = -2.0 * dx;
            (*J)[eqIdx][PT_OFF(circ.centerPointId) + 1] = -2.0 * dy;
        }
        (*J)[eqIdx][circ.radiusParamOffset] = -2.0 * r;
    }
    return 1;
}

// ---------- Distance: entityIds = [pointId1, pointId2], value = d ----------
// (x2-x1)^2 + (y2-y1)^2 - d^2 = 0
int SketchSolver::emitDistance(const SketchConstraint& c, const std::vector<double>& X,
                               std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    const auto& p1id = c.entityIds[0];
    const auto& p2id = c.entityIds[1];
    PT_XY(p1, p1id); PT_XY(p2, p2id);
    double d = c.value;

    double dx = p2x - p1x, dy = p2y - p1y;
    F[eqIdx] = dx * dx + dy * dy - d * d;

    if (J) {
        if (!PT_FIXED(p1id)) {
            (*J)[eqIdx][PT_OFF(p1id)]     = -2.0 * dx;
            (*J)[eqIdx][PT_OFF(p1id) + 1] = -2.0 * dy;
        }
        if (!PT_FIXED(p2id)) {
            (*J)[eqIdx][PT_OFF(p2id)]     =  2.0 * dx;
            (*J)[eqIdx][PT_OFF(p2id) + 1] =  2.0 * dy;
        }
    }
    return 1;
}

// ---------- DistancePointLine: entityIds = [pointId, lineId], value = d ----------
// signed distance from point to line = d
// line from A to B.  dist = ((px-ax)(by-ay) - (py-ay)(bx-ax)) / len(AB)
// We use:  cross - d * len = 0  where cross = (px-ax)(by-ay)-(py-ay)(bx-ax), len = sqrt((bx-ax)^2+(by-ay)^2)
// But squaring avoids the sign issue:  cross^2 - d^2 * lenSq = 0
int SketchSolver::emitDistancePointLine(const SketchConstraint& c, const std::vector<double>& X,
                                         std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    const auto& pid = c.entityIds[0];
    const auto& lid = c.entityIds[1];
    auto& line = m_lines->at(lid);
    PT_XY(p, pid); PT_XY(a, line.startPointId); PT_XY(b, line.endPointId);
    double d = c.value;

    double dax = px - ax, day = py - ay;
    double dbx = bx - ax, dby = by - ay;
    double cross = dax * dby - day * dbx;
    double lenSq = dbx * dbx + dby * dby;
    F[eqIdx] = cross * cross - d * d * lenSq;

    if (J) {
        // Let C = cross, L = lenSq.  f = C^2 - d^2*L
        // df/d(var) = 2*C * dC/d(var) - d^2 * dL/d(var)
        // dC/dpx = dby,  dC/dpy = -dbx
        // dC/dax = py-by, dC/day = bx-px, dC/dbx = ay-py, dC/dby = px-ax
        // dL/dax = -2*dbx, dL/day = -2*dby, dL/dbx = 2*dbx, dL/dby = 2*dby
        double d2 = d * d;
        if (!PT_FIXED(pid)) {
            (*J)[eqIdx][PT_OFF(pid)]     = 2.0 * cross * dby;
            (*J)[eqIdx][PT_OFF(pid) + 1] = 2.0 * cross * (-dbx);
        }
        if (!PT_FIXED(line.startPointId)) {
            (*J)[eqIdx][PT_OFF(line.startPointId)]     = 2.0*cross*(py-by) - d2*(-2.0*dbx);
            (*J)[eqIdx][PT_OFF(line.startPointId) + 1] = 2.0*cross*(bx-px) - d2*(-2.0*dby);
        }
        if (!PT_FIXED(line.endPointId)) {
            (*J)[eqIdx][PT_OFF(line.endPointId)]     = 2.0*cross*(ay-py) - d2*(2.0*dbx);
            (*J)[eqIdx][PT_OFF(line.endPointId) + 1] = 2.0*cross*(px-ax) - d2*(2.0*dby);
        }
    }
    return 1;
}

// ---------- Horizontal: entityIds = [lineId] ----------
// y_end - y_start = 0
int SketchSolver::emitHorizontal(const SketchConstraint& c, const std::vector<double>& X,
                                  std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    auto& line = m_lines->at(c.entityIds[0]);
    PT_XY(a, line.startPointId); PT_XY(b, line.endPointId);

    F[eqIdx] = by - ay;

    if (J) {
        if (!PT_FIXED(line.startPointId))
            (*J)[eqIdx][PT_OFF(line.startPointId) + 1] = -1.0;
        if (!PT_FIXED(line.endPointId))
            (*J)[eqIdx][PT_OFF(line.endPointId) + 1]   =  1.0;
    }
    return 1;
}

// ---------- Vertical: entityIds = [lineId] ----------
// x_end - x_start = 0
int SketchSolver::emitVertical(const SketchConstraint& c, const std::vector<double>& X,
                                std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    auto& line = m_lines->at(c.entityIds[0]);
    PT_XY(a, line.startPointId); PT_XY(b, line.endPointId);

    F[eqIdx] = bx - ax;

    if (J) {
        if (!PT_FIXED(line.startPointId))
            (*J)[eqIdx][PT_OFF(line.startPointId)]     = -1.0;
        if (!PT_FIXED(line.endPointId))
            (*J)[eqIdx][PT_OFF(line.endPointId)]       =  1.0;
    }
    return 1;
}

// ---------- Parallel: entityIds = [lineId1, lineId2] ----------
// cross product of directions = 0:  (b1x-a1x)(b2y-a2y) - (b1y-a1y)(b2x-a2x) = 0
int SketchSolver::emitParallel(const SketchConstraint& c, const std::vector<double>& X,
                                std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    auto& l1 = m_lines->at(c.entityIds[0]);
    auto& l2 = m_lines->at(c.entityIds[1]);
    PT_XY(a1, l1.startPointId); PT_XY(b1, l1.endPointId);
    PT_XY(a2, l2.startPointId); PT_XY(b2, l2.endPointId);

    double d1x = b1x - a1x, d1y = b1y - a1y;
    double d2x = b2x - a2x, d2y = b2y - a2y;
    F[eqIdx] = d1x * d2y - d1y * d2x;

    if (J) {
        // df/da1x = -d2y, df/da1y = d2x, df/db1x = d2y, df/db1y = -d2x
        // df/da2x = d1y,  df/da2y = -d1x, df/db2x = -d1y, df/db2y = d1x
        if (!PT_FIXED(l1.startPointId)) {
            (*J)[eqIdx][PT_OFF(l1.startPointId)]     = -d2y;
            (*J)[eqIdx][PT_OFF(l1.startPointId) + 1] =  d2x;
        }
        if (!PT_FIXED(l1.endPointId)) {
            (*J)[eqIdx][PT_OFF(l1.endPointId)]       =  d2y;
            (*J)[eqIdx][PT_OFF(l1.endPointId) + 1]   = -d2x;
        }
        if (!PT_FIXED(l2.startPointId)) {
            (*J)[eqIdx][PT_OFF(l2.startPointId)]     =  d1y;
            (*J)[eqIdx][PT_OFF(l2.startPointId) + 1] = -d1x;
        }
        if (!PT_FIXED(l2.endPointId)) {
            (*J)[eqIdx][PT_OFF(l2.endPointId)]       = -d1y;
            (*J)[eqIdx][PT_OFF(l2.endPointId) + 1]   =  d1x;
        }
    }
    return 1;
}

// ---------- Perpendicular: entityIds = [lineId1, lineId2] ----------
// dot product of directions = 0:  d1x*d2x + d1y*d2y = 0
int SketchSolver::emitPerpendicular(const SketchConstraint& c, const std::vector<double>& X,
                                     std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    auto& l1 = m_lines->at(c.entityIds[0]);
    auto& l2 = m_lines->at(c.entityIds[1]);
    PT_XY(a1, l1.startPointId); PT_XY(b1, l1.endPointId);
    PT_XY(a2, l2.startPointId); PT_XY(b2, l2.endPointId);

    double d1x = b1x - a1x, d1y = b1y - a1y;
    double d2x = b2x - a2x, d2y = b2y - a2y;
    F[eqIdx] = d1x * d2x + d1y * d2y;

    if (J) {
        // df/da1x = -d2x, df/da1y = -d2y, df/db1x = d2x, df/db1y = d2y
        // df/da2x = -d1x, df/da2y = -d1y, df/db2x = d1x, df/db2y = d1y
        if (!PT_FIXED(l1.startPointId)) {
            (*J)[eqIdx][PT_OFF(l1.startPointId)]     = -d2x;
            (*J)[eqIdx][PT_OFF(l1.startPointId) + 1] = -d2y;
        }
        if (!PT_FIXED(l1.endPointId)) {
            (*J)[eqIdx][PT_OFF(l1.endPointId)]       =  d2x;
            (*J)[eqIdx][PT_OFF(l1.endPointId) + 1]   =  d2y;
        }
        if (!PT_FIXED(l2.startPointId)) {
            (*J)[eqIdx][PT_OFF(l2.startPointId)]     = -d1x;
            (*J)[eqIdx][PT_OFF(l2.startPointId) + 1] = -d1y;
        }
        if (!PT_FIXED(l2.endPointId)) {
            (*J)[eqIdx][PT_OFF(l2.endPointId)]       =  d1x;
            (*J)[eqIdx][PT_OFF(l2.endPointId) + 1]   =  d1y;
        }
    }
    return 1;
}

// ---------- Tangent (line-circle): entityIds = [lineId, circleId] ----------
// Distance from circle center to line = radius.
// Squared form: cross^2 - r^2 * lenSq = 0
//   where cross = (cx-ax)(by-ay) - (cy-ay)(bx-ax), lenSq = (bx-ax)^2 + (by-ay)^2
int SketchSolver::emitTangent(const SketchConstraint& c, const std::vector<double>& X,
                               std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    const auto& lid = c.entityIds[0];
    const auto& cid = c.entityIds[1];
    auto& line = m_lines->at(lid);
    auto& circ = m_circles->at(cid);
    PT_XY(a, line.startPointId); PT_XY(b, line.endPointId);
    PT_XY(ctr, circ.centerPointId);
    double r = X[circ.radiusParamOffset];

    double dax = ctrx - ax, day = ctry - ay;
    double dbx = bx - ax, dby = by - ay;
    double cross = dax * dby - day * dbx;
    double lenSq = dbx * dbx + dby * dby;
    F[eqIdx] = cross * cross - r * r * lenSq;

    if (J) {
        double r2 = r * r;
        // dC/dctrx = dby, dC/dctry = -dbx
        if (!PT_FIXED(circ.centerPointId)) {
            (*J)[eqIdx][PT_OFF(circ.centerPointId)]     =  2.0 * cross * dby;
            (*J)[eqIdx][PT_OFF(circ.centerPointId) + 1] =  2.0 * cross * (-dbx);
        }
        // dC/dax = -(dby) - (-(dbx)) ... careful:
        // C = (ctrx-ax)*dby - (ctry-ay)*dbx
        //   = ctrx*dby - ax*dby - ctry*dbx + ay*dbx
        // But dby = by-ay, dbx = bx-ax, so these also depend on a and b.
        // Expand fully:
        // C = (ctrx-ax)(by-ay) - (ctry-ay)(bx-ax)
        //   = ctrx*by - ctrx*ay - ax*by + ax*ay - ctry*bx + ctry*ax + ay*bx - ay*ax
        //   = ctrx*by - ctrx*ay - ax*by - ctry*bx + ctry*ax + ay*bx
        // dC/dax = -by + ctry = ctry - by
        // dC/day = -ctrx + bx = bx - ctrx
        // dC/dbx = -ctry + ay = ay - ctry
        // dC/dby = ctrx - ax
        // dL/dax = -2*dbx, dL/day = -2*dby, dL/dbx = 2*dbx, dL/dby = 2*dby
        if (!PT_FIXED(line.startPointId)) {
            (*J)[eqIdx][PT_OFF(line.startPointId)]     = 2.0*cross*(ctry - by) - r2*(-2.0*dbx);
            (*J)[eqIdx][PT_OFF(line.startPointId) + 1] = 2.0*cross*(bx - ctrx) - r2*(-2.0*dby);
        }
        if (!PT_FIXED(line.endPointId)) {
            (*J)[eqIdx][PT_OFF(line.endPointId)]       = 2.0*cross*(ay - ctry) - r2*(2.0*dbx);
            (*J)[eqIdx][PT_OFF(line.endPointId) + 1]   = 2.0*cross*(ctrx - ax) - r2*(2.0*dby);
        }
        // df/dr = -2*r*lenSq
        (*J)[eqIdx][circ.radiusParamOffset] = -2.0 * r * lenSq;
    }
    return 1;
}

// ---------- Equal: entityIds = [lineId1, lineId2] (or [circleId1, circleId2]) ----------
// For lines: len1^2 - len2^2 = 0
// For circles: r1 - r2 = 0
int SketchSolver::emitEqual(const SketchConstraint& c, const std::vector<double>& X,
                             std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    // Try lines first
    auto it1 = m_lines->find(c.entityIds[0]);
    auto it2 = m_lines->find(c.entityIds[1]);
    if (it1 != m_lines->end() && it2 != m_lines->end()) {
        // Equal length lines
        auto& l1 = it1->second;
        auto& l2 = it2->second;
        PT_XY(a1, l1.startPointId); PT_XY(b1, l1.endPointId);
        PT_XY(a2, l2.startPointId); PT_XY(b2, l2.endPointId);

        double d1x = b1x - a1x, d1y = b1y - a1y;
        double d2x = b2x - a2x, d2y = b2y - a2y;
        F[eqIdx] = (d1x*d1x + d1y*d1y) - (d2x*d2x + d2y*d2y);

        if (J) {
            if (!PT_FIXED(l1.startPointId)) {
                (*J)[eqIdx][PT_OFF(l1.startPointId)]     = -2.0*d1x;
                (*J)[eqIdx][PT_OFF(l1.startPointId) + 1] = -2.0*d1y;
            }
            if (!PT_FIXED(l1.endPointId)) {
                (*J)[eqIdx][PT_OFF(l1.endPointId)]       =  2.0*d1x;
                (*J)[eqIdx][PT_OFF(l1.endPointId) + 1]   =  2.0*d1y;
            }
            if (!PT_FIXED(l2.startPointId)) {
                (*J)[eqIdx][PT_OFF(l2.startPointId)]     =  2.0*d2x;
                (*J)[eqIdx][PT_OFF(l2.startPointId) + 1] =  2.0*d2y;
            }
            if (!PT_FIXED(l2.endPointId)) {
                (*J)[eqIdx][PT_OFF(l2.endPointId)]       = -2.0*d2x;
                (*J)[eqIdx][PT_OFF(l2.endPointId) + 1]   = -2.0*d2y;
            }
        }
        return 1;
    }

    // Try circles
    auto c1it = m_circles->find(c.entityIds[0]);
    auto c2it = m_circles->find(c.entityIds[1]);
    if (c1it != m_circles->end() && c2it != m_circles->end()) {
        double r1 = X[c1it->second.radiusParamOffset];
        double r2 = X[c2it->second.radiusParamOffset];
        F[eqIdx] = r1 - r2;
        if (J) {
            (*J)[eqIdx][c1it->second.radiusParamOffset] =  1.0;
            (*J)[eqIdx][c2it->second.radiusParamOffset] = -1.0;
        }
        return 1;
    }

    // Fallback: no match, emit zero residual
    F[eqIdx] = 0.0;
    return 1;
}

// ---------- Symmetric: entityIds = [pointId1, pointId2, lineId] ----------
// P1 and P2 are symmetric about the line A-B.
// Eq 1: midpoint of P1P2 lies on line  (cross product = 0)
// Eq 2: P1P2 is perpendicular to line  (dot product = 0)
int SketchSolver::emitSymmetric(const SketchConstraint& c, const std::vector<double>& X,
                                 std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    const auto& p1id = c.entityIds[0];
    const auto& p2id = c.entityIds[1];
    auto& line = m_lines->at(c.entityIds[2]);
    PT_XY(p1, p1id); PT_XY(p2, p2id);
    PT_XY(a, line.startPointId); PT_XY(b, line.endPointId);

    double mx = (p1x + p2x) * 0.5, my = (p1y + p2y) * 0.5;
    double lx = bx - ax, ly = by - ay;
    double qx = p2x - p1x, qy = p2y - p1y;

    // Eq 1: midpoint on line: (mx-ax)*ly - (my-ay)*lx = 0
    F[eqIdx] = (mx - ax) * ly - (my - ay) * lx;
    // Eq 2: P1P2 perp to line: qx*lx + qy*ly = 0
    F[eqIdx + 1] = qx * lx + qy * ly;

    if (J) {
        // Eq1: f = (mx-ax)*ly - (my-ay)*lx
        // df/dp1x = 0.5*ly, df/dp1y = -0.5*lx
        // df/dp2x = 0.5*ly, df/dp2y = -0.5*lx
        // df/dax = -ly + lx*0 ... wait:
        // f = mx*ly - ax*ly - my*lx + ay*lx
        // but ly = by-ay, lx = bx-ax depend on a,b
        // Expand: f = mx*(by-ay) - ax*(by-ay) - my*(bx-ax) + ay*(bx-ax)
        // = mx*by - mx*ay - ax*by + ax*ay - my*bx + my*ax + ay*bx - ay*ax
        // = mx*by - mx*ay - ax*by - my*bx + my*ax + ay*bx
        // df/dax = -by + my = my - by
        // df/day = -mx + bx = bx - mx
        // df/dbx = -my + ay = ay - my
        // df/dby = mx - ax
        if (!PT_FIXED(p1id)) {
            (*J)[eqIdx][PT_OFF(p1id)]     =  0.5 * ly;
            (*J)[eqIdx][PT_OFF(p1id) + 1] = -0.5 * lx;
        }
        if (!PT_FIXED(p2id)) {
            (*J)[eqIdx][PT_OFF(p2id)]     =  0.5 * ly;
            (*J)[eqIdx][PT_OFF(p2id) + 1] = -0.5 * lx;
        }
        if (!PT_FIXED(line.startPointId)) {
            (*J)[eqIdx][PT_OFF(line.startPointId)]     = my - by;
            (*J)[eqIdx][PT_OFF(line.startPointId) + 1] = bx - mx;
        }
        if (!PT_FIXED(line.endPointId)) {
            (*J)[eqIdx][PT_OFF(line.endPointId)]       = ay - my;
            (*J)[eqIdx][PT_OFF(line.endPointId) + 1]   = mx - ax;
        }

        // Eq2: g = qx*lx + qy*ly = (p2x-p1x)(bx-ax) + (p2y-p1y)(by-ay)
        // dg/dp1x = -lx = -(bx-ax),  dg/dp1y = -ly = -(by-ay)
        // dg/dp2x = lx,  dg/dp2y = ly
        // dg/dax = -qx,  dg/day = -qy
        // dg/dbx = qx,   dg/dby = qy
        if (!PT_FIXED(p1id)) {
            (*J)[eqIdx + 1][PT_OFF(p1id)]     = -lx;
            (*J)[eqIdx + 1][PT_OFF(p1id) + 1] = -ly;
        }
        if (!PT_FIXED(p2id)) {
            (*J)[eqIdx + 1][PT_OFF(p2id)]     =  lx;
            (*J)[eqIdx + 1][PT_OFF(p2id) + 1] =  ly;
        }
        if (!PT_FIXED(line.startPointId)) {
            (*J)[eqIdx + 1][PT_OFF(line.startPointId)]     = -qx;
            (*J)[eqIdx + 1][PT_OFF(line.startPointId) + 1] = -qy;
        }
        if (!PT_FIXED(line.endPointId)) {
            (*J)[eqIdx + 1][PT_OFF(line.endPointId)]       =  qx;
            (*J)[eqIdx + 1][PT_OFF(line.endPointId) + 1]   =  qy;
        }
    }
    return 2;
}

// ---------- Midpoint: entityIds = [pointId, lineId] ----------
// mx - (ax+bx)/2 = 0,  my - (ay+by)/2 = 0
int SketchSolver::emitMidpoint(const SketchConstraint& c, const std::vector<double>& X,
                                std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    const auto& pid = c.entityIds[0];
    auto& line = m_lines->at(c.entityIds[1]);
    PT_XY(p, pid);
    PT_XY(a, line.startPointId); PT_XY(b, line.endPointId);

    F[eqIdx]     = px - (ax + bx) * 0.5;
    F[eqIdx + 1] = py - (ay + by) * 0.5;

    if (J) {
        if (!PT_FIXED(pid)) {
            (*J)[eqIdx][PT_OFF(pid)]         = 1.0;
            (*J)[eqIdx + 1][PT_OFF(pid) + 1] = 1.0;
        }
        if (!PT_FIXED(line.startPointId)) {
            (*J)[eqIdx][PT_OFF(line.startPointId)]         = -0.5;
            (*J)[eqIdx + 1][PT_OFF(line.startPointId) + 1] = -0.5;
        }
        if (!PT_FIXED(line.endPointId)) {
            (*J)[eqIdx][PT_OFF(line.endPointId)]           = -0.5;
            (*J)[eqIdx + 1][PT_OFF(line.endPointId) + 1]   = -0.5;
        }
    }
    return 2;
}

// ---------- Concentric: entityIds = [circleId1, circleId2] ----------
// cx1 - cx2 = 0,  cy1 - cy2 = 0
int SketchSolver::emitConcentric(const SketchConstraint& c, const std::vector<double>& X,
                                  std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    auto& c1 = m_circles->at(c.entityIds[0]);
    auto& c2 = m_circles->at(c.entityIds[1]);
    PT_XY(ctr1, c1.centerPointId); PT_XY(ctr2, c2.centerPointId);

    F[eqIdx]     = ctr1x - ctr2x;
    F[eqIdx + 1] = ctr1y - ctr2y;

    if (J) {
        if (!PT_FIXED(c1.centerPointId)) {
            (*J)[eqIdx][PT_OFF(c1.centerPointId)]         =  1.0;
            (*J)[eqIdx + 1][PT_OFF(c1.centerPointId) + 1] =  1.0;
        }
        if (!PT_FIXED(c2.centerPointId)) {
            (*J)[eqIdx][PT_OFF(c2.centerPointId)]         = -1.0;
            (*J)[eqIdx + 1][PT_OFF(c2.centerPointId) + 1] = -1.0;
        }
    }
    return 2;
}

// ---------- FixedAngle: entityIds = [lineId], value = theta (radians) ----------
// (bx-ax)*sin(theta) - (by-ay)*cos(theta) = 0
int SketchSolver::emitFixedAngle(const SketchConstraint& c, const std::vector<double>& X,
                                  std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    auto& line = m_lines->at(c.entityIds[0]);
    PT_XY(a, line.startPointId); PT_XY(b, line.endPointId);
    double theta = c.value;
    double st = std::sin(theta), ct = std::cos(theta);

    double dx = bx - ax, dy = by - ay;
    F[eqIdx] = dx * st - dy * ct;

    if (J) {
        // df/dax = -st, df/day = ct, df/dbx = st, df/dby = -ct
        if (!PT_FIXED(line.startPointId)) {
            (*J)[eqIdx][PT_OFF(line.startPointId)]     = -st;
            (*J)[eqIdx][PT_OFF(line.startPointId) + 1] =  ct;
        }
        if (!PT_FIXED(line.endPointId)) {
            (*J)[eqIdx][PT_OFF(line.endPointId)]       =  st;
            (*J)[eqIdx][PT_OFF(line.endPointId) + 1]   = -ct;
        }
    }
    return 1;
}

// ---------- AngleBetween: entityIds = [lineId1, lineId2], value = theta (radians) ----------
// dot(d1,d2) - |d1|*|d2|*cos(theta) = 0
// Squared form to avoid sqrt: (d1.d2)^2 - cos^2(theta)*(d1.d1)*(d2.d2) = 0
int SketchSolver::emitAngleBetween(const SketchConstraint& c, const std::vector<double>& X,
                                    std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    auto& l1 = m_lines->at(c.entityIds[0]);
    auto& l2 = m_lines->at(c.entityIds[1]);
    PT_XY(a1, l1.startPointId); PT_XY(b1, l1.endPointId);
    PT_XY(a2, l2.startPointId); PT_XY(b2, l2.endPointId);
    double theta = c.value;
    double cosT = std::cos(theta);
    double cos2 = cosT * cosT;

    double d1x = b1x - a1x, d1y = b1y - a1y;
    double d2x = b2x - a2x, d2y = b2y - a2y;
    double dot = d1x * d2x + d1y * d2y;
    double l1sq = d1x * d1x + d1y * d1y;
    double l2sq = d2x * d2x + d2y * d2y;

    F[eqIdx] = dot * dot - cos2 * l1sq * l2sq;

    if (J) {
        // f = D^2 - cos2 * L1 * L2   where D = dot, L1 = l1sq, L2 = l2sq
        // df/d(var) = 2*D*dD/d(var) - cos2*(dL1/d(var)*L2 + L1*dL2/d(var))
        // dD/da1x = -d2x, dD/da1y = -d2y, dD/db1x = d2x, dD/db1y = d2y
        // dD/da2x = -d1x, dD/da2y = -d1y, dD/db2x = d1x, dD/db2y = d1y
        // dL1/da1x = -2*d1x, dL1/da1y = -2*d1y, dL1/db1x = 2*d1x, dL1/db1y = 2*d1y
        // dL2/da2x = -2*d2x, dL2/da2y = -2*d2y, dL2/db2x = 2*d2x, dL2/db2y = 2*d2y
        if (!PT_FIXED(l1.startPointId)) {
            (*J)[eqIdx][PT_OFF(l1.startPointId)]     = 2.0*dot*(-d2x) - cos2*((-2.0*d1x)*l2sq);
            (*J)[eqIdx][PT_OFF(l1.startPointId) + 1] = 2.0*dot*(-d2y) - cos2*((-2.0*d1y)*l2sq);
        }
        if (!PT_FIXED(l1.endPointId)) {
            (*J)[eqIdx][PT_OFF(l1.endPointId)]       = 2.0*dot*(d2x)  - cos2*((2.0*d1x)*l2sq);
            (*J)[eqIdx][PT_OFF(l1.endPointId) + 1]   = 2.0*dot*(d2y)  - cos2*((2.0*d1y)*l2sq);
        }
        if (!PT_FIXED(l2.startPointId)) {
            (*J)[eqIdx][PT_OFF(l2.startPointId)]     = 2.0*dot*(-d1x) - cos2*(l1sq*(-2.0*d2x));
            (*J)[eqIdx][PT_OFF(l2.startPointId) + 1] = 2.0*dot*(-d1y) - cos2*(l1sq*(-2.0*d2y));
        }
        if (!PT_FIXED(l2.endPointId)) {
            (*J)[eqIdx][PT_OFF(l2.endPointId)]       = 2.0*dot*(d1x)  - cos2*(l1sq*(2.0*d2x));
            (*J)[eqIdx][PT_OFF(l2.endPointId) + 1]   = 2.0*dot*(d1y)  - cos2*(l1sq*(2.0*d2y));
        }
    }
    return 1;
}

// ---------- Radius: entityIds = [circleId or arcId], value = r ----------
// r_current - value = 0
int SketchSolver::emitRadius(const SketchConstraint& c, const std::vector<double>& X,
                              std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    // Try circle first, then arc
    auto cit = m_circles->find(c.entityIds[0]);
    if (cit != m_circles->end()) {
        double r = X[cit->second.radiusParamOffset];
        F[eqIdx] = r - c.value;
        if (J) {
            (*J)[eqIdx][cit->second.radiusParamOffset] = 1.0;
        }
        return 1;
    }
    auto ait = m_arcs->find(c.entityIds[0]);
    if (ait != m_arcs->end()) {
        double r = X[ait->second.radiusParamOffset];
        F[eqIdx] = r - c.value;
        if (J) {
            (*J)[eqIdx][ait->second.radiusParamOffset] = 1.0;
        }
        return 1;
    }
    F[eqIdx] = 0.0;
    return 1;
}

// ---------- Fix: entityIds = [pointId], value = x_fixed, value2 = y_fixed ----------
// x - x_fixed = 0,  y - y_fixed = 0
int SketchSolver::emitFix(const SketchConstraint& c, const std::vector<double>& X,
                           std::vector<double>& F, std::vector<std::vector<double>>* J, int eqIdx) const
{
    const auto& pid = c.entityIds[0];
    PT_XY(p, pid);
    double xFixed = c.value;
    double yFixed = c.value2;

    F[eqIdx]     = px - xFixed;
    F[eqIdx + 1] = py - yFixed;

    if (J) {
        if (!PT_FIXED(pid)) {
            (*J)[eqIdx][PT_OFF(pid)]         = 1.0;
            (*J)[eqIdx + 1][PT_OFF(pid) + 1] = 1.0;
        }
    }
    return 2;
}

#undef PT_XY
#undef PT_FIXED
#undef PT_OFF

} // namespace sketch
