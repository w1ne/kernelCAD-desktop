#pragma once
#include <string>
#include <functional>
#include <memory>

namespace document { class Document; }

namespace scripting {

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    /// Process a single JSON command string. Returns a JSON result string.
    /// Never throws -- errors are returned as {"error": "message"}.
    std::string execute(const std::string& jsonCommand);

    /// Process a batch of commands (JSON array). Returns JSON array of results.
    std::string executeBatch(const std::string& jsonArray);

    /// Access the underlying document (for integration with GUI).
    document::Document& document();

    /// Set a callback for log messages.
    using LogCallback = std::function<void(const std::string&)>;
    void setLogCallback(LogCallback cb);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace scripting
