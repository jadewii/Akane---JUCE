#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
 * Professional Basic Oscillator with PolyBLEP Anti-Aliasing
 *
 * Provides 5 classic waveforms: Sine, Saw, Square, Triangle, Pulse
 * Uses PolyBLEP (Polynomial Bandwidth-Limited Step) to eliminate harsh aliasing
 * in discontinuous waveforms (Saw, Square, Pulse)
 */
class BasicOscillator
{
public:
    enum class WaveType
    {
        Sine,
        Saw,
        Square,
        Triangle,
        Pulse
    };

    BasicOscillator()
    {
        reset();
    }

    void setWaveType(WaveType type)
    {
        waveType = type;
    }

    void setFrequency(float freq)
    {
        frequency = juce::jlimit(20.0f, 20000.0f, freq);
        updatePhaseIncrement();
    }

    void setPulseWidth(float width)
    {
        pulseWidth = juce::jlimit(0.01f, 0.99f, width);
    }

    void setSampleRate(double sr)
    {
        sampleRate = sr;
        updatePhaseIncrement();
    }

    void reset()
    {
        phase = 0.0f;
        lastOutput = 0.0f;
    }

    /**
     * Generate one sample
     * Uses PolyBLEP to reduce aliasing in discontinuous waveforms
     */
    float processSample()
    {
        float output = 0.0f;

        switch (waveType)
        {
            case WaveType::Sine:
            {
                // Pure sine wave - no aliasing possible
                output = std::sin(2.0f * juce::MathConstants<float>::pi * phase);
                break;
            }

            case WaveType::Saw:
            {
                // Sawtooth with PolyBLEP anti-aliasing
                float naiveSaw = 2.0f * phase - 1.0f;
                output = naiveSaw - polyBLEP(phase, phaseIncrement);
                break;
            }

            case WaveType::Square:
            {
                // Square wave with PolyBLEP anti-aliasing at both transitions
                float naiveSquare = (phase < 0.5f) ? 1.0f : -1.0f;
                output = naiveSquare;

                // PolyBLEP at rising edge (phase = 0)
                output += polyBLEP(phase, phaseIncrement);

                // PolyBLEP at falling edge (phase = 0.5)
                output -= polyBLEP(std::fmod(phase + 0.5f, 1.0f), phaseIncrement);
                break;
            }

            case WaveType::Triangle:
            {
                // Triangle wave - smooth, no discontinuities, no aliasing
                if (phase < 0.25f)
                    output = 4.0f * phase;
                else if (phase < 0.75f)
                    output = 2.0f - 4.0f * phase;
                else
                    output = 4.0f * phase - 4.0f;
                break;
            }

            case WaveType::Pulse:
            {
                // Variable-width pulse with PolyBLEP anti-aliasing
                float naivePulse = (phase < pulseWidth) ? 1.0f : -1.0f;
                output = naivePulse;

                // PolyBLEP at rising edge (phase = 0)
                output += polyBLEP(phase, phaseIncrement);

                // PolyBLEP at falling edge (phase = pulseWidth)
                output -= polyBLEP(std::fmod(phase + (1.0f - pulseWidth), 1.0f), phaseIncrement);
                break;
            }
        }

        // Update phase for next sample
        phase += phaseIncrement;
        if (phase >= 1.0f)
            phase -= 1.0f;

        lastOutput = output;
        return output;
    }

private:
    /**
     * PolyBLEP (Polynomial Bandwidth-Limited Step)
     *
     * Removes aliasing from discontinuities in waveforms by smoothing
     * the transition using a polynomial curve.
     *
     * Based on the paper: "Alias-Free Digital Synthesis of Classic Analog Waveforms"
     * by Välimäki & Huovilainen
     *
     * @param t Current phase (0-1)
     * @param dt Phase increment per sample
     * @return Correction value to subtract from naive waveform
     */
    float polyBLEP(float t, float dt)
    {
        // Discontinuity at phase = 0 (or very close to 1)
        if (t < dt)
        {
            t = t / dt;
            // 2t - t^2 - 1
            return t + t - t * t - 1.0f;
        }
        // Discontinuity at phase = 1 (wrapping around)
        else if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            // t^2 + 2t + 1
            return t * t + t + t + 1.0f;
        }

        return 0.0f;
    }

    void updatePhaseIncrement()
    {
        phaseIncrement = frequency / static_cast<float>(sampleRate);

        // Clamp to prevent phase overflow
        phaseIncrement = juce::jlimit(0.0f, 0.5f, phaseIncrement);
    }

    WaveType waveType = WaveType::Saw;
    float frequency = 440.0f;
    float phase = 0.0f;
    float phaseIncrement = 0.0f;
    float pulseWidth = 0.5f;
    double sampleRate = 44100.0;
    float lastOutput = 0.0f;
};
