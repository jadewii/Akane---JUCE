#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ModulationMatrix.h"

/**
 * Modulation Slot Component
 * Single row showing one modulation routing
 */
class ModulationSlot : public juce::Component
{
public:
    ModulationSlot(AdvancedModulationMatrix& matrix, 
                   ModulationSource::Type src,
                   ModulationDestination::Type dest,
                   float amt)
        : modMatrix(matrix), source(src), destination(dest), amount(amt)
    {
        // Source label
        sourceLabel.setText(getSourceName(src), juce::dontSendNotification);
        sourceLabel.setColour(juce::Label::textColourId, ModulationConnection::getSourceColor(src));
        sourceLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(sourceLabel);

        // Arrow - pastel pink
        arrowLabel.setText("→", juce::dontSendNotification);
        arrowLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffb3d9));
        arrowLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(arrowLabel);
        
        // Destination label
        destLabel.setText(getDestinationName(dest), juce::dontSendNotification);
        destLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffffff));
        destLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(destLabel);
        
        // Amount slider
        amountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        amountSlider.setRange(-1.0, 1.0, 0.01);
        amountSlider.setValue(amt);
        amountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        amountSlider.setTextValueSuffix("%");
        amountSlider.onValueChange = [this] {
            amount = amountSlider.getValue();
            modMatrix.addConnection(source, destination, amount);
        };
        addAndMakeVisible(amountSlider);
        
        // Remove button
        removeButton.setButtonText("×");
        removeButton.onClick = [this] {
            modMatrix.removeConnection(source, destination);
            if (onRemove)
                onRemove();
        };
        addAndMakeVisible(removeButton);
        
        setupStyling();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Background with source color
        auto bgColor = ModulationConnection::getSourceColor(source).withAlpha(0.1f);
        g.setColour(bgColor);
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Border
        g.setColour(ModulationConnection::getSourceColor(source).withAlpha(0.3f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(5);
        
        sourceLabel.setBounds(bounds.removeFromLeft(100));
        arrowLabel.setBounds(bounds.removeFromLeft(30));
        destLabel.setBounds(bounds.removeFromLeft(150));
        removeButton.setBounds(bounds.removeFromRight(30));
        amountSlider.setBounds(bounds);
    }
    
    std::function<void()> onRemove;
    
private:
    juce::String getSourceName(ModulationSource::Type src)
    {
        switch (src)
        {
            case ModulationSource::Type::LFO1: return "LFO 1";
            case ModulationSource::Type::LFO2: return "LFO 2";
            case ModulationSource::Type::LFO3: return "LFO 3";
            case ModulationSource::Type::Envelope1: return "ENV 1";
            case ModulationSource::Type::Envelope2: return "ENV 2";
            case ModulationSource::Type::Velocity: return "Velocity";
            case ModulationSource::Type::Aftertouch: return "Aftertouch";
            case ModulationSource::Type::ModWheel: return "Mod Wheel";
            case ModulationSource::Type::PitchBend: return "Pitch Bend";
            case ModulationSource::Type::Random: return "Random";
            default: return "Unknown";
        }
    }
    
    juce::String getDestinationName(ModulationDestination::Type dest)
    {
        switch (dest)
        {
            case ModulationDestination::Type::FilterCutoff: return "Filter Cutoff";
            case ModulationDestination::Type::FilterResonance: return "Filter Resonance";
            case ModulationDestination::Type::GrainDensity: return "Grain Density";
            case ModulationDestination::Type::GrainSize: return "Grain Size";
            case ModulationDestination::Type::CloudsTexture: return "Clouds Texture";
            case ModulationDestination::Type::RingsStructure: return "Rings Structure";
            case ModulationDestination::Type::RingsBrightness: return "Rings Brightness";
            case ModulationDestination::Type::WavetablePosition: return "Wavetable Pos";
            case ModulationDestination::Type::OscillatorPitch: return "Osc Pitch";
            case ModulationDestination::Type::DelayTime: return "Delay Time";
            case ModulationDestination::Type::ReverbSize: return "Reverb Size";
            case ModulationDestination::Type::Volume: return "Volume";
            case ModulationDestination::Type::Pan: return "Pan";
            default: return "Unknown";
        }
    }
    
    void setupStyling()
    {
        sourceLabel.setFont(juce::Font("Helvetica Neue", 12.0f, juce::Font::bold));
        arrowLabel.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
        destLabel.setFont(juce::Font("Helvetica Neue", 12.0f, juce::Font::bold));

        amountSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xffd8b5ff)); // Pastel purple
        amountSlider.setColour(juce::Slider::thumbColourId, ModulationConnection::getSourceColor(source));
        amountSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffffffff));
        amountSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xffc8a0ff));

        removeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffffb3d9).withAlpha(0.5f)); // Pastel pink
        removeButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));
    }
    
    AdvancedModulationMatrix& modMatrix;
    ModulationSource::Type source;
    ModulationDestination::Type destination;
    float amount;
    
    juce::Label sourceLabel, arrowLabel, destLabel;
    juce::Slider amountSlider;
    juce::TextButton removeButton;
};

/**
 * Add Modulation Dialog
 * Allows user to create new modulation routings
 */
class AddModulationDialog : public juce::Component
{
public:
    AddModulationDialog(AdvancedModulationMatrix& matrix) : modMatrix(matrix)
    {
        titleLabel.setText("Add Modulation", juce::dontSendNotification);
        titleLabel.setFont(juce::Font("Helvetica Neue", 18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffb3d9)); // Pastel pink
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);
        
        // Source selector
        sourceLabel.setText("Source:", juce::dontSendNotification);
        sourceLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffffff));
        addAndMakeVisible(sourceLabel);
        
        for (const auto& source : modMatrix.getSources())
        {
            sourceSelector.addItem(source.name, static_cast<int>(source.type) + 1);
        }
        sourceSelector.setSelectedId(1);
        addAndMakeVisible(sourceSelector);
        
        // Destination selector
        destLabel.setText("Destination:", juce::dontSendNotification);
        destLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffffff));
        addAndMakeVisible(destLabel);
        
        // Group destinations by category
        juce::StringArray categories;
        for (const auto& dest : modMatrix.getDestinations())
        {
            if (!categories.contains(dest.category))
                categories.add(dest.category);
        }
        
        int itemId = 1;
        for (const auto& category : categories)
        {
            destSelector.addSeparator();
            // Category header (non-selectable in JUCE 7+)
            destSelector.addItem(category, -1);

            for (const auto& dest : modMatrix.getDestinations())
            {
                if (dest.category == category)
                {
                    destSelector.addItem("  " + dest.name, itemId);
                    destinationMap[itemId] = dest.type;
                    itemId++;
                }
            }
        }
        destSelector.setSelectedId(1);
        addAndMakeVisible(destSelector);
        
        // Amount slider
        amountLabel.setText("Amount:", juce::dontSendNotification);
        amountLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffffff));
        addAndMakeVisible(amountLabel);
        
        amountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        amountSlider.setRange(-1.0, 1.0, 0.01);
        amountSlider.setValue(0.5);
        amountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
        amountSlider.setTextValueSuffix("%");
        addAndMakeVisible(amountSlider);
        
        // Add button
        addButton.setButtonText("Add Connection");
        addButton.onClick = [this] {
            auto sourceType = static_cast<ModulationSource::Type>(sourceSelector.getSelectedId() - 1);
            auto destType = destinationMap[destSelector.getSelectedId()];
            modMatrix.addConnection(sourceType, destType, amountSlider.getValue());
            
            if (onConnectionAdded)
                onConnectionAdded();
        };
        addAndMakeVisible(addButton);
        
        // Cancel button
        cancelButton.setButtonText("Cancel");
        cancelButton.onClick = [this] {
            if (onCancel)
                onCancel();
        };
        addAndMakeVisible(cancelButton);
        
        setupStyling();
        setSize(400, 300);
    }
    
    void paint(juce::Graphics& g) override
    {
        // Background - pastel purple gradient
        juce::ColourGradient gradient(
            juce::Colour(0xfff0e0ff), 0, 0,
            juce::Colour(0xffe8d5ff), 0, getHeight(), false
        );
        g.setGradientFill(gradient);
        g.fillAll();

        // Border - pastel pink
        g.setColour(juce::Colour(0xffffb3d9));
        g.drawRect(getLocalBounds(), 2);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(20);
        
        titleLabel.setBounds(bounds.removeFromTop(30));
        bounds.removeFromTop(20);
        
        sourceLabel.setBounds(bounds.removeFromTop(25));
        sourceSelector.setBounds(bounds.removeFromTop(30));
        bounds.removeFromTop(15);
        
        destLabel.setBounds(bounds.removeFromTop(25));
        destSelector.setBounds(bounds.removeFromTop(30));
        bounds.removeFromTop(15);
        
        amountLabel.setBounds(bounds.removeFromTop(25));
        amountSlider.setBounds(bounds.removeFromTop(30));
        bounds.removeFromTop(20);
        
        auto buttonArea = bounds.removeFromTop(40);
        int buttonWidth = buttonArea.getWidth() / 2 - 5;
        addButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
        buttonArea.removeFromLeft(10);
        cancelButton.setBounds(buttonArea);
    }
    
    std::function<void()> onConnectionAdded;
    std::function<void()> onCancel;
    
private:
    void setupStyling()
    {
        auto setupCombo = [](juce::ComboBox& combo)
        {
            combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xffc8a0ff)); // Pastel purple
            combo.setColour(juce::ComboBox::textColourId, juce::Colour(0xffffffff));
            combo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xffffb3d9)); // Pastel pink
            combo.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffffb3d9)); // Pastel pink
        };

        setupCombo(sourceSelector);
        setupCombo(destSelector);

        amountSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xffd8b5ff)); // Pastel purple
        amountSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffa8ffb4)); // Pastel green
        amountSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffffffff));
        amountSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xffc8a0ff));

        addButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffa8ffb4)); // Pastel green
        addButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff000000));

        cancelButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd8b5ff)); // Pastel purple
        cancelButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));
    }

    AdvancedModulationMatrix& modMatrix;
    std::map<int, ModulationDestination::Type> destinationMap;
    
    juce::Label titleLabel, sourceLabel, destLabel, amountLabel;
    juce::ComboBox sourceSelector, destSelector;
    juce::Slider amountSlider;
    juce::TextButton addButton, cancelButton;
};

/**
 * Modulation Matrix View
 * Complete visual modulation routing interface
 */
class ModulationMatrixView : public juce::Component
{
public:
    ModulationMatrixView(AdvancedModulationMatrix& matrix) : modMatrix(matrix)
    {
        // Title
        titleLabel.setText("MODULATION MATRIX", juce::dontSendNotification);
        titleLabel.setFont(juce::Font("Helvetica Neue", 20.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffb3d9)); // Pastel pink
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);
        
        // Add button
        addButton.setButtonText("+ Add Modulation");
        addButton.onClick = [this] { showAddDialog(); };
        addAndMakeVisible(addButton);
        
        // Clear button
        clearButton.setButtonText("Clear All");
        clearButton.onClick = [this] {
            modMatrix.clearAllConnections();
            updateConnectionList();
        };
        addAndMakeVisible(clearButton);
        
        // Viewport for connections
        viewport.setViewedComponent(&connectionContainer, false);
        addAndMakeVisible(viewport);
        
        setupStyling();
        updateConnectionList();
    }
    
    void paint(juce::Graphics& g) override
    {
        // Background - pastel purple gradient
        juce::ColourGradient bgGradient(
            juce::Colour(0xffe8d5ff), 0, 0,
            juce::Colour(0xffc8a0ff), 0, getHeight(), false
        );
        g.setGradientFill(bgGradient);
        g.fillAll();

        // Title area background - lighter gradient
        auto titleArea = getLocalBounds().removeFromTop(60).toFloat();
        juce::ColourGradient gradient(
            juce::Colour(0xfff0e0ff).withAlpha(0.5f), titleArea.getX(), titleArea.getY(),
            juce::Colour(0xffe8d5ff).withAlpha(0.6f), titleArea.getX(), titleArea.getBottom(),
            false
        );
        g.setGradientFill(gradient);
        g.fillRect(titleArea);

        // Border - pastel pink
        g.setColour(juce::Colour(0xffffb3d9));
        g.drawRect(getLocalBounds(), 2);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        
        // Title area
        auto titleArea = bounds.removeFromTop(50);
        titleLabel.setBounds(titleArea.removeFromLeft(titleArea.getWidth() - 240));
        titleArea.removeFromLeft(10);
        clearButton.setBounds(titleArea.removeFromLeft(110));
        titleArea.removeFromLeft(10);
        addButton.setBounds(titleArea);
        
        bounds.removeFromTop(10);
        
        // Viewport
        viewport.setBounds(bounds);
        
        // Update connection container height
        int totalHeight = connectionSlots.size() * 50 + 10;
        connectionContainer.setSize(bounds.getWidth() - 20, totalHeight);
        
        // Layout connection slots
        auto slotBounds = connectionContainer.getLocalBounds().reduced(5);
        for (auto* slot : connectionSlots)
        {
            slot->setBounds(slotBounds.removeFromTop(45));
            slotBounds.removeFromTop(5);
        }
    }
    
    void updateConnectionList()
    {
        // Clear existing slots
        connectionSlots.clear();
        connectionContainer.removeAllChildren();
        
        // Create slots for each connection
        for (const auto& conn : modMatrix.getConnections())
        {
            auto* slot = new ModulationSlot(modMatrix, conn.source, conn.destination, conn.amount);
            slot->onRemove = [this] { updateConnectionList(); };
            connectionSlots.add(slot);
            connectionContainer.addAndMakeVisible(slot);
        }
        
        resized();
        repaint();
    }
    
private:
    void showAddDialog()
    {
        auto* dialog = new AddModulationDialog(modMatrix);
        dialog->onConnectionAdded = [this, dialog] {
            updateConnectionList();
            delete dialog;
        };
        dialog->onCancel = [dialog] {
            delete dialog;
        };
        
        dialog->setBounds(getWidth() / 2 - 200, getHeight() / 2 - 150, 400, 300);
        addAndMakeVisible(dialog);
        dialog->toFront(true);
    }
    
    void setupStyling()
    {
        addButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffa8ffb4)); // Pastel green
        addButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff000000));

        clearButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffffb3d9)); // Pastel pink
        clearButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));

        viewport.setScrollBarsShown(true, false);
    }

    AdvancedModulationMatrix& modMatrix;

    juce::Label titleLabel;
    juce::TextButton addButton, clearButton;
    juce::Viewport viewport;
    juce::Component connectionContainer;
    juce::OwnedArray<ModulationSlot> connectionSlots;
};
