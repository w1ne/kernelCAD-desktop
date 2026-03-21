#pragma once
#include "Command.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace document {

enum class CommandInputType {
    Value, Selection, DropDown, CheckBox, TextBox,
    Slider, Angle, Distance, Direction
};

struct CommandInputDef {
    std::string id;
    std::string label;
    CommandInputType type = CommandInputType::Value;
    double defaultValue = 0;
    double minValue = -1e6, maxValue = 1e6;
    std::string defaultExpression;
    std::vector<std::string> dropdownItems;
    int defaultDropdownIndex = 0;
    bool defaultBool = false;
    std::string defaultText;
    std::string selectionFilter = "All";
    int minSelections = 1, maxSelections = 1;
    bool isRequired = true;
};

struct CommandInputValues {
    std::unordered_map<std::string, double> numericValues;
    std::unordered_map<std::string, std::string> stringValues;
    std::unordered_map<std::string, int> intValues;
    std::unordered_map<std::string, bool> boolValues;
    std::unordered_map<std::string, std::vector<std::string>> selectionValues;

    double getNumeric(const std::string& id, double def = 0) const {
        auto it = numericValues.find(id); return it != numericValues.end() ? it->second : def;
    }
    std::string getString(const std::string& id, const std::string& def = "") const {
        auto it = stringValues.find(id); return it != stringValues.end() ? it->second : def;
    }
    int getInt(const std::string& id, int def = 0) const {
        auto it = intValues.find(id); return it != intValues.end() ? it->second : def;
    }
    bool getBool(const std::string& id, bool def = false) const {
        auto it = boolValues.find(id); return it != boolValues.end() ? it->second : def;
    }
};

/// Extended command that declares its inputs for dynamic UI generation.
class InteractiveCommand : public Command {
public:
    virtual std::vector<CommandInputDef> inputDefinitions() const = 0;
    virtual void executeWithInputs(Document& doc, const CommandInputValues& inputs) = 0;
    virtual bool preview(Document& doc, const CommandInputValues& inputs) {
        (void)doc; (void)inputs; return false;
    }

    void execute(Document& doc) override { executeWithInputs(doc, m_values); }
    void undo(Document& doc) override;
    void setInputValues(const CommandInputValues& values) { m_values = values; }

protected:
    CommandInputValues m_values;
    std::string m_createdFeatureId;
};

} // namespace document
