#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "LFO.h"

/**
 * LFO Control Panel
 * Complete UI for a single LFO with all controls
 */
class LFOPanel : public juce::Component
{
public:
    LFOPanel(LFO& lfoRef, const juce::String& lfoName, juce::AudioProcessorValueTreeState& /* apvts */, int /* lfoIndex */)
        : lfo(lfoRef), name(lfoName)
    {
        // Create shape selector
        shapeSelector.addItem("Sine", 1);
        shapeSelector.addItem("Triangle", 2);
        shapeSelector.addItem("Saw", 3);
        shapeSelector.addItem("Square", 4);
        shapeSelector.addItem("Random", 5);
        shapeSelector.addItem("S&H", 6);
        shapeSelector.setSelectedId(1);
        shapeSelector.onChange = [this] { updateLFOShape(); };
        addAndMakeVisible(shapeSelector);
        
        // Create rate slider
        rateSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        rateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        rateSlider.setRange(0.01, 50.0, 0.01);
        rateSlider.setValue(1.0);
        rateSlider.setTextValueSuffix(" Hz");
        rateSlider.onValueChange = [this] { lfo.setRate(rateSlider.getValue()); };
        addAndMakeVisible(rateSlider);
        
        rateLabel.setText("Rate", juce::dontSendNotification);
        rateLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(rateLabel);
        
        // Create depth slider
        depthSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        depthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        depthSlider.setRange(0.0, 1.0, 0.01);
        depthSlider.setValue(1.0);
        depthSlider.setTextValueSuffix(" %");
        depthSlider.onValueChange = [this] { lfo.setDepth(depthSlider.getValue()); };
        addAndMakeVisible(depthSlider);
        
        depthLabel.setText("Depth", juce::dontSendNotification);
        depthLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(depthLabel);
        
        // Create phase slider
        phaseSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        phaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        phaseSlider.setRange(0.0, 1.0, 0.01);
        phaseSlider.setValue(0.0);
        phaseSlider.setTextValueSuffix(" °");
        phaseSlider.onValueChange = [this] { lfo.setPhaseOffset(phaseSlider.getValue()); };
        addAndMakeVisible(phaseSlider);
        
        phaseLabel.setText("Phase", juce::dontSendNotification);
        phaseLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(phaseLabel);
        
        // Create retrigger button
        retriggerButton.setButtonText("Retrig");
        retriggerButton.setToggleable(true);
        retriggerButton.onClick = [this] { lfo.setRetrigger(retriggerButton.getToggleState()); };
        addAndMakeVisible(retriggerButton);
        
        // Create bipolar button
        bipolarButton.setButtonText("±");
        bipolarButton.setToggleable(true);
        bipolarButton.setToggleState(true, juce::dontSendNotification);
        bipolarButton.onClick = [this] { lfo.setBipolar(bipolarButton.getToggleState()); };
        addAndMakeVisible(bipolarButton);
        
        // Create LFO display
        lfoDisplay = std::make_unique<LFODisplay>(lfo);
        addAndMakeVisible(lfoDisplay.get());
        
        // Style everything
        setupStyling();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        // Background with pastel purple gradient
        juce::ColourGradient gradient(
            juce::Colour(0xfff0e0ff).withAlpha(0.3f), bounds.getX(), bounds.getY(),
            juce::Colour(0xffe8d5ff).withAlpha(0.4f), bounds.getX(), bounds.getBottom(),
            false
        );
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

        // Border - pastel pink
        g.setColour(juce::Colour(0xffffb3d9));
        g.drawRoundedRectangle(bounds.toFloat().reduced(1.0f), 8.0f, 2.0f);

        // Title - pastel pink
        g.setColour(juce::Colour(0xffffb3d9));
        g.setFont(juce::Font("Helvetica Neue", 16.0f, juce::Font::bold));
        g.drawText(name, bounds.removeFromTop(30).reduced(10, 5), juce::Justification::centredLeft);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        bounds.removeFromTop(30); // Title area
        
        // LFO Display at top
        auto displayArea = bounds.removeFromTop(80);
        lfoDisplay->setBounds(displayArea.reduced(5));
        
        bounds.removeFromTop(10); // Spacing
        
        // Shape selector
        auto shapeArea = bounds.removeFromTop(30);
        shapeSelector.setBounds(shapeArea.reduced(5));
        
        bounds.removeFromTop(10); // Spacing
        
        // Controls in row
        auto controlsArea = bounds.removeFromTop(100);
        int controlWidth = controlsArea.getWidth() / 3;
        
        // Rate
        auto rateArea = controlsArea.removeFromLeft(controlWidth).reduced(5);
        rateLabel.setBounds(rateArea.removeFromTop(20));
        rateSlider.setBounds(rateArea);
        
        // Depth
        auto depthArea = controlsArea.removeFromLeft(controlWidth).reduced(5);
        depthLabel.setBounds(depthArea.removeFromTop(20));
        depthSlider.setBounds(depthArea);
        
        // Phase
        auto phaseArea = controlsArea.removeFromLeft(controlWidth).reduced(5);
        phaseLabel.setBounds(phaseArea.removeFromTop(20));
        phaseSlider.setBounds(phaseArea);
        
        // Buttons at bottom
        auto buttonArea = bounds.removeFromTop(30);
        int buttonWidth = buttonArea.getWidth() / 2;
        retriggerButton.setBounds(buttonArea.removeFromLeft(buttonWidth).reduced(5));
        bipolarButton.setBounds(buttonArea.removeFromLeft(buttonWidth).reduced(5));
    }
    
private:
    void updateLFOShape()
    {
        int selectedId = shapeSelector.getSelectedId();
        LFO::Shape shape = static_cast<LFO::Shape>(selectedId - 1);
        lfo.setShape(shape);
    }
    
    void setupStyling()
    {
        // Shape selector styling - pastel theme
        shapeSelector.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xffc8a0ff));
        shapeSelector.setColour(juce::ComboBox::textColourId, juce::Colour(0xffffffff));
        shapeSelector.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xffffb3d9));
        shapeSelector.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffffb3d9));

        // Slider styling - pastel theme
        auto setupSlider = [](juce::Slider& slider)
        {
            slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffa8ffb4)); // Green
            slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xffd8b5ff)); // Purple
            slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffffb3d9)); // Pink
            slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffffffff));
            slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xffc8a0ff));
            slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xffffb3d9));
        };

        setupSlider(rateSlider);
        setupSlider(depthSlider);
        setupSlider(phaseSlider);

        // Label styling
        auto setupLabel = [](juce::Label& label)
        {
            label.setColour(juce::Label::textColourId, juce::Colour(0xffffffff));
            label.setFont(juce::Font("Helvetica Neue", 12.0f, juce::Font::bold));
        };

        setupLabel(rateLabel);
        setupLabel(depthLabel);
        setupLabel(phaseLabel);

        // Button styling - pastel theme
        auto setupButton = [](juce::TextButton& button)
        {
            button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd8b5ff)); // Purple
            button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffa8ffb4)); // Green when on
            button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));
            button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff000000));
        };

        setupButton(retriggerButton);
        setupButton(bipolarButton);
    }
    
    LFO& lfo;
    juce::String name;
    // juce::AudioProcessorValueTreeState& parameters; // Unused field
    // int index; // Unused field
    
    juce::ComboBox shapeSelector;
    juce::Slider rateSlider, depthSlider, phaseSlider;
    juce::Label rateLabel, depthLabel, phaseLabel;
    juce::TextButton retriggerButton, bipolarButton;
    std::unique_ptr<LFODisplay> lfoDisplay;
};

/**
 * Complete LFO Section with all 3 LFOs
 */
class LFOSection : public juce::Component
{
public:
    LFOSection(juce::AudioProcessorValueTreeState& apvts)
        : parameters(apvts)
    {
        // Create 3 LFOs
        lfo1 = std::make_unique<LFO>();
        lfo2 = std::make_unique<LFO>();
        lfo3 = std::make_unique<LFO>();
        
        // Create panels for each LFO
        lfo1Panel = std::make_unique<LFOPanel>(*lfo1, "LFO 1", apvts, 0);
        lfo2Panel = std::make_unique<LFOPanel>(*lfo2, "LFO 2", apvts, 1);
        lfo3Panel = std::make_unique<LFOPanel>(*lfo3, "LFO 3", apvts, 2);
        
        addAndMakeVisible(lfo1Panel.get());
        addAndMakeVisible(lfo2Panel.get());
        addAndMakeVisible(lfo3Panel.get());
    }
    
    void prepare(double sampleRate)
    {
        lfo1->prepare(sampleRate);
        lfo2->prepare(sampleRate);
        lfo3->prepare(sampleRate);
    }
    
    void reset()
    {
        lfo1->reset();
        lfo2->reset();
        lfo3->reset();
    }
    
    void processBlock(int numSamples)
    {
        // Generate LFO samples
        for (int i = 0; i < numSamples; ++i)
        {
            lfo1->getNextSample();
            lfo2->getNextSample();
            lfo3->getNextSample();
        }
    }
    
    LFO* getLFO(int index)
    {
        switch (index)
        {
            case 0: return lfo1.get();
            case 1: return lfo2.get();
            case 2: return lfo3.get();
            default: return nullptr;
        }
    }
    
    void paint(juce::Graphics& g) override
    {
        // Pastel purple background gradient to match main UI
        juce::ColourGradient gradient(
            juce::Colour(0xffe8d5ff), 0, 0,
            juce::Colour(0xffc8a0ff), 0, getHeight(), false
        );
        g.setGradientFill(gradient);
        g.fillAll();
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        
        // Stack LFOs vertically
        int panelHeight = (bounds.getHeight() - 20) / 3; // 3 LFOs with spacing
        
        lfo1Panel->setBounds(bounds.removeFromTop(panelHeight));
        bounds.removeFromTop(10); // Spacing
        
        lfo2Panel->setBounds(bounds.removeFromTop(panelHeight));
        bounds.removeFromTop(10); // Spacing
        
        lfo3Panel->setBounds(bounds);
    }
    
private:
    juce::AudioProcessorValueTreeState& parameters;
    
    std::unique_ptr<LFO> lfo1, lfo2, lfo3;
    std::unique_ptr<LFOPanel> lfo1Panel, lfo2Panel, lfo3Panel;
};
