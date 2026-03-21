#pragma once
#include "Command.h"
#include <vector>
#include <memory>
#include <string>

namespace document {

class Document;

/// Manages the undo/redo stacks for a Document.
class CommandHistory {
public:
    /// Execute a command and push it onto the undo stack.
    /// Clears the redo stack (any previously-undone commands are discarded).
    void execute(std::unique_ptr<Command> cmd, Document& doc);

    /// Undo the last command. Returns false if nothing to undo.
    bool undo(Document& doc);

    /// Redo the last undone command. Returns false if nothing to redo.
    bool redo(Document& doc);

    bool canUndo() const;
    bool canRedo() const;

    /// Description of the command that would be undone (for menu text).
    std::string undoDescription() const;

    /// Description of the command that would be redone (for menu text).
    std::string redoDescription() const;

    /// Clear both stacks (called on new document / load).
    void clear();

private:
    std::vector<std::unique_ptr<Command>> m_undoStack;
    std::vector<std::unique_ptr<Command>> m_redoStack;
};

} // namespace document
