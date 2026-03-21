#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace document {

/// Minimal recursive-descent JSON parser.
/// Parses a JSON string into an in-memory tree that can be queried.
/// No external dependencies.
class JsonValue
{
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    double numberVal = 0.0;
    bool boolVal = false;
    std::string stringVal;
    std::vector<std::shared_ptr<JsonValue>> arrayVal;
    // Ordered keys to preserve insertion order + map for lookup.
    std::vector<std::string> objectKeys;
    std::unordered_map<std::string, std::shared_ptr<JsonValue>> objectVal;

    // Convenience accessors with defaults
    std::string getString(const std::string& key, const std::string& def = "") const
    {
        auto it = objectVal.find(key);
        if (it != objectVal.end() && it->second->type == Type::String)
            return it->second->stringVal;
        return def;
    }

    double getNumber(const std::string& key, double def = 0.0) const
    {
        auto it = objectVal.find(key);
        if (it != objectVal.end() && it->second->type == Type::Number)
            return it->second->numberVal;
        return def;
    }

    int getInt(const std::string& key, int def = 0) const
    {
        return static_cast<int>(getNumber(key, static_cast<double>(def)));
    }

    bool getBool(const std::string& key, bool def = false) const
    {
        auto it = objectVal.find(key);
        if (it != objectVal.end() && it->second->type == Type::Bool)
            return it->second->boolVal;
        return def;
    }

    const JsonValue* getObject(const std::string& key) const
    {
        auto it = objectVal.find(key);
        if (it != objectVal.end() && it->second->type == Type::Object)
            return it->second.get();
        return nullptr;
    }

    const JsonValue* getArray(const std::string& key) const
    {
        auto it = objectVal.find(key);
        if (it != objectVal.end() && it->second->type == Type::Array)
            return it->second.get();
        return nullptr;
    }

    bool has(const std::string& key) const
    {
        return objectVal.find(key) != objectVal.end();
    }
};

class JsonReader
{
public:
    /// Parse a JSON string. Returns nullptr on failure.
    static std::shared_ptr<JsonValue> parse(const std::string& json);

private:
    explicit JsonReader(const std::string& json);

    std::shared_ptr<JsonValue> parseValue();
    std::shared_ptr<JsonValue> parseObject();
    std::shared_ptr<JsonValue> parseArray();
    std::shared_ptr<JsonValue> parseString();
    std::shared_ptr<JsonValue> parseNumber();
    std::shared_ptr<JsonValue> parseLiteral();

    void skipWhitespace();
    char peek() const;
    char advance();
    bool match(char c);
    std::string parseRawString();

    std::string m_json;
    size_t m_pos = 0;
};

} // namespace document
