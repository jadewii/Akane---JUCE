#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include "ModulationMatrix.h"

/**
 * Macro Assignment
 */
struct MacroAssignment
{
    enum class TargetType
    {
        Parameter,
        ModulationAmount
    };
    
    TargetType type;
    juce::String parameterID;
    float amount = 1.0f;  // -1 to 1
    
    // For modulation amount targets
    ModulationSource::Type modSource;
    ModulationDestination::Type modDest;
    
    MacroAssignment() = default;
    
    MacroAssignment(const juce::String& paramID, float amt)
        : type(TargetType::Parameter), parameterID(paramID), amount(amt) {}
    
    MacroAssignment(ModulationSource::Type src, ModulationDestination::Type dest, float amt)
        : type(TargetType::ModulationAmount), parameterID(), amount(amt), modSource(src), modDest(dest) {}
};

/**
 * Macro Control
 */
class MacroControl
{
public:
    MacroControl(int index) : macroIndex(index)
    {
        // REAL-TIME SAFE: Use pre-formatted strings to avoid dynamic string operations
        static const char* macroNames[8] = {
            "Macro 1", "Macro 2", "Macro 3", "Macro 4",
            "Macro 5", "Macro 6", "Macro 7", "Macro 8"
        };
        if (index >= 0 && index < 8)
            name = macroNames[index];
        else
            name = "Macro";
    }
    
    void setValue(float newValue)
    {
        value = juce::jlimit(0.0f, 1.0f, newValue);
    }
    
    float getValue() const { return value; }
    
    void setName(const juce::String& newName) { name = newName; }
    juce::String getName() const { return name; }
    
    void addAssignment(const MacroAssignment& assignment)
    {
        assignments.push_back(assignment);
    }
    
    void removeAssignment(int index)
    {
        if (index >= 0 && index < assignments.size())
            assignments.erase(assignments.begin() + index);
    }
    
    void clearAssignments()
    {
        assignments.clear();
    }
    
    const std::vector<MacroAssignment>& getAssignments() const
    {
        return assignments;
    }
    
    void applyToParameters(juce::AudioProcessorValueTreeState& apvts)
    {
        for (const auto& assignment : assignments)
        {
            if (assignment.type == MacroAssignment::TargetType::Parameter)
            {
                if (auto* param = apvts.getParameter(assignment.parameterID))
                {
                    float currentValue = param->getValue();
                    float modulation = value * assignment.amount;
                    float newValue = juce::jlimit(0.0f, 1.0f, currentValue + modulation);
                    param->setValueNotifyingHost(newValue);
                }
            }
        }
    }
    
    void applyToModulation(AdvancedModulationMatrix& modMatrix)
    {
        for (const auto& assignment : assignments)
        {
            if (assignment.type == MacroAssignment::TargetType::ModulationAmount)
            {
                float currentAmount = modMatrix.getConnectionAmount(assignment.modSource, assignment.modDest);
                float modulation = value * assignment.amount;
                float newAmount = juce::jlimit(-1.0f, 1.0f, currentAmount + modulation);
                modMatrix.addConnection(assignment.modSource, assignment.modDest, newAmount);
            }
        }
    }
    
private:
    int macroIndex;
    juce::String name;
    float value = 0.0f;
    std::vector<MacroAssignment> assignments;
};

/**
 * Macro System
 */
class MacroSystem
{
public:
    MacroSystem()
    {
        for (int i = 0; i < 8; ++i)
        {
            macros.push_back(std::make_unique<MacroControl>(i));
        }
    }
    
    MacroControl* getMacro(int index)
    {
        if (index >= 0 && index < macros.size())
            return macros[index].get();
        return nullptr;
    }
    
    void applyAll(juce::AudioProcessorValueTreeState& apvts, AdvancedModulationMatrix& modMatrix)
    {
        for (auto& macro : macros)
        {
            macro->applyToParameters(apvts);
            macro->applyToModulation(modMatrix);
        }
    }
    
    int getNumMacros() const { return macros.size(); }
    
private:
    std::vector<std::unique_ptr<MacroControl>> macros;
};
