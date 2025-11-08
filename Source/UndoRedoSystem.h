#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * Parameter Change Action
 */
struct ParameterChangeAction
{
    juce::String parameterID;
    float oldValue;
    float newValue;
    juce::Time timestamp;
    
    ParameterChangeAction(const juce::String& id, float oldVal, float newVal)
        : parameterID(id), oldValue(oldVal), newValue(newVal),
          timestamp(juce::Time::getCurrentTime()) {}
};

/**
 * Undo/Redo Manager
 */
class UndoRedoManager
{
public:
    UndoRedoManager(juce::AudioProcessorValueTreeState& apvts, int maxHistorySize = 100)
        : parameters(apvts), maxHistory(maxHistorySize)
    {
        attachToParameters();
    }
    
    void recordChange(const juce::String& parameterID, float oldValue, float newValue)
    {
        if (isPerformingUndo)
            return;
        
        // Group rapid changes (within 50ms) into single action
        if (!history.empty())
        {
            auto& lastAction = history[currentHistoryIndex];
            auto timeDiff = juce::Time::getCurrentTime() - lastAction.timestamp;
            
            if (timeDiff.inMilliseconds() < 50 && lastAction.parameterID == parameterID)
            {
                lastAction.newValue = newValue;
                lastAction.timestamp = juce::Time::getCurrentTime();
                return;
            }
        }
        
        // Clear any redo history
        if (currentHistoryIndex < history.size() - 1)
        {
            history.erase(history.begin() + currentHistoryIndex + 1, history.end());
        }
        
        // Add new action
        history.emplace_back(parameterID, oldValue, newValue);
        currentHistoryIndex = history.size() - 1;
        
        // Limit history size
        if (history.size() > maxHistory)
        {
            history.erase(history.begin());
            currentHistoryIndex--;
        }
    }
    
    bool canUndo() const
    {
        return currentHistoryIndex >= 0 && !history.empty();
    }
    
    bool canRedo() const
    {
        return currentHistoryIndex < static_cast<int>(history.size()) - 1;
    }
    
    void undo()
    {
        if (!canUndo())
            return;
        
        isPerformingUndo = true;
        
        const auto& action = history[currentHistoryIndex];
        if (auto* param = parameters.getParameter(action.parameterID))
        {
            param->setValueNotifyingHost(action.oldValue);
        }
        
        currentHistoryIndex--;
        isPerformingUndo = false;
    }
    
    void redo()
    {
        if (!canRedo())
            return;
        
        isPerformingUndo = true;
        
        currentHistoryIndex++;
        const auto& action = history[currentHistoryIndex];
        if (auto* param = parameters.getParameter(action.parameterID))
        {
            param->setValueNotifyingHost(action.newValue);
        }
        
        isPerformingUndo = false;
    }
    
    void clear()
    {
        history.clear();
        currentHistoryIndex = -1;
    }
    
    juce::String getUndoDescription() const
    {
        if (!canUndo())
            return "Nothing to undo";
        
        const auto& action = history[currentHistoryIndex];
        return "Undo " + getParameterName(action.parameterID);
    }
    
    juce::String getRedoDescription() const
    {
        if (!canRedo())
            return "Nothing to redo";
        
        const auto& action = history[currentHistoryIndex + 1];
        return "Redo " + getParameterName(action.parameterID);
    }
    
private:
    void attachToParameters()
    {
        for (auto* param : parameters.processor.getParameters())
        {
            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(param))
            {
                parameterValues[rangedParam->paramID] = rangedParam->getValue();
                
                rangedParam->addListener(this);
            }
        }
    }
    
    juce::String getParameterName(const juce::String& paramID) const
    {
        if (auto* param = parameters.getParameter(paramID))
        {
            return param->getName(50);
        }
        return paramID;
    }
    
    juce::AudioProcessorValueTreeState& parameters;
    std::vector<ParameterChangeAction> history;
    std::map<juce::String, float> parameterValues;
    int currentHistoryIndex = -1;
    int maxHistory;
    bool isPerformingUndo = false;
};

/**
 * Undo/Redo UI Component
 */
class UndoRedoControls : public juce::Component, private juce::Timer
{
public:
    UndoRedoControls(UndoRedoManager& manager) : undoRedoManager(manager)
    {
        undoButton.setButtonText("⟲");
        undoButton.onClick = [this] { undoRedoManager.undo(); };
        undoButton.setTooltip("Undo (Cmd+Z)");
        addAndMakeVisible(undoButton);
        
        redoButton.setButtonText("⟳");
        redoButton.onClick = [this] { undoRedoManager.redo(); };
        redoButton.setTooltip("Redo (Cmd+Shift+Z)");
        addAndMakeVisible(redoButton);
        
        startTimerHz(10);
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1a1a2e));
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(2);
        int buttonWidth = bounds.getWidth() / 2;
        
        undoButton.setBounds(bounds.removeFromLeft(buttonWidth).reduced(2));
        redoButton.setBounds(bounds.reduced(2));
    }
    
    bool keyPressed(const juce::KeyPress& key) override
    {
        // Cmd+Z / Ctrl+Z
        if (key.isKeyCode(juce::KeyPress::deleteKey) || 
            (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Z'))
        {
            if (key.getModifiers().isShiftDown())
                undoRedoManager.redo();
            else
                undoRedoManager.undo();
            return true;
        }
        
        return false;
    }
    
private:
    void timerCallback() override
    {
        undoButton.setEnabled(undoRedoManager.canUndo());
        redoButton.setEnabled(undoRedoManager.canRedo());
        
        undoButton.setTooltip(undoRedoManager.getUndoDescription());
        redoButton.setTooltip(undoRedoManager.getRedoDescription());
    }
    
    UndoRedoManager& undoRedoManager;
    juce::TextButton undoButton, redoButton;
};
