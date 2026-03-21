#pragma once
#include <string>
#include <unordered_map>
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
    void   set(const std::string& name, const std::string& expression, const std::string& unit = "mm");
    double evaluate(const std::string& expression) const;
    double get(const std::string& name) const;
    bool   has(const std::string& name) const;
    void   remove(const std::string& name);

    const std::unordered_map<std::string, Parameter>& all() const { return m_params; }

private:
    std::unordered_map<std::string, Parameter> m_params;
};

} // namespace document
