#include "CommandHistory.h"
#include "Document.h"
#include <stdexcept>

namespace document {

void CommandHistory::execute(std::unique_ptr<Command> cmd, Document& doc)
{
    doc.beginTransaction();
    try {
        cmd->execute(doc);
        doc.commitTransaction();
        m_undoStack.push_back(std::move(cmd));
        m_redoStack.clear();
    } catch (const std::exception&) {
        doc.rollbackTransaction();
        throw;  // Re-throw so caller can show error message
    }
}

bool CommandHistory::undo(Document& doc)
{
    if (m_undoStack.empty())
        return false;

    auto cmd = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    cmd->undo(doc);
    m_redoStack.push_back(std::move(cmd));
    return true;
}

bool CommandHistory::redo(Document& doc)
{
    if (m_redoStack.empty())
        return false;

    auto cmd = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    cmd->execute(doc);
    m_undoStack.push_back(std::move(cmd));
    return true;
}

bool CommandHistory::canUndo() const
{
    return !m_undoStack.empty();
}

bool CommandHistory::canRedo() const
{
    return !m_redoStack.empty();
}

std::string CommandHistory::undoDescription() const
{
    if (m_undoStack.empty())
        return {};
    return m_undoStack.back()->description();
}

std::string CommandHistory::redoDescription() const
{
    if (m_redoStack.empty())
        return {};
    return m_redoStack.back()->description();
}

void CommandHistory::clear()
{
    m_undoStack.clear();
    m_redoStack.clear();
}

} // namespace document
