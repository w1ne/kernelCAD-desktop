#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <iomanip>

namespace document {

/// Minimal hand-rolled JSON writer. Produces compact JSON strings.
/// No external dependencies.
class JsonWriter
{
public:
    void beginObject()
    {
        maybeComma();
        m_buf += '{';
        m_stack.push_back(false); // nothing written in this scope yet
    }

    void endObject()
    {
        m_buf += '}';
        m_stack.pop_back();
        if (!m_stack.empty())
            m_stack.back() = true; // parent scope now has content
    }

    void beginArray(const std::string& key)
    {
        maybeComma();
        writeRawKey(key);
        m_buf += '[';
        m_stack.push_back(false);
    }

    /// Begin an anonymous array element (no key).
    void beginArrayAnon()
    {
        maybeComma();
        m_buf += '[';
        m_stack.push_back(false);
    }

    void endArray()
    {
        m_buf += ']';
        m_stack.pop_back();
        if (!m_stack.empty())
            m_stack.back() = true;
    }

    void writeString(const std::string& key, const std::string& value)
    {
        maybeComma();
        writeRawKey(key);
        m_buf += '"';
        m_buf += escape(value);
        m_buf += '"';
        markWritten();
    }

    void writeNumber(const std::string& key, double value)
    {
        maybeComma();
        writeRawKey(key);
        std::ostringstream oss;
        oss << std::setprecision(17) << value;
        m_buf += oss.str();
        markWritten();
    }

    void writeInt(const std::string& key, int value)
    {
        maybeComma();
        writeRawKey(key);
        m_buf += std::to_string(value);
        markWritten();
    }

    void writeBool(const std::string& key, bool value)
    {
        maybeComma();
        writeRawKey(key);
        m_buf += value ? "true" : "false";
        markWritten();
    }

    /// Write a key whose value will be supplied by the next call
    /// (e.g. beginObject() or beginArrayAnon()).
    void writeKey(const std::string& key)
    {
        maybeComma();
        writeRawKey(key);
        // Suppress the comma that the following beginObject/beginArrayAnon
        // would otherwise emit — the value directly follows the colon.
        if (!m_stack.empty())
            m_stack.back() = false;
    }

    std::string result() const { return m_buf; }

private:
    void maybeComma()
    {
        if (!m_stack.empty() && m_stack.back())
            m_buf += ',';
    }

    void markWritten()
    {
        if (!m_stack.empty())
            m_stack.back() = true;
    }

    void writeRawKey(const std::string& key)
    {
        m_buf += '"';
        m_buf += escape(key);
        m_buf += "\":";
    }

    static std::string escape(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
            }
        }
        return out;
    }

    std::string m_buf;
    // Stack tracks whether each scope (object/array) has had content written.
    std::vector<bool> m_stack;
};

} // namespace document
