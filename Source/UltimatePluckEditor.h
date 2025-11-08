#pragma once
#include "UltimatePluckProcessor.h"
#include "TooltipSystem.h"
#include "ResizableUI.h"
// Skipping UndoRedoSystem - has bugs (missing Listener interface implementation)
#include "PresetBrowser.h"
#include "PerformancePanel.h"
#include <set>

//==============================================================================
// PROFESSIONAL LOOK AND FEEL - Pigments Style
//==============================================================================
class PigmentsStyleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PigmentsStyleLookAndFeel()
    {
        // Pastel purple, pink, green theme
        setColour(juce::Slider::thumbColourId, juce::Colour(0xffffb3d9)); // Pastel pink
        setColour(juce::Slider::trackColourId, juce::Colour(0xffffb3d9)); // Pastel pink
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xffd8b5ff)); // Pastel purple
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xffc8a0ff)); // Darker pastel purple
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xffffb3d9)); // Pastel pink
        setColour(juce::Label::textColourId, juce::Colours::white);
    }
    
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                         juce::Slider&) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
        auto centre = bounds.getCentre();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f * 0.75f;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        
        // Outer glow ring (pastel pink)
        g.setColour(juce::Colour(0xffffb3d9).withAlpha(0.2f));
        g.fillEllipse(centre.x - radius * 1.2f, centre.y - radius * 1.2f,
                     radius * 2.4f, radius * 2.4f);

        // Light purple background circle
        juce::ColourGradient bgGradient(
            juce::Colour(0xffd8b5ff), centre.x, centre.y - radius,
            juce::Colour(0xffc8a0ff), centre.x, centre.y + radius, false
        );
        g.setGradientFill(bgGradient);
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2, radius * 2);

        // Track arc (background) in lighter purple
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(centre.x, centre.y, radius * 0.85f, radius * 0.85f,
                                   0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xffe0ccff));
        g.strokePath(backgroundArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));

        // Value arc with gradient (pink to green)
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, radius * 0.85f, radius * 0.85f,
                              0.0f, rotaryStartAngle, toAngle, true);

        juce::ColourGradient arcGradient(
            juce::Colour(0xffffb3d9), centre.x - radius, centre.y,
            juce::Colour(0xffa8ffb4), centre.x + radius, centre.y, false
        );
        g.setGradientFill(arcGradient);
        g.strokePath(valueArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

        // Inner circle with subtle purple gradient
        juce::ColourGradient innerGradient(
            juce::Colour(0xfff0e0ff), centre.x, centre.y - radius * 0.5f,
            juce::Colour(0xffd8b5ff), centre.x, centre.y + radius * 0.5f, false
        );
        g.setGradientFill(innerGradient);
        g.fillEllipse(centre.x - radius * 0.6f, centre.y - radius * 0.6f,
                     radius * 1.2f, radius * 1.2f);

        // Value indicator dot
        float dotAngle = toAngle - juce::MathConstants<float>::halfPi;
        float dotX = centre.x + std::cos(dotAngle) * radius * 0.5f;
        float dotY = centre.y + std::sin(dotAngle) * radius * 0.5f;

        // Dot glow (green)
        g.setColour(juce::Colour(0xffa8ffb4).withAlpha(0.4f));
        g.fillEllipse(dotX - 6, dotY - 6, 12, 12);

        // Dot core (white)
        g.setColour(juce::Colours::white);
        g.fillEllipse(dotX - 3, dotY - 3, 6, 6);
    }
    
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                     int, int, int, int, juce::ComboBox&) override
    {
        auto cornerSize = 3.0f;
        juce::Rectangle<int> boxBounds(0, 0, width, height);
        
        // Background with purple gradient
        juce::ColourGradient gradient(
            juce::Colour(0xffd8b5ff), 0, 0,
            juce::Colour(0xffc8a0ff), 0, height, false
        );
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(boxBounds.toFloat(), cornerSize);

        // Pink border
        g.setColour(juce::Colour(0xffffb3d9).withAlpha(0.7f));
        g.drawRoundedRectangle(boxBounds.toFloat().reduced(0.5f, 0.5f), cornerSize, 1.0f);
    }
    
    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return juce::Font("Helvetica Neue", 14.0f, juce::Font::plain);
    }

    juce::Font getLabelFont(juce::Label&) override
    {
        return juce::Font("Helvetica Neue", 12.0f, juce::Font::plain);
    }
};

//==============================================================================
// CHROMATIC SCALE KEYBOARD - All notes same size
//==============================================================================
class ChromaticKeyboard : public juce::Component, private juce::MidiKeyboardState::Listener
{
public:
    ChromaticKeyboard(UltimatePluckProcessor& proc)
        : processor(proc)
    {
        setWantsKeyboardFocus(true);
        processor.keyboardState.addListener(this);
    }

    ~ChromaticKeyboard() override
    {
        processor.keyboardState.removeListener(this);
    }

    void paint(juce::Graphics& g) override
    {
        // Light purple background
        g.fillAll(juce::Colour(0xfff0e0ff));

        // Safety check
        if (getWidth() < 10 || getHeight() < 10)
            return;

        // 13 chromatic notes (C to C, one octave)
        int numNotes = 13;
        float noteWidth = getWidth() / float(numNotes);
        float noteHeight = getHeight();

        // Chromatic scale: C, C#, D, D#, E, F, F#, G, G#, A, A#, B, C
        int midiNotes[] = {60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72};
        bool isBlackNote[] = {false, true, false, true, false, false, true, false, true, false, true, false, false};

        for (int i = 0; i < numNotes; ++i)
        {
            float x = i * noteWidth;
            int midiNote = midiNotes[i];
            bool isBlack = isBlackNote[i];

            // Base color - white notes are lighter, black notes are purple
            juce::Colour baseColor = isBlack ? juce::Colour(0xffc8a0ff) : juce::Colours::white;

            g.setColour(baseColor);
            g.fillRect(x + 1, 1.0f, noteWidth - 2, noteHeight - 2);

            // Highlight if pressed
            if (processor.keyboardState.isNoteOn(1, midiNote))
            {
                g.setColour(juce::Colour(0xffa8ffb4).withAlpha(0.7f)); // Green
                g.fillRect(x + 1, 1.0f, noteWidth - 2, noteHeight - 2);
            }

            // Border - pink
            g.setColour(juce::Colour(0xffffb3d9).withAlpha(0.4f));
            g.drawRect(x + 1, 1.0f, noteWidth - 2, noteHeight - 2, 1.0f);
        }

        // Bottom indicator line (green)
        g.setColour(juce::Colour(0xffa8ffb4).withAlpha(0.5f));
        g.fillRect(0.0f, noteHeight - 2.0f, (float)getWidth(), 2.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        int midiNote = getMidiNoteAtPosition(e.position.x);
        if (midiNote >= 0)
            processor.keyboardState.noteOn(1, midiNote, 1.0f);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        int midiNote = getMidiNoteAtPosition(e.position.x);
        if (midiNote >= 0)
            processor.keyboardState.noteOff(1, midiNote, 0.0f);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        int midiNote = getMidiNoteAtPosition(e.position.x);
        if (midiNote >= 0)
        {
            // Turn off all currently playing notes
            for (int i = 60; i <= 72; ++i)
            {
                if (processor.keyboardState.isNoteOn(1, i))
                    processor.keyboardState.noteOff(1, i, 0.0f);
            }
            processor.keyboardState.noteOn(1, midiNote, 1.0f);
        }
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        // QWERTY keyboard to MIDI mapping
        // Bottom row: Z X C V B N M , . = C to C (white keys)
        // Top row: Q W E R T Y U I O P = C# to B (black keys)

        int midiNote = -1;
        char c = key.getTextCharacter();

        // White keys (bottom row)
        if (c == 'z' || c == 'Z') midiNote = 60;  // C
        else if (c == 'x' || c == 'X') midiNote = 62;  // D
        else if (c == 'c' || c == 'C') midiNote = 64;  // E
        else if (c == 'v' || c == 'V') midiNote = 65;  // F
        else if (c == 'b' || c == 'B') midiNote = 67;  // G
        else if (c == 'n' || c == 'N') midiNote = 69;  // A
        else if (c == 'm' || c == 'M') midiNote = 71;  // B
        else if (c == ',' || c == '<') midiNote = 72;  // C

        // Black keys (top row)
        else if (c == 'q' || c == 'Q') midiNote = 61;  // C#
        else if (c == 'w' || c == 'W') midiNote = 63;  // D#
        else if (c == 'e' || c == 'E') midiNote = 66;  // F#
        else if (c == 'r' || c == 'R') midiNote = 68;  // G#
        else if (c == 't' || c == 'T') midiNote = 70;  // A#

        if (midiNote >= 0)
        {
            processor.keyboardState.noteOn(1, midiNote, 1.0f);
            repaint();
            return true;
        }

        return false;
    }

    bool keyStateChanged(bool isKeyDown) override
    {
        if (!isKeyDown)
        {
            // Release all notes when any key is released
            for (int i = 60; i <= 72; ++i)
            {
                if (processor.keyboardState.isNoteOn(1, i))
                    processor.keyboardState.noteOff(1, i, 0.0f);
            }
            repaint();
            return true;
        }
        return false;
    }

private:
    UltimatePluckProcessor& processor;

    void handleNoteOn(juce::MidiKeyboardState*, int, int, float) override { repaint(); }
    void handleNoteOff(juce::MidiKeyboardState*, int, int, float) override { repaint(); }

    int getMidiNoteAtPosition(float x)
    {
        int numNotes = 13;
        float noteWidth = getWidth() / float(numNotes);
        int noteIndex = static_cast<int>(x / noteWidth);

        if (noteIndex >= 0 && noteIndex < numNotes)
        {
            int midiNotes[] = {60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72};
            return midiNotes[noteIndex];
        }
        return -1;
    }
};

//==============================================================================
// REAL-TIME OSCILLOSCOPE (Pigments has this!)
//==============================================================================
class WaveformDisplay : public juce::Component, private juce::Timer
{
public:
    WaveformDisplay()
    {
        startTimerHz(30);
    }
    
    void paint(juce::Graphics& g) override
    {
        // Light purple background with gradient
        juce::ColourGradient bgGradient(
            juce::Colour(0xffe8d5ff), 0, 0,
            juce::Colour(0xffd8b5ff), 0, getHeight(), false
        );
        g.setGradientFill(bgGradient);
        g.fillAll();

        // Grid lines (subtle purple)
        g.setColour(juce::Colour(0xfff0e0ff));
        for (int i = 1; i < 4; ++i)
        {
            float y = getHeight() * (i / 4.0f);
            g.drawLine(0, y, getWidth(), y, 0.5f);
        }

        // Center line (white)
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.drawLine(0, getHeight() / 2.0f, getWidth(), getHeight() / 2.0f, 1.0f);

        // Waveform with glow effect
        juce::Path waveform;
        bool started = false;

        for (int i = 0; i < getWidth(); ++i)
        {
            int bufferIndex = (writePos + (i * bufferSize) / getWidth()) % bufferSize;
            float sample = buffer[bufferIndex];
            float y = getHeight() / 2.0f - sample * getHeight() * 0.35f;

            if (!started)
            {
                waveform.startNewSubPath(i, y);
                started = true;
            }
            else
            {
                waveform.lineTo(i, y);
            }
        }

        // Glow layers (pink and green)
        for (int i = 2; i >= 0; --i)
        {
            g.setColour(juce::Colour(0xffffb3d9).withAlpha(0.3f / (i + 1)));
            g.strokePath(waveform, juce::PathStrokeType(3.0f + i * 2));
        }

        // Main waveform line (green)
        g.setColour(juce::Colour(0xff88ff88));
        g.strokePath(waveform, juce::PathStrokeType(2.0f));
    }
    
    void pushSample(float sample)
    {
        buffer[writePos] = sample;
        writePos = (writePos + 1) % bufferSize;
    }
    
    void timerCallback() override
    {
        repaint();
    }
    
private:
    static constexpr int bufferSize = 1024;
    std::array<float, bufferSize> buffer{};
    int writePos = 0;
};

//==============================================================================
// LIVE SPECTRUM ANALYZER - Real-time FFT visualization
//==============================================================================
class LiveSpectrumAnalyzer : public juce::Component, private juce::Timer
{
public:
    LiveSpectrumAnalyzer()
    {
        fftData.fill(0.0f);
        startTimerHz(30);
    }

    void pushSample(float sample)
    {
        if (fifoIndex < fftSize)
        {
            fifo[fifoIndex++] = sample;

            if (fifoIndex == fftSize)
            {
                performFFT();
                fifoIndex = 0;
            }
        }
    }

    void pushSamples(const float* samples, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
            pushSample(samples[i]);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Draw spectrum bars (pink/green gradient)
        int numBars = 32;
        float barWidth = bounds.getWidth() / numBars;

        for (int i = 0; i < numBars; ++i)
        {
            float magnitude = fftData[i];
            float barHeight = magnitude * bounds.getHeight() * 0.8f;

            // Gradient from pink to green
            float hue = juce::jmap(static_cast<float>(i), 0.0f, static_cast<float>(numBars), 0.85f, 0.35f);
            juce::Colour barColour = juce::Colour::fromHSV(hue, 0.5f, 0.9f, 0.3f);

            g.setColour(barColour);
            g.fillRect(i * barWidth, bounds.getHeight() - barHeight, barWidth - 1, barHeight);
        }
    }

private:
    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;

    std::array<float, fftSize> fifo{};
    std::array<float, fftSize * 2> fftData{};
    juce::dsp::FFT fft{fftOrder};
    int fifoIndex = 0;

    void performFFT()
    {
        std::copy(fifo.begin(), fifo.end(), fftData.begin());
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        // Normalize and smooth
        for (size_t i = 0; i < 32; ++i)
        {
            float val = fftData[i] / fftSize;
            fftData[i] = fftData[i] * 0.7f + val * 0.3f;  // Smooth
        }
    }

    void timerCallback() override
    {
        repaint();
    }
};

//==============================================================================
// LIVE GRAIN VISUALIZER - Particle animation
//==============================================================================
class LiveGrainVisualizer : public juce::Component, private juce::Timer
{
public:
    LiveGrainVisualizer()
    {
        startTimerHz(60);
    }

    void addGrain(float x, float y, float velocity)
    {
        Grain grain;
        grain.x = x;
        grain.y = y;
        grain.vx = (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 4.0f;  // More horizontal movement
        grain.vy = -velocity * 5.0f;  // Faster upward movement
        grain.life = 1.0f;
        grain.hue = juce::Random::getSystemRandom().nextFloat();

        grains.push_back(grain);

        if (grains.size() > 300)  // Allow more particles
            grains.erase(grains.begin());
    }

    void triggerFromAudio(float amplitude)
    {
        currentAmplitude = amplitude;
    }

    void paint(juce::Graphics& g) override
    {
        for (const auto& grain : grains)
        {
            float alpha = grain.life * 0.7f;  // More opaque
            juce::Colour colour = juce::Colour::fromHSV(grain.hue, 0.7f, 1.0f, alpha);

            g.setColour(colour);
            float size = grain.life * 15.0f;  // Much bigger particles
            g.fillEllipse(grain.x - size/2, grain.y - size/2, size, size);

            // Bigger glow effect
            g.setColour(colour.withAlpha(alpha * 0.5f));
            g.fillEllipse(grain.x - size * 1.5f, grain.y - size * 1.5f, size * 3.0f, size * 3.0f);
        }
    }

private:
    struct Grain
    {
        float x, y, vx, vy, life, hue;
    };

    std::vector<Grain> grains;

    void timerCallback() override
    {
        // Update particles
        for (auto& grain : grains)
        {
            grain.x += grain.vx;
            grain.y += grain.vy;
            grain.vy += 0.3f;  // More gravity
            grain.life *= 0.97f;  // Slower decay
        }

        // Remove dead particles
        grains.erase(
            std::remove_if(grains.begin(), grains.end(),
                [](const Grain& g) { return g.life < 0.01f; }),
            grains.end()
        );

        // Spawn grains ONLY when there's actual audio
        if (currentAmplitude > 0.01f)
        {
            // Spawn multiple grains per frame for more density
            int numToSpawn = static_cast<int>(currentAmplitude * 10.0f);  // More particles with louder sound
            for (int i = 0; i < numToSpawn; ++i)
            {
                float x = juce::Random::getSystemRandom().nextFloat() * getWidth();
                float y = getHeight() * (0.7f + juce::Random::getSystemRandom().nextFloat() * 0.3f);
                addGrain(x, y, currentAmplitude);
            }
        }

        // Decay amplitude
        currentAmplitude *= 0.8f;

        repaint();
    }

    float currentAmplitude = 0.0f;
};

//==============================================================================
// SECTION COMPONENT (Groups parameters visually)
//==============================================================================
class ParameterSection : public juce::Component
{
public:
    ParameterSection(const juce::String& name, juce::Colour accentColour)
        : sectionName(name), accent(accentColour)
    {
        titleLabel.setText(name, juce::dontSendNotification);
        titleLabel.setFont(juce::Font("Helvetica Neue", 14.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, accent);
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);
    }

    void setBackgroundVisualization(juce::Component* viz)
    {
        backgroundViz = viz;
        if (backgroundViz)
        {
            addAndMakeVisible(backgroundViz);
            backgroundViz->toBack();  // Send to back so it's behind knobs
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        // Light semi-transparent pastel purple background
        juce::ColourGradient gradient(
            juce::Colour(0xfff0e0ff).withAlpha(0.3f), 0, 0,
            juce::Colour(0xffe8d5ff).withAlpha(0.4f), 0, bounds.getHeight(), false
        );
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

        // Top accent line (pink or green)
        g.setColour(accent.withAlpha(0.6f));
        g.fillRect(bounds.removeFromTop(3).toFloat());

        // Border with accent color
        g.setColour(accent.withAlpha(0.5f));
        g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 2.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        titleLabel.setBounds(bounds.removeFromTop(25));

        // Resize background visualization to fill the section
        if (backgroundViz)
        {
            backgroundViz->setBounds(getLocalBounds().reduced(5));
        }
    }

private:
    juce::String sectionName;
    juce::Colour accent;
    juce::Label titleLabel;
    juce::Component* backgroundViz = nullptr;
};

//==============================================================================
// ULTIMATE PLUCK EDITOR
//==============================================================================
class UltimatePluckEditor : public juce::AudioProcessorEditor
{
public:
    UltimatePluckEditor(UltimatePluckProcessor& p)
        : AudioProcessorEditor(&p), processor(p), keyboard(p),
          modulationMatrixView(p.modulationMatrix)
    {
        setLookAndFeel(&pigmentsLookAndFeel);
        setWantsKeyboardFocus(true);  // Enable keyboard input for playing notes

        // Title
        titleLabel.setText("AKANE", juce::dontSendNotification);
        titleLabel.setFont(juce::Font("Helvetica Neue", 24.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::black); // Black text on pink background
        titleLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xffffb3d9)); // Pink background like selected button
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        // Keyboard (always visible)
        addAndMakeVisible(keyboard);

        // Main synthesis sections (always visible)
        addAndMakeVisible(ringsSection);
        addAndMakeVisible(cloudsSection);
        addAndMakeVisible(wavetableSection);
        addAndMakeVisible(envelopeSection);
        addAndMakeVisible(filterSection);

        // Tab buttons for switching views
        mainTabButton.setButtonText("SYNTH");
        mainTabButton.setToggleState(true, juce::dontSendNotification);
        mainTabButton.onClick = [this] { setActiveTab(0); };
        addAndMakeVisible(mainTabButton);

        effectsTabButton.setButtonText("EFFECTS");
        effectsTabButton.onClick = [this] { setActiveTab(1); };
        addAndMakeVisible(effectsTabButton);

        modulationTabButton.setButtonText("MODULATION");
        modulationTabButton.onClick = [this] { setActiveTab(2); };
        addAndMakeVisible(modulationTabButton);

        visualTabButton.setButtonText("VISUAL");
        visualTabButton.onClick = [this] { setActiveTab(3); };
        addAndMakeVisible(visualTabButton);

        // Preset panel button
        presetButton.setButtonText("PRESETS");
        presetButton.onClick = [this] { togglePresetPanel(); };
        addAndMakeVisible(presetButton);

        // Prev/Next preset buttons
        prevPresetButton.setButtonText("<");
        prevPresetButton.onClick = [this] { loadPreviousPreset(); };
        addAndMakeVisible(prevPresetButton);

        nextPresetButton.setButtonText(">");
        nextPresetButton.onClick = [this] { loadNextPreset(); };
        addAndMakeVisible(nextPresetButton);

        // Tab content panels
        // LFO section for modulation tab
        if (processor.lfoSection)
            addChildComponent(processor.lfoSection.get());
        addChildComponent(modulationMatrixView);

        // Effects and Performance Controls for effects tab
        addChildComponent(effectsPanel);
        addChildComponent(performancePanel);

        // Visual Feedback for visual tab
        addChildComponent(visualFeedbackPanel);

        // Register visual feedback with processor
        processor.setVisualFeedbackPanel(&visualFeedbackPanel);

        // Setup background visualizations
        envelopeSection.setBackgroundVisualization(&spectrumAnalyzer);
        filterSection.setBackgroundVisualization(&grainVisualizer);

        // Setup tooltips (pastel themed)
        tooltipWindow = std::make_unique<EnhancedTooltipWindow>(this);
        TooltipManager::setupTooltips(*this, processor.getAPVTS());

        // Setup resizable UI manager
        resizableManager = std::make_unique<ResizableUIManager>(*this, 800, 600, 2000, 1500, 1000, 700);
        resizableManager->attachToEditor();

        // Setup preset browser as side panel
        presetBrowser = std::make_unique<PresetBrowser>(processor.getPresetManager());
        addChildComponent(presetBrowser.get()); // Hidden initially

        // Set initial tab
        setActiveTab(0);

        // Engine mode selector
        engineModeCombo.addItem("Rings", 1);
        engineModeCombo.addItem("Clouds", 2);
        engineModeCombo.addItem("Karplus-Strong", 3);
        engineModeCombo.addItem("Rings → Grains", 4);
        engineModeCombo.addItem("Hybrid All", 5);
        engineModeCombo.setSelectedId(5);
        addAndMakeVisible(engineModeCombo);
        engineModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.getAPVTS(), "engineMode", engineModeCombo);
        
        engineModeLabel.setText("ENGINE MODE", juce::dontSendNotification);
        engineModeLabel.setJustificationType(juce::Justification::centred);
        engineModeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffdddddd));
        addAndMakeVisible(engineModeLabel);

        // Preset browser (temporarily disabled)
        // presetLabel.setText("PRESETS", juce::dontSendNotification);
        // presetLabel.setJustificationType(juce::Justification::centred);
        // presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        // addAndMakeVisible(presetLabel);

        // updatePresetList();
        // presetCombo.onChange = [this] { loadPreset(); };
        // addAndMakeVisible(presetCombo);

        // savePresetButton.setButtonText("Save");
        // savePresetButton.onClick = [this] { savePreset(); };
        // addAndMakeVisible(savePresetButton);

        // Setup all knobs with their sections (pink and green accents)
        setupKnobsInSection("RINGS", juce::Colour(0xffffb3d9), // Pastel pink
                           ringsBrightness, "ringsBrightness", "Brightness", ringsBrightnessLabel, ringsBrightnessAttachment,
                           ringsDamping, "ringsDamping", "Damping", ringsDampingLabel, ringsDampingAttachment,
                           ringsPosition, "ringsPosition", "Position", ringsPositionLabel, ringsPositionAttachment,
                           ringsStructure, "ringsStructure", "Structure", ringsStructureLabel, ringsStructureAttachment);

        setupKnobsInSection("CLOUDS", juce::Colour(0xffa8ffb4), // Pastel green
                           cloudsPosition, "cloudsPosition", "Position", cloudsPositionLabel, cloudsPositionAttachment,
                           cloudsSize, "cloudsSize", "Grain Size", cloudsSizeLabel, cloudsSizeAttachment,
                           cloudsDensity, "cloudsDensity", "Density", cloudsDensityLabel, cloudsDensityAttachment,
                           cloudsTexture, "cloudsTexture", "Texture", cloudsTextureLabel, cloudsTextureAttachment);

        setupKnobsInSection("WAVETABLE", juce::Colour(0xffffb3d9), // Pastel pink
                           wavetableMorph, "wavetableMorph", "Morph", wavetableMorphLabel, wavetableMorphAttachment,
                           wavetableWarp, "wavetableWarp", "Warp", wavetableWarpLabel, wavetableWarpAttachment,
                           wavetableFold, "wavetableFold", "Fold", wavetableFoldLabel, wavetableFoldAttachment,
                           ringsMix, "ringsMix", "Rings Mix", ringsMixLabel, ringsMixAttachment);

        setupKnobsInSection("ENVELOPE", juce::Colour(0xffa8ffb4), // Pastel green
                           attackKnob, "attack", "Attack", attackLabel, attackAttachment,
                           decayKnob, "decay", "Decay", decayLabel, decayAttachment,
                           sustainKnob, "sustain", "Sustain", sustainLabel, sustainAttachment,
                           releaseKnob, "release", "Release", releaseLabel, releaseAttachment);

        setupKnobsInSection("FILTER", juce::Colour(0xffffb3d9), // Pastel pink
                           filterCutoff, "filterCutoff", "Cutoff", filterCutoffLabel, filterCutoffAttachment,
                           filterResonance, "filterResonance", "Resonance", filterResonanceLabel, filterResonanceAttachment,
                           filterEnv, "filterEnv", "Env Amt", filterEnvLabel, filterEnvAttachment,
                           reverbKnob, "reverbMix", "Reverb", reverbLabel, reverbAttachment);

        // Style tab buttons
        styleTabButton(mainTabButton);
        styleTabButton(effectsTabButton);
        styleTabButton(modulationTabButton);
        styleTabButton(visualTabButton);

        // Style preset buttons (same as tab buttons but not toggleable)
        presetButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd8b5ff).withAlpha(0.3f));
        presetButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdddddd));
        prevPresetButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd8b5ff).withAlpha(0.3f));
        prevPresetButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdddddd));
        nextPresetButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd8b5ff).withAlpha(0.3f));
        nextPresetButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdddddd));

        // Hide the ugly macOS title bar for clean pastel look
        if (auto* peer = getPeer())
            peer->setHasChangedSinceSaved(false);

        setSize(1000, 700); // Compact professional VST size
    }
    
    ~UltimatePluckEditor() override
    {
        setLookAndFeel(nullptr);
    }
    
    void paint(juce::Graphics& g) override
    {
        // Pastel purple background gradient
        juce::ColourGradient mainGradient(
            juce::Colour(0xffe8d5ff), 0, 0,
            juce::Colour(0xffc8a0ff), 0, getHeight(), false
        );
        g.setGradientFill(mainGradient);
        g.fillAll();

        // Pink title area background (same as selected button)
        g.setColour(juce::Colour(0xffffb3d9));
        g.fillRect(0, 0, 250, 35);
    }
    
    void resized() override
    {
        auto area = getLocalBounds();

        // Top bar: Title + Tab buttons (compact)
        auto topBar = area.removeFromTop(35);
        titleLabel.setBounds(topBar.removeFromLeft(250).reduced(8, 3));

        // Preset navigation buttons on the right
        nextPresetButton.setBounds(topBar.removeFromRight(35).reduced(3));
        prevPresetButton.setBounds(topBar.removeFromRight(35).reduced(3));
        presetButton.setBounds(topBar.removeFromRight(75).reduced(3));

        // Engine selector
        auto engineArea = topBar.removeFromRight(180).reduced(3);
        engineModeLabel.setBounds(engineArea.removeFromLeft(60));
        engineModeCombo.setBounds(engineArea);

        // Tab buttons in center
        auto tabArea = topBar.reduced(3);
        int tabWidth = 95;
        mainTabButton.setBounds(tabArea.removeFromLeft(tabWidth));
        effectsTabButton.setBounds(tabArea.removeFromLeft(tabWidth));
        modulationTabButton.setBounds(tabArea.removeFromLeft(tabWidth));
        visualTabButton.setBounds(tabArea.removeFromLeft(tabWidth));

        // Keyboard at bottom (twice as tall now)
        keyboard.setBounds(area.removeFromBottom(70).reduced(8, 3));

        // Preset browser panel on right side (if visible)
        if (presetPanelVisible && presetBrowser)
        {
            auto presetPanel = area.removeFromRight(350);
            presetBrowser->setBounds(presetPanel);
            presetBrowser->setVisible(true);
        }
        else if (presetBrowser)
        {
            presetBrowser->setVisible(false);
        }

        // Main content area
        auto contentArea = area.reduced(8, 5);

        // TAB 0: SYNTH - Main synthesis controls (USE FULL SPACE)
        if (currentTab == 0)
        {
            // Top row: Rings, Clouds, Wavetable (3 across, use more space)
            auto topRow = contentArea.removeFromTop(contentArea.getHeight() * 0.6f);
            int sectionWidth = (topRow.getWidth() - 30) / 3;

            layoutKnobSection(ringsSection, topRow.removeFromLeft(sectionWidth),
                             {&ringsBrightness, &ringsDamping, &ringsPosition, &ringsStructure},
                             {&ringsBrightnessLabel, &ringsDampingLabel, &ringsPositionLabel, &ringsStructureLabel}, 2);

            topRow.removeFromLeft(15);
            layoutKnobSection(cloudsSection, topRow.removeFromLeft(sectionWidth),
                             {&cloudsPosition, &cloudsSize, &cloudsDensity, &cloudsTexture},
                             {&cloudsPositionLabel, &cloudsSizeLabel, &cloudsDensityLabel, &cloudsTextureLabel}, 2);

            topRow.removeFromLeft(15);
            layoutKnobSection(wavetableSection, topRow,
                             {&wavetableMorph, &wavetableWarp, &wavetableFold, &ringsMix},
                             {&wavetableMorphLabel, &wavetableWarpLabel, &wavetableFoldLabel, &ringsMixLabel}, 2);

            contentArea.removeFromTop(8);

            // Bottom row: Envelope, Filter (2 across, 4 knobs each in horizontal rows)
            auto bottomRow = contentArea;
            int bottomSectionWidth = (bottomRow.getWidth() - 15) / 2;

            layoutKnobSection(envelopeSection, bottomRow.removeFromLeft(bottomSectionWidth),
                             {&attackKnob, &decayKnob, &sustainKnob, &releaseKnob},
                             {&attackLabel, &decayLabel, &sustainLabel, &releaseLabel}, 4);

            bottomRow.removeFromLeft(15);
            layoutKnobSection(filterSection, bottomRow,
                             {&filterCutoff, &filterResonance, &filterEnv, &reverbKnob},
                             {&filterCutoffLabel, &filterResonanceLabel, &filterEnvLabel, &reverbLabel}, 4);
        }
        // TAB 1: EFFECTS - Effects and Performance Controls
        else if (currentTab == 1)
        {
            auto effectsArea = contentArea.removeFromTop(contentArea.getHeight() * 0.6f);
            effectsPanel.setBounds(effectsArea);

            contentArea.removeFromTop(8);
            performancePanel.setBounds(contentArea);
        }
        // TAB 2: MODULATION - LFOs and Matrix
        else if (currentTab == 2)
        {
            if (processor.lfoSection)
            {
                auto lfoArea = contentArea.removeFromTop(300);
                processor.lfoSection->setBounds(lfoArea);
                contentArea.removeFromTop(8);
            }
            modulationMatrixView.setBounds(contentArea);
        }
        // TAB 3: VISUAL - Grain visualizer and spectrum
        else if (currentTab == 3)
        {
            visualFeedbackPanel.setBounds(contentArea);
        }
    }
    
private:
    UltimatePluckProcessor& processor;
    PigmentsStyleLookAndFeel pigmentsLookAndFeel;

    juce::Label titleLabel;
    ChromaticKeyboard keyboard;

    // Tab system
    juce::TextButton mainTabButton, effectsTabButton, modulationTabButton, visualTabButton;
    int currentTab = 0;

    // Preset panel controls
    juce::TextButton presetButton;
    juce::TextButton prevPresetButton, nextPresetButton;
    bool presetPanelVisible = false;

    // Draggable sections
    ParameterSection ringsSection{"RINGS", juce::Colour(0xffffb3d9)};
    ParameterSection cloudsSection{"CLOUDS", juce::Colour(0xffa8ffb4)};
    ParameterSection wavetableSection{"WAVETABLE & MIX", juce::Colour(0xffffb3d9)};
    ParameterSection envelopeSection{"ENVELOPE", juce::Colour(0xffa8ffb4)};
    ParameterSection filterSection{"FILTER & FX", juce::Colour(0xffffb3d9)};

    // Live visualizations (drawn behind sections)
    LiveSpectrumAnalyzer spectrumAnalyzer;
    LiveGrainVisualizer grainVisualizer;

    // LFO and Modulation
    ModulationMatrixView modulationMatrixView;

    // Visual Feedback, Effects, and Performance Controls
    VisualFeedbackPanel visualFeedbackPanel;
    EffectsPanel effectsPanel{processor.getAPVTS()};
    PerformancePanel performancePanel{processor.getAPVTS()};

    // Engine selector
    juce::ComboBox engineModeCombo;
    juce::Label engineModeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> engineModeAttachment;

    // Preset browser
    juce::ComboBox presetCombo;
    juce::Label presetLabel;
    juce::TextButton savePresetButton;

    // Rings controls
    juce::Slider ringsBrightness, ringsDamping, ringsPosition, ringsStructure;
    juce::Label ringsBrightnessLabel, ringsDampingLabel, ringsPositionLabel, ringsStructureLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> 
        ringsBrightnessAttachment, ringsDampingAttachment, ringsPositionAttachment, ringsStructureAttachment;
    
    // Clouds controls
    juce::Slider cloudsPosition, cloudsSize, cloudsDensity, cloudsTexture;
    juce::Label cloudsPositionLabel, cloudsSizeLabel, cloudsDensityLabel, cloudsTextureLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        cloudsPositionAttachment, cloudsSizeAttachment, cloudsDensityAttachment, cloudsTextureAttachment;
    
    // Wavetable & Mix
    juce::Slider wavetableMorph, wavetableWarp, wavetableFold, ringsMix;
    juce::Label wavetableMorphLabel, wavetableWarpLabel, wavetableFoldLabel, ringsMixLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        wavetableMorphAttachment, wavetableWarpAttachment, wavetableFoldAttachment, ringsMixAttachment;
    
    // Envelope
    juce::Slider attackKnob, decayKnob, sustainKnob, releaseKnob;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        attackAttachment, decayAttachment, sustainAttachment, releaseAttachment;
    
    // Filter & Effects
    juce::Slider filterCutoff, filterResonance, filterEnv, reverbKnob;
    juce::Label filterCutoffLabel, filterResonanceLabel, filterEnvLabel, reverbLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        filterCutoffAttachment, filterResonanceAttachment, filterEnvAttachment, reverbAttachment;

    // Tooltip system
    std::unique_ptr<EnhancedTooltipWindow> tooltipWindow;

    // Resizable UI (Note: UndoRedo system has bugs, skipping for now)
    std::unique_ptr<ResizableUIManager> resizableManager;

    // Preset Browser
    std::unique_ptr<PresetBrowser> presetBrowser;

    template<typename... Args>
    void setupKnobsInSection(const juce::String&, juce::Colour, Args&... args)
    {
        setupKnobs(args...);
    }
    
    template<typename... Args>
    void setupKnobs(juce::Slider& slider, const juce::String& paramID, const juce::String& labelText,
                   juce::Label& label, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
                   Args&... args)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
        // Change value text color to pink (like under AKANE)
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffffb3d9)); // Pink text
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0x00000000)); // Transparent background
        // Don't add to editor - will be added to sections in layoutKnobSection

        label.setText(labelText.toUpperCase(), juce::dontSendNotification); // ALL CAPS
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold))); // Same font as SYNTH button
        label.setColour(juce::Label::textColourId, juce::Colour(0xff6b4f9e)); // Dark purple to match effects panel
        // Don't add to editor - will be added to sections in layoutKnobSection

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getAPVTS(), paramID, slider);

        if constexpr (sizeof...(args) > 0)
            setupKnobs(args...);
    }
    
    void setupKnobs() {} // Base case

    void setActiveTab(int tabIndex)
    {
        currentTab = tabIndex;

        // Update tab button states
        mainTabButton.setToggleState(tabIndex == 0, juce::dontSendNotification);
        effectsTabButton.setToggleState(tabIndex == 1, juce::dontSendNotification);
        modulationTabButton.setToggleState(tabIndex == 2, juce::dontSendNotification);
        visualTabButton.setToggleState(tabIndex == 3, juce::dontSendNotification);

        // Show/hide synthesis sections (visible on SYNTH tab)
        ringsSection.setVisible(tabIndex == 0);
        cloudsSection.setVisible(tabIndex == 0);
        wavetableSection.setVisible(tabIndex == 0);
        envelopeSection.setVisible(tabIndex == 0);
        filterSection.setVisible(tabIndex == 0);

        // Show/hide effects tab content
        effectsPanel.setVisible(tabIndex == 1);
        performancePanel.setVisible(tabIndex == 1);

        // Show/hide modulation tab content (LFOs only on MODULATION tab)
        if (processor.lfoSection)
            processor.lfoSection->setVisible(tabIndex == 2);
        modulationMatrixView.setVisible(tabIndex == 2);

        // Show/hide visual tab content
        visualFeedbackPanel.setVisible(tabIndex == 3);

        resized();
        repaint();
    }

    void styleTabButton(juce::TextButton& button)
    {
        button.setClickingTogglesState(true);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd8b5ff).withAlpha(0.3f));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffffb3d9));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdddddd));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff000000));
    }

    void layoutKnobSection(ParameterSection& section,
                          juce::Rectangle<int> area,
                          std::vector<juce::Slider*> knobs,
                          std::vector<juce::Label*> labels,
                          int knobsPerRow = 2)
    {
        // Position and size the section
        section.setBounds(area);

        // Add knobs to the section if not already added
        for (auto* knob : knobs)
        {
            if (knob->getParentComponent() != &section)
                section.addAndMakeVisible(knob);
        }
        for (auto* label : labels)
        {
            if (label->getParentComponent() != &section)
                section.addAndMakeVisible(label);
        }

        // Layout knobs within the section (relative to section, not main editor)
        auto localArea = section.getLocalBounds().reduced(15, 35); // Room for title and borders

        // Calculate spacing to fit all knobs nicely
        int numKnobs = static_cast<int>(knobs.size());
        int knobSize = juce::jmin(95, (localArea.getWidth() - 25) / knobsPerRow); // Bigger knobs for better visibility
        int spacing = 12;

        // Center the knobs horizontally
        int totalWidth = knobsPerRow * knobSize + (knobsPerRow - 1) * spacing;
        int startX = localArea.getX() + (localArea.getWidth() - totalWidth) / 2;

        for (int i = 0; i < numKnobs; ++i)
        {
            int row = i / knobsPerRow;
            int col = i % knobsPerRow;

            int x = startX + col * (knobSize + spacing);
            int y = localArea.getY() + row * (knobSize + spacing + 25);

            labels[static_cast<size_t>(i)]->setBounds(x, y, knobSize, 18);
            knobs[static_cast<size_t>(i)]->setBounds(x, y + 20, knobSize, knobSize + 20);
        }
    }

    void updatePresetList()
    {
        presetCombo.clear();
        auto& presetManager = processor.getPresetManager();
        auto presetNames = presetManager.getPresetNames();

        for (int i = 0; i < presetNames.size(); ++i)
        {
            presetCombo.addItem(presetNames[i], i + 1);
        }

        presetCombo.setSelectedId(presetManager.getCurrentPresetIndex() + 1, juce::dontSendNotification);
    }

    void loadPreset()
    {
        int selectedIndex = presetCombo.getSelectedItemIndex();
        if (selectedIndex >= 0)
        {
            processor.getPresetManager().loadPreset(selectedIndex);
        }
    }

    void savePreset()
    {
        juce::AlertWindow window("Save Preset", "Enter preset name:", juce::MessageBoxIconType::NoIcon);
        window.addTextEditor("presetName", "My Preset");
        window.addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
        window.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        if (window.runModalLoop() == 1)
        {
            auto presetName = window.getTextEditorContents("presetName");
            if (presetName.isNotEmpty())
            {
                processor.getPresetManager().savePreset(presetName, "User");
                updatePresetList();
            }
        }
    }

    void togglePresetPanel()
    {
        presetPanelVisible = !presetPanelVisible;
        resized();
    }

    void loadNextPreset()
    {
        // Generate a new random preset
        randomizeAllParameters();

        // Generate random name
        auto presetName = generateRandomPresetName();

        // Save it
        processor.getPresetManager().savePreset(presetName, "Random");

        // Refresh the preset browser to show the new preset
        if (presetBrowser)
        {
            juce::MessageManager::callAsync([this]()
            {
                if (presetBrowser)
                {
                    presetBrowser->refresh();
                }
            });
        }
    }

    juce::String generateRandomPresetName()
    {
        juce::StringArray adjectives = {
            "Cosmic", "Ethereal", "Dreamy", "Mystical", "Shimmering",
            "Dark", "Bright", "Warm", "Cold", "Metallic",
            "Organic", "Digital", "Analog", "Vintage", "Modern",
            "Deep", "Shallow", "Wide", "Narrow", "Gentle",
            "Harsh", "Soft", "Hard", "Smooth", "Rough",
            "Liquid", "Crystal", "Velvet", "Silk", "Glass"
        };

        juce::StringArray nouns = {
            "Bell", "Pad", "Pluck", "Lead", "Bass",
            "String", "Brass", "Voice", "Choir", "Piano",
            "Synth", "Organ", "Flute", "Harp", "Sweep",
            "Drone", "Atmosphere", "Texture", "Soundscape", "Wave",
            "Pulse", "Echo", "Shimmer", "Glow", "Dream",
            "Space", "Ocean", "Sky", "Wind", "Rain"
        };

        auto adj = adjectives[juce::Random::getSystemRandom().nextInt(adjectives.size())];
        auto noun = nouns[juce::Random::getSystemRandom().nextInt(nouns.size())];

        return adj + " " + noun;
    }

    void randomizeAllParameters()
    {
        auto& apvts = processor.getAPVTS();
        auto random = juce::Random::getSystemRandom();

        // Randomize each parameter with musically pleasing constraints
        for (auto* param : apvts.processor.getParameters())
        {
            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(param))
            {
                juce::String paramID = rangedParam->paramID;

                // SKIP effects parameters to avoid glitches
                if (paramID.contains("reverb") || paramID.contains("delay") || paramID.contains("Reverb") || paramID.contains("Delay"))
                    continue;

                float randomValue = random.nextFloat();

                // Apply musical constraints for certain parameters
                if (paramID.contains("attack"))
                    randomValue = random.nextFloat() * 0.3f; // Keep attacks relatively short
                else if (paramID.contains("decay"))
                    randomValue = random.nextFloat() * 0.6f;
                else if (paramID.contains("sustain"))
                    randomValue = 0.3f + random.nextFloat() * 0.7f; // Keep sustain above 0.3
                else if (paramID.contains("release"))
                    randomValue = random.nextFloat() * 0.8f;
                else if (paramID.contains("Cutoff") || paramID.contains("cutoff"))
                    randomValue = 0.2f + random.nextFloat() * 0.8f; // Keep filter open enough

                rangedParam->setValueNotifyingHost(randomValue);
            }
        }
    }

    void loadPreviousPreset()
    {
        auto& presetManager = processor.getPresetManager();
        int currentIndex = presetManager.getCurrentPresetIndex();
        int numPresets = presetManager.getPresetNames().size();

        if (numPresets > 0)
        {
            int prevIndex = (currentIndex - 1 + numPresets) % numPresets;
            presetManager.loadPreset(prevIndex);
        }
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        // Forward keyboard events to the chromatic keyboard
        return keyboard.keyPressed(key);
    }

    bool keyStateChanged(bool isKeyDown) override
    {
        // Forward keyboard events to the chromatic keyboard
        return keyboard.keyStateChanged(isKeyDown);
    }

    // Public methods to push audio to visualizations
    void pushAudioToVisualizations(const float* leftChannel, const float* rightChannel, int numSamples)
    {
        // Push to spectrum analyzer
        for (int i = 0; i < numSamples; ++i)
            spectrumAnalyzer.pushSample((leftChannel[i] + rightChannel[i]) * 0.5f);

        // Calculate RMS amplitude for grain visualizer
        float sum = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            float sample = (leftChannel[i] + rightChannel[i]) * 0.5f;
            sum += sample * sample;
        }
        float rms = std::sqrt(sum / numSamples);
        grainVisualizer.triggerFromAudio(rms);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UltimatePluckEditor)
};

// Update the processor to use our custom editor
inline juce::AudioProcessorEditor* UltimatePluckProcessor::createEditor()
{
    return new UltimatePluckEditor(*this);
}
