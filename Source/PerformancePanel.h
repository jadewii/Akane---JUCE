#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class PerformancePanel : public juce::Component
{
public:
    PerformancePanel(juce::AudioProcessorValueTreeState& apvts)
        : parameters(apvts)
    {
        // 8 Performance controls with fixed, useful labels
        setupKnob(portamentoSlider, "PORTAMENTO", portamentoLabel, "Glide time between notes");
        setupKnob(vibratoDepthSlider, "VIBRATO DEPTH", vibratoDepthLabel, "Vibrato intensity");
        setupKnob(vibratoRateSlider, "VIBRATO RATE", vibratoRateLabel, "Vibrato speed");
        setupKnob(masterTuneSlider, "MASTER TUNE", masterTuneLabel, "Global pitch offset");
        setupKnob(velocitySensSlider, "VELOCITY", velocitySensLabel, "Velocity response");
        setupKnob(panSpreadSlider, "PAN SPREAD", panSpreadLabel, "Stereo width");
        setupKnob(unisonVoicesSlider, "UNISON", unisonVoicesLabel, "Voice count");
        setupKnob(unisonDetuneSlider, "DETUNE", unisonDetuneLabel, "Voice detuning");

        // These would need to be added to the processor's APVTS
        // For now, they'll just be visual/UI only
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
        g.drawText("PERFORMANCE CONTROLS", titleArea.reduced(10), juce::Justification::centredLeft);

        g.setColour(juce::Colour(0xffd8b5ff));
        g.drawRect(getLocalBounds(), 2);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        bounds.removeFromTop(40);

        int knobWidth = bounds.getWidth() / 8;

        layoutKnob(portamentoSlider, portamentoLabel, bounds.removeFromLeft(knobWidth).reduced(3));
        layoutKnob(vibratoDepthSlider, vibratoDepthLabel, bounds.removeFromLeft(knobWidth).reduced(3));
        layoutKnob(vibratoRateSlider, vibratoRateLabel, bounds.removeFromLeft(knobWidth).reduced(3));
        layoutKnob(masterTuneSlider, masterTuneLabel, bounds.removeFromLeft(knobWidth).reduced(3));
        layoutKnob(velocitySensSlider, velocitySensLabel, bounds.removeFromLeft(knobWidth).reduced(3));
        layoutKnob(panSpreadSlider, panSpreadLabel, bounds.removeFromLeft(knobWidth).reduced(3));
        layoutKnob(unisonVoicesSlider, unisonVoicesLabel, bounds.removeFromLeft(knobWidth).reduced(3));
        layoutKnob(unisonDetuneSlider, unisonDetuneLabel, bounds.removeFromLeft(knobWidth).reduced(3));
    }

private:
    void setupKnob(juce::Slider& slider, const juce::String& labelText,
                   juce::Label& label, const juce::String& /* tooltip */)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRange(0.0, 1.0, 0.01);
        slider.setValue(0.0);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff6b4f9e));
        addAndMakeVisible(slider);

        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold))); // Bold, compact
        label.setColour(juce::Label::textColourId, juce::Colour(0xff6b4f9e));
        addAndMakeVisible(label);
    }

    void layoutKnob(juce::Slider& slider, juce::Label& label, juce::Rectangle<int> area)
    {
        label.setBounds(area.removeFromTop(18));
        slider.setBounds(area);
    }

    juce::AudioProcessorValueTreeState& parameters;

    juce::Slider portamentoSlider, vibratoDepthSlider, vibratoRateSlider, masterTuneSlider;
    juce::Slider velocitySensSlider, panSpreadSlider, unisonVoicesSlider, unisonDetuneSlider;

    juce::Label portamentoLabel, vibratoDepthLabel, vibratoRateLabel, masterTuneLabel;
    juce::Label velocitySensLabel, panSpreadLabel, unisonVoicesLabel, unisonDetuneLabel;
};
