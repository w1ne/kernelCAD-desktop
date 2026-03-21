#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>

namespace document {

struct Parameter {
    std::string name;
    std::string expression;  // e.g. "flange_h / 2 + 1"
    double      cachedValue = 0.0;
    std::string unit;        // "mm", "deg", etc.
    std::string comment;
};

class ParameterStore
{
public:
    /// Set (or update) a named parameter.  When the expression references other
    /// parameters, all downstream dependents are automatically re-evaluated.
    /// Throws std::runtime_error if a circular dependency would be created.
    void   set(const std::string& name, const std::string& expression, const std::string& unit = "mm");

    /// Evaluate an arbitrary expression string using the current parameter values.
    double evaluate(const std::string& expression) const;

    /// Get the cached numeric value of a parameter by name.
    double get(const std::string& name) const;

    bool   has(const std::string& name) const;
    void   remove(const std::string& name);

    const std::unordered_map<std::string, Parameter>& all() const { return m_params; }

    /// Return the set of parameter names referenced by an expression.
    std::unordered_set<std::string> referencedParams(const std::string& expression) const;

    /// Return true if setting `name` to `expression` would create a circular
    /// dependency chain.
    bool wouldCreateCycle(const std::string& name, const std::string& expression) const;

private:
    std::unordered_map<std::string, Parameter> m_params;

    /// Re-evaluate every parameter that (transitively) depends on `name`.
    void propagateChange(const std::string& name);

    /// Collect all parameter names that transitively depend on `name`.
    void collectDependents(const std::string& name,
                           std::unordered_set<std::string>& visited) const;

    /// Topological re-evaluation order for a set of dirty parameters.
    std::vector<std::string> topoOrder(const std::unordered_set<std::string>& dirty) const;
};

} // namespace document
