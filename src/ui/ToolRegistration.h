#pragma once

class CommandController;
class MainWindow;

/// Register all tools into the global ToolRegistry.
/// Called once during MainWindow construction.
void registerAllTools(MainWindow* mw, CommandController* cmd);
