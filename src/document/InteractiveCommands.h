#pragma once
#include "CommandInput.h"
#include "../features/ExtrudeFeature.h"
#include "../features/FilletFeature.h"
#include "../features/HoleFeature.h"
#include "../features/ChamferFeature.h"
#include "../features/ShellFeature.h"

namespace document {

class ExtrudeInteractiveCommand : public InteractiveCommand {
public:
    std::vector<CommandInputDef> inputDefinitions() const override;
    void executeWithInputs(Document& doc, const CommandInputValues& inputs) override;
    std::string description() const override { return "Extrude"; }
};

class FilletInteractiveCommand : public InteractiveCommand {
public:
    std::vector<CommandInputDef> inputDefinitions() const override;
    void executeWithInputs(Document& doc, const CommandInputValues& inputs) override;
    std::string description() const override { return "Fillet"; }
};

class HoleInteractiveCommand : public InteractiveCommand {
public:
    std::vector<CommandInputDef> inputDefinitions() const override;
    void executeWithInputs(Document& doc, const CommandInputValues& inputs) override;
    std::string description() const override { return "Hole"; }
};

class ChamferInteractiveCommand : public InteractiveCommand {
public:
    std::vector<CommandInputDef> inputDefinitions() const override;
    void executeWithInputs(Document& doc, const CommandInputValues& inputs) override;
    std::string description() const override { return "Chamfer"; }
};

class ShellInteractiveCommand : public InteractiveCommand {
public:
    std::vector<CommandInputDef> inputDefinitions() const override;
    void executeWithInputs(Document& doc, const CommandInputValues& inputs) override;
    std::string description() const override { return "Shell"; }
};

} // namespace document
