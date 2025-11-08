#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>
#include "GrainVisualizer.h"

/**
 * Visual Feedback Panel
 * Combines Grain Visualizer and Spectral Analyzer with tab switching
 */
class VisualFeedbackPanel : public juce::Component
{
public:
    enum class DisplayMode
    {
        Grains,
        Spectrum,
        Both
    };
    
    VisualFeedbackPanel()
    {
        // Create visualizers
        grainVisualizer = std::make_unique<GrainVisualizer>();
        spectralAnalyzer = std::make_unique<SpectralAnalyzer>();
        
        addAndMakeVisible(grainVisualizer.get());
        addAndMakeVisible(spectralAnalyzer.get());
        
        // Mode selector buttons
        grainsButton.setButtonText("Grains");
        grainsButton.setToggleable(true);
        grainsButton.setRadioGroupId(1);
        grainsButton.setToggleState(true, juce::dontSendNotification);
        grainsButton.onClick = [this] { setDisplayMode(DisplayMode::Grains); };
        addAndMakeVisible(grainsButton);
        
        spectrumButton.setButtonText("Spectrum");
        spectrumButton.setToggleable(true);
        spectrumButton.setRadioGroupId(1);
        spectrumButton.onClick = [this] { setDisplayMode(DisplayMode::Spectrum); };
        addAndMakeVisible(spectrumButton);
        
        bothButton.setButtonText("Both");
        bothButton.setToggleable(true);
        bothButton.setRadioGroupId(1);
        bothButton.onClick = [this] { setDisplayMode(DisplayMode::Both); };
        addAndMakeVisible(bothButton);
        
        setupStyling();
        setDisplayMode(DisplayMode::Grains);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        // Background (soft pastel purple)
        g.fillAll(juce::Colour(0xfff5f0ff));

        // Title bar
        auto titleArea = bounds.removeFromTop(50).toFloat();
        juce::ColourGradient gradient(
            juce::Colour(0xffe8dcff), titleArea.getX(), titleArea.getY(),
            juce::Colour(0xffd8b5ff), titleArea.getX(), titleArea.getBottom(),
            false
        );
        g.setGradientFill(gradient);
        g.fillRect(titleArea);

        // Title text (dark purple for contrast)
        g.setColour(juce::Colour(0xff6b4f9e));
        g.setFont(juce::Font("Courier New", 20.0f, juce::Font::bold));
        g.drawText("VISUAL FEEDBACK", titleArea.reduced(15.0f).toNearestInt(),
                  juce::Justification::centredLeft);

        // Border (pastel purple)
        g.setColour(juce::Colour(0xffd8b5ff));
        g.drawRect(getLocalBounds(), 2);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        
        // Title and buttons area
        auto titleArea = bounds.removeFromTop(40);
        titleArea.removeFromLeft(200); // Space for title
        
        int buttonWidth = 90;
        titleArea.removeFromRight(10);
        bothButton.setBounds(titleArea.removeFromRight(buttonWidth));
        titleArea.removeFromRight(5);
        spectrumButton.setBounds(titleArea.removeFromRight(buttonWidth));
        titleArea.removeFromRight(5);
        grainsButton.setBounds(titleArea.removeFromRight(buttonWidth));
        
        bounds.removeFromTop(10);
        
        // Layout based on mode
        switch (currentMode)
        {
            case DisplayMode::Grains:
                grainVisualizer->setBounds(bounds);
                break;
                
            case DisplayMode::Spectrum:
                spectralAnalyzer->setBounds(bounds);
                break;
                
            case DisplayMode::Both:
            {
                int height = bounds.getHeight() / 2;
                grainVisualizer->setBounds(bounds.removeFromTop(height - 5));
                bounds.removeFromTop(10);
                spectralAnalyzer->setBounds(bounds);
                break;
            }
        }
    }
    
    void setDisplayMode(DisplayMode mode)
    {
        currentMode = mode;
        
        grainVisualizer->setVisible(mode == DisplayMode::Grains || mode == DisplayMode::Both);
        spectralAnalyzer->setVisible(mode == DisplayMode::Spectrum || mode == DisplayMode::Both);
        
        resized();
    }
    
    // Forward grain spawning to visualizer
    void spawnGrain(float position, float amplitude, float pitch, float size)
    {
        grainVisualizer->spawnGrain(position, amplitude, pitch, size);
    }
    
    void updateGrainParameters(float density, float grainSize, float position, float texture)
    {
        grainVisualizer->updateParameters(density, grainSize, position, texture);
    }
    
    // Forward audio samples to spectrum analyzer
    void pushSamplesForSpectrum(const float* samples, int numSamples)
    {
        spectralAnalyzer->pushSamples(samples, numSamples);
    }
    
    void prepare(double sampleRate)
    {
        spectralAnalyzer->prepare(sampleRate);
    }
    
    GrainVisualizer* getGrainVisualizer() { return grainVisualizer.get(); }
    SpectralAnalyzer* getSpectralAnalyzer() { return spectralAnalyzer.get(); }
    
private:
    void setupStyling()
    {
        auto setupButton = [](juce::TextButton& button)
        {
            button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffe8dcff));
            button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd8b5ff));
            button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff6b4f9e));
            button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff4a3368));
        };

        setupButton(grainsButton);
        setupButton(spectrumButton);
        setupButton(bothButton);
    }
    
    std::unique_ptr<GrainVisualizer> grainVisualizer;
    std::unique_ptr<SpectralAnalyzer> spectralAnalyzer;
    
    juce::TextButton grainsButton, spectrumButton, bothButton;
    DisplayMode currentMode = DisplayMode::Grains;
};
