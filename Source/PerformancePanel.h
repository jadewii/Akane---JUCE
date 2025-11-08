#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class PerformancePanel : public juce::Component
{
public:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    PerformancePanel(juce::AudioProcessorValueTreeState& apvts)
        : parameters(apvts)
    {
        // 8 Performance controls with real functionality!
        setupPortamentoKnob();
        setupVibratoDepthKnob();
        setupVibratoRateKnob();
        setupMasterTuneKnob();
        setupVelocitySensKnob();
        setupPanSpreadKnob();
        setupUnisonVoicesKnob();
        setupUnisonDetuneKnob();

        // Create parameter attachments for real functionality
        portamentoAttachment = std::make_unique<SliderAttachment>(parameters, "portamento", portamentoSlider);
        vibratoDepthAttachment = std::make_unique<SliderAttachment>(parameters, "vibratoDepth", vibratoDepthSlider);
        vibratoRateAttachment = std::make_unique<SliderAttachment>(parameters, "vibratoRate", vibratoRateSlider);
        masterTuneAttachment = std::make_unique<SliderAttachment>(parameters, "masterTune", masterTuneSlider);
        velocitySensAttachment = std::make_unique<SliderAttachment>(parameters, "velocitySens", velocitySensSlider);
        panSpreadAttachment = std::make_unique<SliderAttachment>(parameters, "panSpread", panSpreadSlider);
        unisonVoicesAttachment = std::make_unique<SliderAttachment>(parameters, "unisonVoices", unisonVoicesSlider);
        unisonDetuneAttachment = std::make_unique<SliderAttachment>(parameters, "unisonDetune", unisonDetuneSlider);
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
    void setupPortamentoKnob()
    {
        setupKnobCommon(portamentoSlider, 0.0, 1.0, 0.01);
        portamentoLabel.setText("PORTAMENTO", juce::dontSendNotification);
        setupLabelCommon(portamentoLabel);
    }

    void setupVibratoDepthKnob()
    {
        setupKnobCommon(vibratoDepthSlider, 0.0, 1.0, 0.01);
        vibratoDepthLabel.setText("VIBRATO DEPTH", juce::dontSendNotification);
        setupLabelCommon(vibratoDepthLabel);
    }

    void setupVibratoRateKnob()
    {
        setupKnobCommon(vibratoRateSlider, 0.1, 10.0, 0.1);
        vibratoRateLabel.setText("VIBRATO RATE", juce::dontSendNotification);
        setupLabelCommon(vibratoRateLabel);
    }

    void setupMasterTuneKnob()
    {
        setupKnobCommon(masterTuneSlider, -100.0, 100.0, 1.0);
        masterTuneSlider.setTextValueSuffix(" cents");
        masterTuneLabel.setText("MASTER TUNE", juce::dontSendNotification);
        setupLabelCommon(masterTuneLabel);
    }

    void setupVelocitySensKnob()
    {
        setupKnobCommon(velocitySensSlider, 0.0, 2.0, 0.01);
        velocitySensLabel.setText("VELOCITY", juce::dontSendNotification);
        setupLabelCommon(velocitySensLabel);
    }

    void setupPanSpreadKnob()
    {
        setupKnobCommon(panSpreadSlider, 0.0, 1.0, 0.01);
        panSpreadLabel.setText("PAN SPREAD", juce::dontSendNotification);
        setupLabelCommon(panSpreadLabel);
    }

    void setupUnisonVoicesKnob()
    {
        setupKnobCommon(unisonVoicesSlider, 1.0, 4.0, 1.0);
        unisonVoicesSlider.setNumDecimalPlacesToDisplay(0); // Integer display
        unisonVoicesLabel.setText("UNISON", juce::dontSendNotification);
        setupLabelCommon(unisonVoicesLabel);
    }

    void setupUnisonDetuneKnob()
    {
        setupKnobCommon(unisonDetuneSlider, 0.0, 50.0, 1.0);
        unisonDetuneSlider.setTextValueSuffix(" cents");
        unisonDetuneLabel.setText("DETUNE", juce::dontSendNotification);
        setupLabelCommon(unisonDetuneLabel);
    }

    void setupKnobCommon(juce::Slider& slider, double min, double max, double step)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRange(min, max, step);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff6b4f9e));
        addAndMakeVisible(slider);
    }

    void setupLabelCommon(juce::Label& label)
    {
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
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

    // Parameter attachments for real functionality
    std::unique_ptr<SliderAttachment> portamentoAttachment;
    std::unique_ptr<SliderAttachment> vibratoDepthAttachment;
    std::unique_ptr<SliderAttachment> vibratoRateAttachment;
    std::unique_ptr<SliderAttachment> masterTuneAttachment;
    std::unique_ptr<SliderAttachment> velocitySensAttachment;
    std::unique_ptr<SliderAttachment> panSpreadAttachment;
    std::unique_ptr<SliderAttachment> unisonVoicesAttachment;
    std::unique_ptr<SliderAttachment> unisonDetuneAttachment;
};
