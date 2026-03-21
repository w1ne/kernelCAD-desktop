#include "ParameterStore.h"
#include <stdexcept>
#include <cmath>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>
#include <queue>
#include <algorithm>

namespace document {

// ---------------------------------------------------------------------------
// Recursive-descent expression parser (self-contained, no external deps)
//
//   Grammar:
//     expr       -> term (('+' | '-') term)*
//     term       -> unary (('*' | '/') unary)*
//     unary      -> '-' unary | primary
//     primary    -> NUMBER
//                 | IDENTIFIER '(' arglist ')'      -- function call
//                 | IDENTIFIER                      -- constant or param ref
//                 | '(' expr ')'
//     arglist    -> expr (',' expr)*
//
//   Supported functions : sin cos tan sqrt abs min max pow
//   Supported constants : PI
//   Unit stripping      : trailing "mm", "deg", "rad", "in", "cm", "m" after
//                          a number are silently consumed.
// ---------------------------------------------------------------------------

namespace {

struct Parser {
    const std::string& src;
    const std::unordered_map<std::string, Parameter>& params;
    size_t pos = 0;

    explicit Parser(const std::string& s,
                    const std::unordered_map<std::string, Parameter>& p)
        : src(s), params(p) {}

    // -- helpers ----------------------------------------------------------

    void skipSpaces() {
        while (pos < src.size() && std::isspace(static_cast<unsigned char>(src[pos])))
            ++pos;
    }

    bool match(char c) {
        skipSpaces();
        if (pos < src.size() && src[pos] == c) { ++pos; return true; }
        return false;
    }

    [[noreturn]] void error(const std::string& msg) const {
        throw std::runtime_error("Expression error at position "
                                 + std::to_string(pos) + ": " + msg
                                 + " in \"" + src + "\"");
    }

    // -- token helpers ----------------------------------------------------

    // Try to parse a floating-point number (including forms like .5 or 3.)
    bool tryNumber(double& out) {
        skipSpaces();
        if (pos >= src.size()) return false;

        size_t start = pos;
        // optional leading digits
        while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos])))
            ++pos;
        // optional decimal point
        if (pos < src.size() && src[pos] == '.') {
            ++pos;
            while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos])))
                ++pos;
        }
        // must have consumed at least something
        if (pos == start) return false;

        // optional exponent
        if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
            ++pos;
            if (pos < src.size() && (src[pos] == '+' || src[pos] == '-'))
                ++pos;
            if (pos >= src.size() || !std::isdigit(static_cast<unsigned char>(src[pos])))
                error("malformed exponent");
            while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos])))
                ++pos;
        }

        out = std::stod(src.substr(start, pos - start));

        // Unit stripping: if the number is immediately followed by a known
        // unit suffix (and that suffix is NOT followed by an alphanumeric
        // character), consume and discard it.
        skipSpaces();
        static const char* units[] = {"mm", "deg", "rad", "cm", "in", "m"};
        for (const char* u : units) {
            size_t ulen = std::strlen(u);
            if (pos + ulen <= src.size()
                && src.compare(pos, ulen, u) == 0
                && (pos + ulen >= src.size()
                    || !std::isalnum(static_cast<unsigned char>(src[pos + ulen])))) {
                pos += ulen;
                break;
            }
        }
        return true;
    }

    // Parse an identifier ([A-Za-z_][A-Za-z0-9_]*)
    bool tryIdentifier(std::string& out) {
        skipSpaces();
        if (pos >= src.size()) return false;
        char c = src[pos];
        if (!std::isalpha(static_cast<unsigned char>(c)) && c != '_')
            return false;
        size_t start = pos;
        while (pos < src.size()
               && (std::isalnum(static_cast<unsigned char>(src[pos])) || src[pos] == '_'))
            ++pos;
        out = src.substr(start, pos - start);
        return true;
    }

    // -- grammar rules ----------------------------------------------------

    double parseExpr() {
        double val = parseTerm();
        for (;;) {
            skipSpaces();
            if (match('+'))      val += parseTerm();
            else if (match('-')) val -= parseTerm();
            else break;
        }
        return val;
    }

    double parseTerm() {
        double val = parseUnary();
        for (;;) {
            skipSpaces();
            if (match('*'))      val *= parseUnary();
            else if (match('/')) {
                double d = parseUnary();
                if (d == 0.0)
                    error("division by zero");
                val /= d;
            }
            else break;
        }
        return val;
    }

    double parseUnary() {
        if (match('-'))
            return -parseUnary();
        if (match('+'))
            return parseUnary();
        return parsePrimary();
    }

    double parsePrimary() {
        skipSpaces();

        // Parenthesised sub-expression
        if (match('(')) {
            double val = parseExpr();
            if (!match(')'))
                error("expected ')'");
            return val;
        }

        // Number literal (possibly followed by a unit suffix)
        double numVal;
        if (tryNumber(numVal))
            return numVal;

        // Identifier: could be a function call, constant, or parameter ref
        std::string id;
        if (tryIdentifier(id)) {
            skipSpaces();

            // --- function call ---
            if (match('(')) {
                // Parse argument list
                std::vector<double> args;
                if (!match(')')) {
                    args.push_back(parseExpr());
                    while (match(','))
                        args.push_back(parseExpr());
                    if (!match(')'))
                        error("expected ')' after function arguments");
                }
                return callFunction(id, args);
            }

            // --- built-in constant ---
            if (id == "PI" || id == "pi")
                return M_PI;
            if (id == "E" || id == "e")
                return M_E;

            // --- parameter name lookup ---
            auto it = params.find(id);
            if (it != params.end())
                return it->second.cachedValue;

            error("unknown identifier '" + id + "'");
        }

        error("unexpected character");
    }

    // -- built-in functions -----------------------------------------------

    double callFunction(const std::string& name,
                        const std::vector<double>& args) const {
        // Single-argument functions
        if (name == "sin")  { expect1(name, args); return std::sin(args[0]); }
        if (name == "cos")  { expect1(name, args); return std::cos(args[0]); }
        if (name == "tan")  { expect1(name, args); return std::tan(args[0]); }
        if (name == "sqrt") { expect1(name, args); return std::sqrt(args[0]); }
        if (name == "abs")  { expect1(name, args); return std::fabs(args[0]); }

        // Two-argument functions
        if (name == "min")  { expect2(name, args); return std::fmin(args[0], args[1]); }
        if (name == "max")  { expect2(name, args); return std::fmax(args[0], args[1]); }
        if (name == "pow")  { expect2(name, args); return std::pow(args[0], args[1]); }

        error("unknown function '" + name + "'");
    }

    void expect1(const std::string& fn, const std::vector<double>& a) const {
        if (a.size() != 1)
            error(fn + "() expects 1 argument, got " + std::to_string(a.size()));
    }
    void expect2(const std::string& fn, const std::vector<double>& a) const {
        if (a.size() != 2)
            error(fn + "() expects 2 arguments, got " + std::to_string(a.size()));
    }
};

// Scan an expression string and return the set of identifiers that look like
// parameter references (i.e. not function names, not built-in constants).
// This is a lightweight lexical scan -- it does NOT need a full parse.
std::unordered_set<std::string> collectIdentifiers(
        const std::string& src,
        const std::unordered_map<std::string, Parameter>& params)
{
    static const std::unordered_set<std::string> builtins = {
        "sin", "cos", "tan", "sqrt", "abs", "min", "max", "pow",
        "PI", "pi", "E", "e"
    };

    std::unordered_set<std::string> refs;
    size_t pos = 0;
    while (pos < src.size()) {
        // skip non-alpha
        if (!std::isalpha(static_cast<unsigned char>(src[pos])) && src[pos] != '_') {
            ++pos;
            continue;
        }
        size_t start = pos;
        while (pos < src.size()
               && (std::isalnum(static_cast<unsigned char>(src[pos])) || src[pos] == '_'))
            ++pos;
        std::string id = src.substr(start, pos - start);

        // skip whitespace to see if '(' follows (function call)
        size_t peek = pos;
        while (peek < src.size() && std::isspace(static_cast<unsigned char>(src[peek])))
            ++peek;
        bool isCall = (peek < src.size() && src[peek] == '(');

        // Known unit suffixes -- skip them
        static const std::unordered_set<std::string> units = {
            "mm", "cm", "m", "deg", "rad", "in"
        };

        if (!isCall && builtins.find(id) == builtins.end()
            && units.find(id) == units.end()) {
            // Could be a param reference -- include if it exists or is at
            // least plausibly a forward reference.  For cycle detection we
            // include any identifier that is already a parameter.
            if (params.count(id))
                refs.insert(id);
        }
    }
    return refs;
}

} // anonymous namespace

void ParameterStore::set(const std::string& name,
                          const std::string& expression,
                          const std::string& unit)
{
    // --- Circular-dependency check ---
    if (wouldCreateCycle(name, expression))
        throw std::runtime_error(
            "Circular dependency: setting '" + name
            + "' to \"" + expression + "\" creates a cycle");

    Parameter p;
    p.name       = name;
    p.expression = expression;
    p.unit       = unit;
    p.cachedValue = evaluate(expression);
    m_params[name] = std::move(p);

    // --- Propagate to downstream dependents ---
    propagateChange(name);
}

double ParameterStore::evaluate(const std::string& expression) const
{
    if (expression.empty())
        throw std::runtime_error("Cannot evaluate empty expression");

    Parser parser(expression, m_params);
    double result = parser.parseExpr();

    // Make sure the entire input was consumed
    parser.skipSpaces();
    if (parser.pos < expression.size())
        throw std::runtime_error("Unexpected trailing characters at position "
                                 + std::to_string(parser.pos)
                                 + " in \"" + expression + "\"");
    return result;
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

// ---------------------------------------------------------------------------
// Dependency tracking helpers
// ---------------------------------------------------------------------------

std::unordered_set<std::string>
ParameterStore::referencedParams(const std::string& expression) const
{
    return collectIdentifiers(expression, m_params);
}

bool ParameterStore::wouldCreateCycle(const std::string& name,
                                       const std::string& expression) const
{
    // Collect the parameters directly referenced by the new expression.
    auto directDeps = collectIdentifiers(expression, m_params);

    // If `name` is among its own direct dependencies, that is trivially cyclic.
    if (directDeps.count(name))
        return true;

    // Check whether any direct dependency transitively depends on `name`.
    // We do a BFS from each direct dependency, walking the existing
    // dependency edges (expr -> referenced params).  If we ever reach `name`,
    // there is a cycle.
    std::unordered_set<std::string> visited;
    std::queue<std::string> q;
    for (const auto& dep : directDeps) {
        if (visited.insert(dep).second)
            q.push(dep);
    }

    while (!q.empty()) {
        std::string cur = q.front();
        q.pop();

        auto it = m_params.find(cur);
        if (it == m_params.end())
            continue;

        auto refs = collectIdentifiers(it->second.expression, m_params);
        for (const auto& r : refs) {
            if (r == name)
                return true;
            if (visited.insert(r).second)
                q.push(r);
        }
    }
    return false;
}

void ParameterStore::collectDependents(const std::string& name,
                                        std::unordered_set<std::string>& visited) const
{
    // A parameter P depends on `name` if `name` appears in P's expression.
    std::queue<std::string> q;
    q.push(name);

    while (!q.empty()) {
        std::string cur = q.front();
        q.pop();

        for (const auto& [pname, param] : m_params) {
            if (visited.count(pname))
                continue;
            auto refs = collectIdentifiers(param.expression, m_params);
            if (refs.count(cur)) {
                visited.insert(pname);
                q.push(pname);
            }
        }
    }
}

std::vector<std::string>
ParameterStore::topoOrder(const std::unordered_set<std::string>& dirty) const
{
    // Build a mini dependency graph over the dirty set and topologically sort.
    std::unordered_map<std::string, std::unordered_set<std::string>> deps;
    std::unordered_map<std::string, int> inDegree;

    for (const auto& n : dirty) {
        deps[n];          // ensure entry
        inDegree[n] = 0;
    }

    for (const auto& n : dirty) {
        auto it = m_params.find(n);
        if (it == m_params.end()) continue;
        auto refs = collectIdentifiers(it->second.expression, m_params);
        for (const auto& r : refs) {
            if (dirty.count(r)) {
                deps[r].insert(n);
                inDegree[n]++;
            }
        }
    }

    // Kahn's algorithm
    std::queue<std::string> ready;
    for (const auto& [n, deg] : inDegree) {
        if (deg == 0)
            ready.push(n);
    }

    std::vector<std::string> order;
    order.reserve(dirty.size());

    while (!ready.empty()) {
        std::string cur = ready.front();
        ready.pop();
        order.push_back(cur);

        for (const auto& dep : deps[cur]) {
            if (--inDegree[dep] == 0)
                ready.push(dep);
        }
    }
    return order;
}

void ParameterStore::propagateChange(const std::string& name)
{
    // Collect all parameters that transitively depend on `name`.
    std::unordered_set<std::string> dirty;
    collectDependents(name, dirty);

    if (dirty.empty())
        return;

    // Re-evaluate in topological order so each param sees up-to-date deps.
    auto order = topoOrder(dirty);
    for (const auto& pname : order) {
        auto it = m_params.find(pname);
        if (it != m_params.end())
            it->second.cachedValue = evaluate(it->second.expression);
    }
}

} // namespace document
