#pragma once
#include <string>
#include <memory>

namespace document {

class Document;

/// Base class for all undoable commands (Command pattern).
/// Each concrete command captures enough state to execute and undo itself.
class Command {
public:
    virtual ~Command() = default;

    /// Human-readable description shown in Edit > Undo/Redo menu items.
    virtual std::string description() const = 0;

    /// Execute the command (first time or redo).
    virtual void execute(Document& doc) = 0;

    /// Undo the command, restoring the previous state.
    virtual void undo(Document& doc) = 0;
};

} // namespace document
