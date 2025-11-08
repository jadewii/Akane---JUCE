#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * Enhanced Tooltip Window
 */
class EnhancedTooltipWindow : public juce::TooltipWindow
{
public:
    EnhancedTooltipWindow(juce::Component* parent = nullptr, int delayMs = 700)
        : juce::TooltipWindow(parent, delayMs)
    {
        setOpaque(false);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Shadow
        juce::DropShadow shadow(juce::Colours::black.withAlpha(0.5f), 8, {0, 2});
        shadow.drawForRectangle(g, bounds.toNearestInt());
        
        // Background (pastel purple)
        g.setColour(juce::Colour(0xfff0e0ff));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Border (pastel pink)
        g.setColour(juce::Colour(0xffffb3d9));
        g.drawRoundedRectangle(bounds.reduced(1), 4.0f, 1.5f);

        // Text (dark for readability)
        g.setColour(juce::Colour(0xff333333));
        g.setFont(juce::FontOptions(13.0f));
        g.drawText(getTipFor(*getParentComponent()),
                   bounds.reduced(8, 4),
                   juce::Justification::centredLeft);
    }
};

/**
 * Tooltip Manager - Sets up tooltips for all parameters
 */
class TooltipManager
{
public:
    static void setupTooltips(juce::Component& rootComponent, 
                              juce::AudioProcessorValueTreeState& apvts)
    {
        // Parameter tooltips
        tooltips["filterCutoff"] = "Filter Cutoff: Adjust the filter frequency\nRange: 20Hz - 20kHz";
        tooltips["filterResonance"] = "Filter Resonance: Control filter peak emphasis\nRange: 0% - 100%";
        
        tooltips["grainDensity"] = "Grain Density: Number of simultaneous grains\nHigher = more texture";
        tooltips["grainSize"] = "Grain Size: Length of each grain\nSmaller = granular, Larger = smoother";
        tooltips["grainPosition"] = "Grain Position: Playback position in sample\n0% = start, 100% = end";
        
        tooltips["cloudsTexture"] = "Clouds Texture: Granular texture character\nLow = clean, High = chaotic";
        tooltips["cloudsBlend"] = "Clouds Blend: Mix between dry and processed\n0% = dry, 100% = wet";
        
        tooltips["ringsStructure"] = "Rings Structure: Resonator structure\nChanges harmonic content";
        tooltips["ringsBrightness"] = "Rings Brightness: High frequency damping\nLower = darker, Higher = brighter";
        tooltips["ringsDamping"] = "Rings Damping: How quickly resonance decays\nLower = sustains longer";
        
        tooltips["wavetablePosition"] = "Wavetable Position: Navigate through wavetable\n0% = wave A, 100% = wave B";
        tooltips["wavetableMorph"] = "Wavetable Morph: Blend between wavetables\nSmooth morphing between positions";
        
        tooltips["lfo1Rate"] = "LFO 1 Rate: Speed of modulation\nRange: 0.01Hz - 50Hz";
        tooltips["lfo1Depth"] = "LFO 1 Depth: Amount of modulation\n0% = off, 100% = full range";
        tooltips["lfo1Shape"] = "LFO 1 Shape: Waveform type\nSine, Triangle, Saw, Square, Random, S&H";
        
        tooltips["distortionDrive"] = "Distortion Drive: Amount of saturation\nHigher = more harmonics";
        tooltips["distortionMix"] = "Distortion Mix: Blend dry/wet signal\n0% = clean, 100% = full distortion";
        
        tooltips["delayTime"] = "Delay Time: Delay duration\nRange: 1ms - 2000ms";
        tooltips["delayFeedback"] = "Delay Feedback: Number of repeats\n0% = single echo, 95% = infinite";
        tooltips["delayFilter"] = "Delay Filter: High-cut on feedback\nLower = darker repeats";
        
        tooltips["reverbSize"] = "Reverb Size: Virtual room size\nLarger = longer decay";
        tooltips["reverbDamping"] = "Reverb Damping: High frequency absorption\nHigher = darker reverb";
        tooltips["reverbShimmer"] = "Reverb Shimmer: Octave-up feedback\nAdds ethereal character";
        
        // Apply tooltips to components
        applyTooltipsRecursive(rootComponent);
    }
    
private:
    static void applyTooltipsRecursive(juce::Component& component)
    {
        // Check component name against tooltip map
        juce::String name = component.getName();
        if (tooltips.count(name) > 0)
        {
            component.setHelpText(tooltips[name]);
        }
        
        // Recursively apply to children
        for (int i = 0; i < component.getNumChildComponents(); ++i)
        {
            applyTooltipsRecursive(*component.getChildComponent(i));
        }
    }
    
    static std::map<juce::String, juce::String> tooltips;
};

std::map<juce::String, juce::String> TooltipManager::tooltips;
