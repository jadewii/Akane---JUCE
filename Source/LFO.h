#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * WiiPluck LFO System
 * Professional-grade Low Frequency Oscillator with visual feedback
 * Supports: Sine, Triangle, Saw, Square, Random, Sample & Hold
 */

class LFO
{
public:
    enum class Shape
    {
        Sine,
        Triangle,
        Saw,
        Square,
        Random,
        SampleHold
    };
    
    LFO()
    {
        reset();
    }
    
    void prepare(double sampleRate)
    {
        this->sampleRate = sampleRate;
        phase = 0.0f;
        lastOutput = 0.0f;
        randomValue = 0.0f;
        sampleHoldValue = 0.0f;
    }
    
    void reset()
    {
        phase = 0.0f;
        lastOutput = 0.0f;
        randomValue = 0.0f;
        sampleHoldValue = 0.0f;
    }
    
    void setShape(Shape newShape)
    {
        shape = newShape;
    }
    
    void setRate(float rateHz)
    {
        rate = juce::jlimit(0.01f, 50.0f, rateHz);
    }
    
    void setDepth(float depthAmount)
    {
        depth = juce::jlimit(0.0f, 1.0f, depthAmount);
    }
    
    void setPhaseOffset(float offset)
    {
        phaseOffset = offset;
    }
    
    void setRetrigger(bool shouldRetrigger)
    {
        retrigger = shouldRetrigger;
    }
    
    void setBipolar(bool isBipolar)
    {
        bipolar = isBipolar;
    }
    
    void triggerRetrigger()
    {
        if (retrigger)
        {
            phase = phaseOffset;
        }
    }
    
    float getNextSample()
    {
        float output = 0.0f;
        float adjustedPhase = phase + phaseOffset;
        
        // Wrap phase
        while (adjustedPhase >= 1.0f) adjustedPhase -= 1.0f;
        while (adjustedPhase < 0.0f) adjustedPhase += 1.0f;
        
        switch (shape)
        {
            case Shape::Sine:
                output = std::sin(adjustedPhase * juce::MathConstants<float>::twoPi);
                break;
                
            case Shape::Triangle:
                if (adjustedPhase < 0.5f)
                    output = (adjustedPhase * 4.0f) - 1.0f;
                else
                    output = 3.0f - (adjustedPhase * 4.0f);
                break;
                
            case Shape::Saw:
                output = (adjustedPhase * 2.0f) - 1.0f;
                break;
                
            case Shape::Square:
                output = adjustedPhase < 0.5f ? 1.0f : -1.0f;
                break;
                
            case Shape::Random:
            {
                // Smooth random interpolation
                float target = (juce::Random::getSystemRandom().nextFloat() * 2.0f) - 1.0f;
                randomValue += (target - randomValue) * 0.001f;
                output = randomValue;
                break;
            }
                
            case Shape::SampleHold:
            {
                // Sample & Hold - update value at zero crossing
                if (phase < lastPhase)
                {
                    sampleHoldValue = (juce::Random::getSystemRandom().nextFloat() * 2.0f) - 1.0f;
                }
                output = sampleHoldValue;
                break;
            }
        }
        
        lastPhase = phase;
        
        // Advance phase
        float phaseIncrement = rate / sampleRate;
        phase += phaseIncrement;
        if (phase >= 1.0f)
            phase -= 1.0f;
        
        // Apply depth
        output *= depth;
        
        // Convert to unipolar if needed
        if (!bipolar)
        {
            output = (output + 1.0f) * 0.5f; // 0 to 1
        }
        
        lastOutput = output;
        return output;
    }
    
    float getCurrentValue() const
    {
        return lastOutput;
    }
    
    float getCurrentPhase() const
    {
        return phase;
    }
    
    Shape getCurrentShape() const
    {
        return shape;
    }
    
    float getRate() const
    {
        return rate;
    }
    
    float getDepth() const
    {
        return depth;
    }
    
    bool isBipolar() const
    {
        return bipolar;
    }
    
    // For visual display - get waveform at specific phase (0-1)
    float getWaveformAtPhase(float phaseValue) const
    {
        float output = 0.0f;
        
        switch (shape)
        {
            case Shape::Sine:
                output = std::sin(phaseValue * juce::MathConstants<float>::twoPi);
                break;
                
            case Shape::Triangle:
                if (phaseValue < 0.5f)
                    output = (phaseValue * 4.0f) - 1.0f;
                else
                    output = 3.0f - (phaseValue * 4.0f);
                break;
                
            case Shape::Saw:
                output = (phaseValue * 2.0f) - 1.0f;
                break;
                
            case Shape::Square:
                output = phaseValue < 0.5f ? 1.0f : -1.0f;
                break;
                
            case Shape::Random:
            case Shape::SampleHold:
                output = lastOutput;
                break;
        }
        
        return output * depth * (bipolar ? 1.0f : 0.5f) + (bipolar ? 0.0f : 0.5f);
    }
    
private:
    Shape shape = Shape::Sine;
    float rate = 1.0f;        // Hz
    float depth = 1.0f;       // 0-1
    float phase = 0.0f;
    float lastPhase = 0.0f;
    float phaseOffset = 0.0f;
    bool retrigger = false;
    bool bipolar = true;      // true = -1 to 1, false = 0 to 1
    
    double sampleRate = 44100.0;
    float lastOutput = 0.0f;
    float randomValue = 0.0f;
    float sampleHoldValue = 0.0f;
};

/**
 * LFO Visual Display Component
 * Shows real-time waveform with current phase position
 */
class LFODisplay : public juce::Component, private juce::Timer
{
public:
    LFODisplay(LFO& lfoToDisplay) : lfo(lfoToDisplay)
    {
        startTimerHz(60); // 60 FPS refresh
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        
        // Background - light pastel purple
        g.setColour(juce::Colour(0xfff0e0ff).withAlpha(0.4f));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Border - pastel pink
        g.setColour(juce::Colour(0xffffb3d9));
        g.drawRoundedRectangle(bounds, 4.0f, 1.5f);
        
        // Draw waveform
        juce::Path waveformPath;
        float width = bounds.getWidth();
        float height = bounds.getHeight();
        float centerY = bounds.getCentreY();
        
        bool firstPoint = true;
        for (int x = 0; x < width; ++x)
        {
            float phase = static_cast<float>(x) / width;
            float value = lfo.getWaveformAtPhase(phase);
            
            // Convert to screen coordinates (-1 to 1 -> top to bottom)
            float y = centerY - (value * height * 0.4f);
            
            if (firstPoint)
            {
                waveformPath.startNewSubPath(bounds.getX() + x, y);
                firstPoint = false;
            }
            else
            {
                waveformPath.lineTo(bounds.getX() + x, y);
            }
        }
        
        g.setColour(juce::Colour(0xffa8ffb4).withAlpha(0.8f)); // Pastel green
        g.strokePath(waveformPath, juce::PathStrokeType(2.0f));

        // Draw center line
        g.setColour(juce::Colour(0xffd8b5ff).withAlpha(0.3f)); // Light purple
        g.drawLine(bounds.getX(), centerY, bounds.getRight(), centerY, 1.0f);

        // Draw current phase indicator
        float currentPhase = lfo.getCurrentPhase();
        float indicatorX = bounds.getX() + (currentPhase * width);

        g.setColour(juce::Colour(0xffffb3d9)); // Pastel pink
        g.fillEllipse(indicatorX - 4.0f, centerY - 4.0f, 8.0f, 8.0f);

        // Draw vertical line at current phase
        g.setColour(juce::Colour(0xffffb3d9).withAlpha(0.3f)); // Pastel pink
        g.drawLine(indicatorX, bounds.getY(), indicatorX, bounds.getBottom(), 1.5f);
        
        // Draw LFO info text
        g.setColour(juce::Colour(0xffffffff).withAlpha(0.6f));
        g.setFont(10.0f);
        
        juce::String shapeText;
        switch (lfo.getCurrentShape())
        {
            case LFO::Shape::Sine: shapeText = "SINE"; break;
            case LFO::Shape::Triangle: shapeText = "TRI"; break;
            case LFO::Shape::Saw: shapeText = "SAW"; break;
            case LFO::Shape::Square: shapeText = "SQR"; break;
            case LFO::Shape::Random: shapeText = "RND"; break;
            case LFO::Shape::SampleHold: shapeText = "S&H"; break;
        }
        
        // REAL-TIME SAFE: Cache pre-formatted strings to avoid runtime string operations
        g.drawText(shapeText, bounds.reduced(4.0f), juce::Justification::topLeft);

        // Skip rate display to avoid ANY string operations during paint
        // (Rate can be shown in parameter labels instead)
    }
    
private:
    void timerCallback() override
    {
        repaint();
    }
    
    LFO& lfo;
};

