#include "JsonReader.h"
#include <cstdlib>
#include <stdexcept>

namespace document {

JsonReader::JsonReader(const std::string& json)
    : m_json(json) {}

std::shared_ptr<JsonValue> JsonReader::parse(const std::string& json)
{
    JsonReader reader(json);
    reader.skipWhitespace();
    if (reader.m_pos >= reader.m_json.size())
        return nullptr;
    auto val = reader.parseValue();
    return val;
}

std::shared_ptr<JsonValue> JsonReader::parseValue()
{
    skipWhitespace();
    if (m_pos >= m_json.size())
        return nullptr;

    char c = peek();
    if (c == '{')
        return parseObject();
    if (c == '[')
        return parseArray();
    if (c == '"')
        return parseString();
    if (c == 't' || c == 'f')
        return parseLiteral();
    if (c == 'n')
        return parseLiteral();
    // Must be a number
    return parseNumber();
}

std::shared_ptr<JsonValue> JsonReader::parseObject()
{
    auto val = std::make_shared<JsonValue>();
    val->type = JsonValue::Type::Object;

    advance(); // consume '{'
    skipWhitespace();

    if (peek() == '}') {
        advance();
        return val;
    }

    while (true) {
        skipWhitespace();
        if (peek() != '"')
            return nullptr; // error

        std::string key = parseRawString();

        skipWhitespace();
        if (!match(':'))
            return nullptr;

        skipWhitespace();
        auto child = parseValue();
        if (!child)
            return nullptr;

        val->objectKeys.push_back(key);
        val->objectVal[key] = child;

        skipWhitespace();
        if (peek() == ',') {
            advance();
            continue;
        }
        break;
    }

    skipWhitespace();
    if (!match('}'))
        return nullptr;

    return val;
}

std::shared_ptr<JsonValue> JsonReader::parseArray()
{
    auto val = std::make_shared<JsonValue>();
    val->type = JsonValue::Type::Array;

    advance(); // consume '['
    skipWhitespace();

    if (peek() == ']') {
        advance();
        return val;
    }

    while (true) {
        skipWhitespace();
        auto child = parseValue();
        if (!child)
            return nullptr;
        val->arrayVal.push_back(child);

        skipWhitespace();
        if (peek() == ',') {
            advance();
            continue;
        }
        break;
    }

    skipWhitespace();
    if (!match(']'))
        return nullptr;

    return val;
}

std::shared_ptr<JsonValue> JsonReader::parseString()
{
    auto val = std::make_shared<JsonValue>();
    val->type = JsonValue::Type::String;
    val->stringVal = parseRawString();
    return val;
}

std::shared_ptr<JsonValue> JsonReader::parseNumber()
{
    auto val = std::make_shared<JsonValue>();
    val->type = JsonValue::Type::Number;

    size_t start = m_pos;
    // Consume sign
    if (m_pos < m_json.size() && (m_json[m_pos] == '-' || m_json[m_pos] == '+'))
        m_pos++;
    // Consume digits, dot, exponent
    while (m_pos < m_json.size()) {
        char c = m_json[m_pos];
        if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
            // Avoid consuming a sign that isn't part of an exponent
            if ((c == '+' || c == '-') && m_pos > start) {
                char prev = m_json[m_pos - 1];
                if (prev != 'e' && prev != 'E')
                    break;
            }
            m_pos++;
        } else {
            break;
        }
    }

    std::string numStr = m_json.substr(start, m_pos - start);
    val->numberVal = std::stod(numStr);
    return val;
}

std::shared_ptr<JsonValue> JsonReader::parseLiteral()
{
    auto val = std::make_shared<JsonValue>();

    if (m_json.compare(m_pos, 4, "true") == 0) {
        val->type = JsonValue::Type::Bool;
        val->boolVal = true;
        m_pos += 4;
    } else if (m_json.compare(m_pos, 5, "false") == 0) {
        val->type = JsonValue::Type::Bool;
        val->boolVal = false;
        m_pos += 5;
    } else if (m_json.compare(m_pos, 4, "null") == 0) {
        val->type = JsonValue::Type::Null;
        m_pos += 4;
    } else {
        return nullptr;
    }
    return val;
}

void JsonReader::skipWhitespace()
{
    while (m_pos < m_json.size()) {
        char c = m_json[m_pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            m_pos++;
        else
            break;
    }
}

char JsonReader::peek() const
{
    if (m_pos >= m_json.size())
        return '\0';
    return m_json[m_pos];
}

char JsonReader::advance()
{
    if (m_pos >= m_json.size())
        return '\0';
    return m_json[m_pos++];
}

bool JsonReader::match(char c)
{
    if (m_pos < m_json.size() && m_json[m_pos] == c) {
        m_pos++;
        return true;
    }
    return false;
}

std::string JsonReader::parseRawString()
{
    if (m_pos >= m_json.size() || m_json[m_pos] != '"')
        return "";

    m_pos++; // skip opening quote
    std::string result;
    while (m_pos < m_json.size()) {
        char c = m_json[m_pos++];
        if (c == '"')
            return result;
        if (c == '\\') {
            if (m_pos >= m_json.size())
                break;
            char esc = m_json[m_pos++];
            switch (esc) {
            case '"':  result += '"';  break;
            case '\\': result += '\\'; break;
            case '/':  result += '/';  break;
            case 'n':  result += '\n'; break;
            case 'r':  result += '\r'; break;
            case 't':  result += '\t'; break;
            case 'b':  result += '\b'; break;
            case 'f':  result += '\f'; break;
            default:   result += esc;  break;
            }
        } else {
            result += c;
        }
    }
    return result;
}

} // namespace document
