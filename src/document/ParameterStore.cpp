#include "ParameterStore.h"
#include <stdexcept>

namespace document {

void ParameterStore::set(const std::string& name,
                          const std::string& expression,
                          const std::string& unit)
{
    Parameter p;
    p.name       = name;
    p.expression = expression;
    p.unit       = unit;
    p.cachedValue = evaluate(expression);
    m_params[name] = std::move(p);
}

double ParameterStore::evaluate(const std::string& expression) const
{
    // TODO: integrate exprtk or muParser for full expression evaluation
    // For now try direct numeric parse
    try {
        return std::stod(expression);
    } catch (...) {
        // Try resolving as a parameter name
        auto it = m_params.find(expression);
        if (it != m_params.end())
            return it->second.cachedValue;
        throw std::runtime_error("Cannot evaluate expression: " + expression);
    }
}

double ParameterStore::get(const std::string& name) const
{
    auto it = m_params.find(name);
    if (it == m_params.end())
        throw std::runtime_error("Parameter not found: " + name);
    return it->second.cachedValue;
}

bool ParameterStore::has(const std::string& name) const
{
    return m_params.count(name) > 0;
}

void ParameterStore::remove(const std::string& name)
{
    m_params.erase(name);
}

} // namespace document
