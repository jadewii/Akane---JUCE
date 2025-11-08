#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include "AdvancedEffects.h"

// Custom LED indicator component
class LEDIndicator : public juce::Component
{
public:
    LEDIndicator() = default;

    void setOn(bool shouldBeOn)
    {
        isOn = shouldBeOn;
        repaint();
    }

    bool isLedOn() const { return isOn; }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto centre = bounds.getCentre();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.4f;

        // Outer circle (darker border)
        g.setColour(juce::Colour(0xff404040));
        g.fillEllipse(centre.x - radius - 1, centre.y - radius - 1, (radius + 1) * 2, (radius + 1) * 2);

        // Main LED circle
        if (isOn)
        {
            // Green when on
            g.setColour(juce::Colour(0xff00ff00));
            g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2, radius * 2);

            // Bright center highlight
            g.setColour(juce::Colour(0xff80ff80));
            g.fillEllipse(centre.x - radius * 0.6f, centre.y - radius * 0.6f, radius * 1.2f, radius * 1.2f);
        }
        else
        {
            // Red when off
            g.setColour(juce::Colour(0xffff0000));
            g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2, radius * 2);

            // Darker center when off
            g.setColour(juce::Colour(0xff800000));
            g.fillEllipse(centre.x - radius * 0.6f, centre.y - radius * 0.6f, radius * 1.2f, radius * 1.2f);
        }
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        setOn(!isOn);
        if (onClick)
            onClick(isOn);
    }

    std::function<void(bool)> onClick;

private:
    bool isOn = true;
};

class EffectsPanel : public juce::Component
{
public:
    EffectsPanel(juce::AudioProcessorValueTreeState& apvts)
        : parameters(apvts)
    {
        // Delay controls
        setupSlider(delayTimeSlider, "Time", 1.0, 2000.0);
        delayTimeSlider.setTextValueSuffix(" ms");
        setupSlider(delayFeedbackSlider, "Feedback", 0.0, 0.95);
        setupSlider(delayMixSlider, "Mix", 0.0, 1.0);
        setupSlider(delayFilterSlider, "Filter", 20.0, 20000.0);
        delayFilterSlider.setTextValueSuffix(" Hz");
        setupSlider(delayWidthSlider, "Width", 0.0, 1.0);

        pingPongButton.setButtonText("Ping-Pong");
        pingPongButton.setToggleable(true);
        pingPongButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd8b5ff));
        pingPongButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffc8a5ff));
        pingPongButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff6b4f9e));
        pingPongButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff6b4f9e));
        addAndMakeVisible(pingPongButton);

        // Delay bypass LED
        delayLED.setOn(true); // On by default
        delayLED.onClick = [this](bool isOn) {
            // Handle LED toggle - could connect to bypass parameter
        };
        addAndMakeVisible(delayLED);

        // Reverb controls
        setupSlider(reverbSizeSlider, "Size", 0.0, 1.0);
        setupSlider(reverbDampingSlider, "Damping", 0.0, 1.0);
        setupSlider(reverbWidthSlider, "Width", 0.0, 1.0);
        setupSlider(reverbMixSlider, "Mix", 0.0, 1.0);
        setupSlider(reverbShimmerSlider, "Shimmer", 0.0, 1.0);

        // Reverb bypass LED
        reverbLED.setOn(true); // On by default
        reverbLED.onClick = [this](bool isOn) {
            // Handle LED toggle - could connect to bypass parameter
        };
        addAndMakeVisible(reverbLED);

        // Chorus controls (replacing distortion)
        setupSlider(chorusRateSlider, "Rate", 0.1, 10.0);
        chorusRateSlider.setTextValueSuffix(" Hz");
        setupSlider(chorusDepthSlider, "Depth", 0.0, 1.0);
        setupSlider(chorusMixSlider, "Mix", 0.0, 1.0);
        setupSlider(chorusFeedbackSlider, "Feedback", 0.0, 0.7);
        setupSlider(chorusWidthSlider, "Stereo", 0.0, 1.0);

        // CREATE ATTACHMENTS - Connect UI to parameters
        delayTimeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, "delayTime", delayTimeSlider);
        delayFeedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, "delayFeedback", delayFeedbackSlider);
        delayMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, "delayMix", delayMixSlider);
        delayFilterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, "delayFilter", delayFilterSlider);
        delayWidthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, "delayWidth", delayWidthSlider);
        pingPongAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            parameters, "delayPingPong", pingPongButton);

        reverbSizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, "reverbSize", reverbSizeSlider);
        reverbDampingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, "reverbDamping", reverbDampingSlider);
        reverbWidthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, "reverbWidth", reverbWidthSlider);
        reverbMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, "reverbMix", reverbMixSlider);
        reverbShimmerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, "reverbShimmer", reverbShimmerSlider);

        chorusRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, "chorusRate", chorusRateSlider);
        chorusDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, "chorusDepth", chorusDepthSlider);
        chorusMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, "chorusMix", chorusMixSlider);
        chorusFeedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, "chorusFeedback", chorusFeedbackSlider);
        chorusWidthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, "chorusWidth", chorusWidthSlider);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        g.fillAll(juce::Colour(0xfff5f0ff));

        auto titleArea = bounds.removeFromTop(50).toFloat();
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(0xffe8dcff), 0, 0,
            juce::Colour(0xffd8b5ff), 0, titleArea.getBottom(), false));
        g.fillRect(titleArea);

        g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
        g.setColour(juce::Colour(0xff6b4f9e));
        g.drawText("EFFECTS", titleArea.reduced(15), juce::Justification::centredLeft);

        g.setColour(juce::Colour(0xffd8b5ff));
        g.drawRect(getLocalBounds(), 2);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        bounds.removeFromTop(40);

        // FIXED HEIGHT PER SECTION = BIG KNOBS LIKE MACRO KNOBS
        int sectionHeight = 165;  // Same as macro knob height

        // Delay section (FIRST)
        auto delayArea = bounds.removeFromTop(sectionHeight);
        layoutSection(delayArea, "DELAY",
            {&delayTimeSlider, &delayFeedbackSlider, &delayMixSlider, &delayFilterSlider, &delayWidthSlider, &pingPongButton});

        // Reverb section (SECOND)
        auto reverbArea = bounds.removeFromTop(sectionHeight);
        layoutSection(reverbArea, "REVERB",
            {&reverbSizeSlider, &reverbDampingSlider, &reverbWidthSlider, &reverbMixSlider, &reverbShimmerSlider});

        // Chorus section (THIRD - replacing distortion)
        auto chorusArea = bounds.removeFromTop(sectionHeight);
        layoutSection(chorusArea, "CHORUS",
            {&chorusRateSlider, &chorusDepthSlider, &chorusMixSlider, &chorusFeedbackSlider, &chorusWidthSlider});
    }

    AdvancedDelay delay;
    EnhancedReverb reverb;
    ChorusEffect chorus;
    
private:
    void setupSlider(juce::Slider& slider, const juce::String& label, double min, double max)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 120, 25); // BIGGER text box
        slider.setRange(min, max, 0.01);
        // Change value text color to pink (like under AKANE)
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffffb3d9)); // Pink text
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0x00000000)); // Transparent background
        addAndMakeVisible(slider);

        auto* lbl = new juce::Label();
        lbl->setText(label, juce::dontSendNotification);
        lbl->setJustificationType(juce::Justification::centred);
        lbl->setColour(juce::Label::textColourId, juce::Colour(0xff6b4f9e)); // Pastel purple text
        addAndMakeVisible(lbl);
        labels.add(lbl);
    }
    
    void layoutSection(juce::Rectangle<int>& area, const juce::String& title,
                      std::vector<juce::Component*> components)
    {
        // Add section title label
        auto* titleLabel = new juce::Label();
        titleLabel->setText(title, juce::dontSendNotification);
        titleLabel->setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        titleLabel->setColour(juce::Label::textColourId, juce::Colour(0xff6b4f9e)); // Dark purple
        titleLabel->setJustificationType(juce::Justification::centredLeft);
        titleLabel->setBounds(area.removeFromTop(25));
        addAndMakeVisible(titleLabel);
        sectionTitles.add(titleLabel);

        // Count sliders vs other controls
        int numSliders = 0;
        for (auto* comp : components)
        {
            if (dynamic_cast<juce::Slider*>(comp))
                numSliders++;
        }

        // Fixed width for non-slider controls (buttons)
        const int fixedControlWidth = 90;
        int numFixedControls = static_cast<int>(components.size()) - numSliders;

        // Remaining space for sliders
        int remainingWidth = area.getWidth() - (numFixedControls * fixedControlWidth);
        int sliderWidth = numSliders > 0 ? remainingWidth / numSliders : 0;

        // Add LED next to section title
        if (title == "DELAY")
        {
            // Position LED to the right of the title text
            auto titleBounds = sectionTitles.getLast()->getBounds();
            auto ledArea = juce::Rectangle<int>(titleBounds.getRight() + 10, titleBounds.getY() + 2, 16, 16);
            delayLED.setBounds(ledArea);
        }
        else if (title == "REVERB")
        {
            // Position LED to the right of the title text
            auto titleBounds = sectionTitles.getLast()->getBounds();
            auto ledArea = juce::Rectangle<int>(titleBounds.getRight() + 10, titleBounds.getY() + 2, 16, 16);
            reverbLED.setBounds(ledArea);
        }

        // Layout each component
        for (auto* comp : components)
        {
            if (dynamic_cast<juce::Slider*>(comp))
            {
                comp->setBounds(area.removeFromLeft(sliderWidth));
            }
            else
            {
                // Button - use fixed width
                comp->setBounds(area.removeFromLeft(fixedControlWidth));
            }
        }
    }

    juce::AudioProcessorValueTreeState& parameters;

    juce::Slider delayTimeSlider, delayFeedbackSlider, delayMixSlider, delayFilterSlider, delayWidthSlider;
    juce::TextButton pingPongButton;
    LEDIndicator delayLED;

    juce::Slider reverbSizeSlider, reverbDampingSlider, reverbWidthSlider, reverbMixSlider, reverbShimmerSlider;
    LEDIndicator reverbLED;

    juce::Slider chorusRateSlider, chorusDepthSlider, chorusMixSlider, chorusFeedbackSlider, chorusWidthSlider;

    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::Label> sectionTitles;

    // ATTACHMENTS - Connect UI to APVTS
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayTimeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayFeedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayFilterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayWidthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pingPongAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbSizeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbDampingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbWidthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbShimmerAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chorusRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chorusDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chorusMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chorusFeedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chorusWidthAttachment;
};
