#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>
#include "MacroSystem.h"

class MacroKnob : public juce::Component
{
public:
    MacroKnob(MacroControl& macro, int /* index */)
        : macroControl(macro)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRange(0.0, 1.0, 0.01);
        slider.setValue(0.0);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.onValueChange = [this] { 
            macroControl.setValue(slider.getValue());
        };
        addAndMakeVisible(slider);
        
        nameLabel.setText(macro.getName(), juce::dontSendNotification);
        nameLabel.setJustificationType(juce::Justification::centred);
        nameLabel.setEditable(true);
        nameLabel.onTextChange = [this] {
            macroControl.setName(nameLabel.getText());
        };
        addAndMakeVisible(nameLabel);
        
        valueLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(valueLabel);
        
        assignButton.setButtonText("•••");
        // Pastel colors instead of black
        assignButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd8b5ff));
        assignButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff6b4f9e));
        addAndMakeVisible(assignButton);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.fillAll(juce::Colour(0xffe8dcff));
        g.setColour(juce::Colour(0xffd8b5ff));
        g.drawRect(bounds, 1.0f);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(5);
        
        nameLabel.setBounds(bounds.removeFromTop(20));
        assignButton.setBounds(bounds.removeFromBottom(25));
        valueLabel.setBounds(bounds.removeFromBottom(18));
        slider.setBounds(bounds);
    }
    
    std::function<void()> onAssignClick;
    
private:
    void timerCallback()
    {
        float val = macroControl.getValue();
        // REAL-TIME SAFE: Use pre-formatted string to avoid dynamic string operations
        static char buffer[8];
        int percentage = static_cast<int>(val * 100);
        std::snprintf(buffer, sizeof(buffer), "%d%%", percentage);
        valueLabel.setText(juce::String(buffer), juce::dontSendNotification);
    }
    
    MacroControl& macroControl;
    // int macroIndex; // Unused field

    juce::Slider slider;
    juce::Label nameLabel;
    juce::Label valueLabel;
    juce::TextButton assignButton;
};

class MacroPanel : public juce::Component
{
public:
    MacroPanel(MacroSystem& system) : macroSystem(system)
    {
        for (int i = 0; i < 8; ++i)
        {
            auto* knob = new MacroKnob(*macroSystem.getMacro(i), i);
            knob->onAssignClick = [this, i] { showAssignDialog(i); };
            macroKnobs.add(knob);
            addAndMakeVisible(knob);
        }
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xfff5f0ff));

        auto titleArea = getLocalBounds().removeFromTop(40).toFloat();
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(0xffe8dcff), 0, 0,
            juce::Colour(0xffd8b5ff), 0, titleArea.getBottom(), false));
        g.fillRect(titleArea);

        g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
        g.setColour(juce::Colour(0xff6b4f9e));
        g.drawText("MACRO CONTROLS", titleArea.reduced(10), juce::Justification::centredLeft);

        g.setColour(juce::Colour(0xffd8b5ff));
        g.drawRect(getLocalBounds(), 2);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        bounds.removeFromTop(40);

        int knobWidth = bounds.getWidth() / 8;
        for (auto* knob : macroKnobs)
        {
            knob->setBounds(bounds.removeFromLeft(knobWidth).reduced(3));
        }
    }
    
private:
    void showAssignDialog(int macroIndex)
    {
        // Assignment dialog implementation
    }
    
    MacroSystem& macroSystem;
    juce::OwnedArray<MacroKnob> macroKnobs;
};
