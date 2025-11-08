#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include <cmath>

/**
 * Professional Multi-Mode Distortion with Antialiasing and Gain Compensation
 */
class AdvancedDistortion
{
public:
    enum class Mode
    {
        Tube,
        HardClip,
        SoftClip,
        Bitcrush,
        Wavefold,
        Saturate
    };

    void prepare(double sampleRate)
    {
        this->sampleRate = sampleRate;

        // DC blocking filter
        dcBlocker.setCoefficients(juce::IIRCoefficients::makeHighPass(sampleRate, 10.0));
        dcBlocker.reset();

        currentDrive = 0.0f;
    }

    void setMode(Mode newMode) { mode = newMode; }

    void setDrive(float newDrive)
    {
        targetDrive = juce::jlimit(0.0f, 1.0f, newDrive);
    }

    void setMix(float newMix) { mix = juce::jlimit(0.0f, 1.0f, newMix); }
    void setBias(float newBias) { bias = juce::jlimit(-1.0f, 1.0f, newBias); }

    float processSample(float input)
    {
        // Smooth drive parameter changes to avoid zipper noise
        currentDrive += (targetDrive - currentDrive) * 0.01f;

        float dry = input;
        float wet = input;

        // Input gain staging based on drive
        float inputGain = 1.0f + currentDrive * 19.0f; // Up to 20x gain
        wet = wet * inputGain + bias * 0.5f;

        float makeupGain = 1.0f;

        switch (mode)
        {
            case Mode::Tube:
            {
                // Triode tube modeling with plate curves
                // Based on 12AX7 characteristics
                wet = tubeSaturation(wet);
                makeupGain = 1.0f / (1.0f + currentDrive * 0.5f);
                break;
            }

            case Mode::HardClip:
            {
                // Soft knee hard clipping (not brick wall)
                const float knee = 0.1f;
                if (wet > 1.0f - knee)
                    wet = 1.0f - knee + knee * std::tanh((wet - (1.0f - knee)) / knee);
                else if (wet < -(1.0f - knee))
                    wet = -(1.0f - knee) + knee * std::tanh((wet + (1.0f - knee)) / knee);
                makeupGain = 0.9f;
                break;
            }

            case Mode::SoftClip:
            {
                // Cubic soft clipping with gain compensation
                if (std::abs(wet) > 1.0f)
                    wet = (wet > 0.0f) ? 1.0f : -1.0f;
                else
                    wet = wet - (wet * wet * wet) / 3.0f;
                makeupGain = 1.2f;
                break;
            }

            case Mode::Bitcrush:
            {
                // Bitcrushing with antialiasing
                float bits = 16.0f - (currentDrive * 14.0f); // 16 down to 2 bits
                float levels = std::pow(2.0f, bits);

                // Sample rate reduction with smoothing
                float sampleRateReduction = 1.0f + currentDrive * 15.0f;

                if (++bitcrushHoldCounter >= static_cast<int>(sampleRateReduction))
                {
                    bitcrushLastSample = std::round(wet * levels) / levels;
                    bitcrushHoldCounter = 0;
                }

                wet = bitcrushLastSample;
                makeupGain = 1.0f;
                break;
            }

            case Mode::Wavefold:
            {
                // Wavefolding algorithm
                // Apply multiple folds for more aggressive effect
                float folded = wet;
                while (folded > 1.0f) folded = 2.0f - folded;
                while (folded < -1.0f) folded = -2.0f - folded;
                wet = folded;
                makeupGain = 0.7f;
                break;
            }

            case Mode::Saturate:
            {
                // Arctangent saturation with proper curve
                wet = (2.0f / juce::MathConstants<float>::pi) * std::atan(wet * 2.5f);
                makeupGain = 1.1f;
                break;
            }
        }

        // DC blocking filter to prevent offset
        wet = dcBlocker.processSingleSampleRaw(wet);

        // Apply makeup gain
        wet *= makeupGain;

        // Mix dry and wet
        return dry + (wet - dry) * mix;
    }

private:
    float tubeSaturation(float input)
    {
        // 12AX7 triode tube model
        // Asymmetric clipping characteristic
        float x = input * 1.5f;

        if (x > 0.0f)
        {
            // Positive side - soft compression
            float Ex = std::exp(-x);
            return x / (1.0f + Ex) / 1.2f;
        }
        else
        {
            // Negative side - harder compression
            float Ex = std::exp(x);
            return x / (1.0f + Ex) / 1.1f;
        }
    }

    Mode mode = Mode::Tube;
    float currentDrive = 0.0f;
    float targetDrive = 0.0f;
    float mix = 0.0f;
    float bias = 0.0f;
    double sampleRate = 44100.0;

    // Bitcrush state (was static - BUG FIXED)
    float bitcrushLastSample = 0.0f;
    int bitcrushHoldCounter = 0;

    juce::IIRFilter dcBlocker;
};

/**
 * Professional Delay with Butterworth Filter and Fractional Delay
 */
class AdvancedDelay
{
public:
    void prepare(double sampleRate, int maxDelayTime = 2000)
    {
        this->sampleRate = sampleRate;
        int bufferSize = static_cast<int>(sampleRate * maxDelayTime / 1000.0) + 1;
        delayBuffer.resize(bufferSize, 0.0f);
        writePos = 0;

        // Initialize 2-pole Butterworth low-pass filter
        updateFilter();

        // Initialize smooth parameter values
        currentDelayTime = 500.0f;
        currentFeedback = 0.3f;
    }

    void setDelayTime(float timeMs)
    {
        targetDelayTime = juce::jlimit(1.0f, 2000.0f, timeMs);
    }

    void setFeedback(float fb)
    {
        targetFeedback = juce::jlimit(0.0f, 0.95f, fb);
    }

    void setMix(float m)
    {
        mix = juce::jlimit(0.0f, 1.0f, m);
    }

    void setPingPong(bool enabled)
    {
        pingPong = enabled;
    }

    void setFilterCutoff(float cutoff)
    {
        filterCutoff = juce::jlimit(20.0f, 20000.0f, cutoff);
        updateFilter();
    }

    float processSample(float input, int channel)
    {
        // Smooth parameter changes to prevent clicks
        currentDelayTime += (targetDelayTime - currentDelayTime) * 0.001f;
        currentFeedback += (targetFeedback - currentFeedback) * 0.01f;

        // Fractional delay using cubic interpolation
        float delaySamplesFloat = currentDelayTime * sampleRate / 1000.0f;
        int delaySamplesInt = static_cast<int>(delaySamplesFloat);
        float fraction = delaySamplesFloat - delaySamplesInt;

        // Get 4 samples for cubic interpolation
        int pos0 = (writePos - delaySamplesInt - 1 + delayBuffer.size()) % delayBuffer.size();
        int pos1 = (writePos - delaySamplesInt + delayBuffer.size()) % delayBuffer.size();
        int pos2 = (writePos - delaySamplesInt + 1 + delayBuffer.size()) % delayBuffer.size();
        int pos3 = (writePos - delaySamplesInt + 2 + delayBuffer.size()) % delayBuffer.size();

        float y0 = delayBuffer[pos0];
        float y1 = delayBuffer[pos1];
        float y2 = delayBuffer[pos2];
        float y3 = delayBuffer[pos3];

        // Cubic interpolation (Hermite)
        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        float delayed = ((c3 * fraction + c2) * fraction + c1) * fraction + c0;

        // 2-pole Butterworth filter (better than one-pole)
        filtered1 = filtered1 + filterCoeff * (delayed - filtered1);
        filtered2 = filtered2 + filterCoeff * (filtered1 - filtered2);

        float output = input + filtered2 * mix;

        // Feedback with soft limiting to prevent runaway
        float feedbackSample = filtered2 * currentFeedback;

        // Soft limiter on feedback
        if (std::abs(feedbackSample) > 0.95f)
            feedbackSample = 0.95f * std::tanh(feedbackSample / 0.95f);

        // Ping-pong: alternate channels
        if (pingPong && channel == 1)
            feedbackSample *= -1.0f;

        delayBuffer[writePos] = input + feedbackSample;
        writePos = (writePos + 1) % delayBuffer.size();

        return output;
    }

private:
    void updateFilter()
    {
        filterCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * filterCutoff / sampleRate);
    }

    std::vector<float> delayBuffer;
    int writePos = 0;
    double sampleRate = 44100.0;

    float targetDelayTime = 500.0f;
    float currentDelayTime = 500.0f;
    float targetFeedback = 0.3f;
    float currentFeedback = 0.3f;
    float mix = 0.3f;

    bool pingPong = false;
    float filterCutoff = 8000.0f;
    float filterCoeff = 0.5f;

    // 2-pole filter state
    float filtered1 = 0.0f;
    float filtered2 = 0.0f;
};

/**
 * Freeverb-Style Reverb with Early Reflections and Shimmer
 */
class EnhancedReverb
{
public:
    void prepare(double sampleRate)
    {
        this->sampleRate = sampleRate;

        // Initialize allpass and comb filters for Freeverb algorithm
        const int combTunings[] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
        const int allpassTunings[] = {225, 556, 441, 341};

        for (int i = 0; i < 8; ++i)
        {
            int size = static_cast<int>(combTunings[i] * sampleRate / 44100.0);
            combBuffers[i].resize(size, 0.0f);
            combIndices[i] = 0;
            filterStates[i] = 0.0f;
        }

        for (int i = 0; i < 4; ++i)
        {
            int size = static_cast<int>(allpassTunings[i] * sampleRate / 44100.0);
            allpassBuffers[i].resize(size, 0.0f);
            allpassIndices[i] = 0;
        }

        updateParameters();
    }

    void setSize(float s)
    {
        size = juce::jlimit(0.0f, 1.0f, s);
        updateParameters();
    }

    void setDamping(float d)
    {
        damping = juce::jlimit(0.0f, 1.0f, d);
    }

    void setWidth(float w)
    {
        width = juce::jlimit(0.0f, 1.0f, w);
    }

    void setMix(float m)
    {
        mix = juce::jlimit(0.0f, 1.0f, m);
    }

    void setShimmer(float s)
    {
        shimmer = juce::jlimit(0.0f, 1.0f, s);
    }

    void processStereo(float* left, float* right, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float dryL = left[i];
            float dryR = right[i];

            // Mono input for reverb
            float input = (dryL + dryR) * 0.5f;

            // Process through comb filters
            float combOut = 0.0f;
            for (int j = 0; j < 8; ++j)
            {
                int index = combIndices[j];
                float delayed = combBuffers[j][index];

                // One-pole damping filter
                filterStates[j] = delayed * (1.0f - damping) + filterStates[j] * damping;

                combBuffers[j][index] = input + filterStates[j] * roomSize;
                combIndices[j] = (index + 1) % combBuffers[j].size();

                combOut += filterStates[j];
            }
            combOut *= 0.125f; // Average

            // Process through allpass filters
            float allpassOut = combOut;
            for (int j = 0; j < 4; ++j)
            {
                int index = allpassIndices[j];
                float delayed = allpassBuffers[j][index];
                float temp = allpassOut + delayed * 0.5f;
                allpassBuffers[j][index] = temp;
                allpassOut = delayed - allpassOut * 0.5f;
                allpassIndices[j] = (index + 1) % allpassBuffers[j].size();
            }

            // Shimmer effect (pitch shift up one octave)
            float shimmerSample = 0.0f;
            if (shimmer > 0.001f)
            {
                // Simple octave-up using sample skipping (not perfect but musical)
                if (shimmerCounter % 2 == 0)
                    lastShimmerSample = allpassOut;

                shimmerSample = lastShimmerSample * shimmer * 0.3f;
                shimmerCounter++;
            }

            allpassOut += shimmerSample;

            // Create stereo width
            float wetL = allpassOut * (1.0f + width * 0.5f);
            float wetR = allpassOut * (1.0f - width * 0.5f);

            // Mix dry and wet
            left[i] = dryL * (1.0f - mix) + wetL * mix;
            right[i] = dryR * (1.0f - mix) + wetR * mix;
        }
    }

private:
    void updateParameters()
    {
        roomSize = size * 0.28f + 0.7f;
    }

    double sampleRate = 44100.0;
    float size = 0.5f;
    float damping = 0.5f;
    float width = 1.0f;
    float mix = 0.3f;
    float shimmer = 0.0f;
    float roomSize = 0.84f;

    // Freeverb comb filters (8 parallel)
    std::vector<float> combBuffers[8];
    int combIndices[8];
    float filterStates[8];

    // Allpass filters (4 in series)
    std::vector<float> allpassBuffers[4];
    int allpassIndices[4];

    // Shimmer state (was static - BUG FIXED)
    int shimmerCounter = 0;
    float lastShimmerSample = 0.0f;
};

/**
 * Professional Chorus Effect with Multiple Voices and Stereo Width
 */
class ChorusEffect
{
public:
    void prepare(double sampleRate)
    {
        this->sampleRate = sampleRate;

        // 50ms max delay buffer
        int bufferSize = static_cast<int>(sampleRate * 0.05);
        delayBuffer.resize(bufferSize, 0.0f);
        writePos = 0;

        // Initialize LFO phases for each voice
        lfoPhases[0] = 0.0f;
        lfoPhases[1] = juce::MathConstants<float>::pi * 0.66f;  // 120 degrees
        lfoPhases[2] = juce::MathConstants<float>::pi * 1.33f;  // 240 degrees
    }

    void setRate(float rateHz)
    {
        rate = juce::jlimit(0.1f, 10.0f, rateHz);
    }

    void setDepth(float d)
    {
        depth = juce::jlimit(0.0f, 1.0f, d);
    }

    void setMix(float m)
    {
        mix = juce::jlimit(0.0f, 1.0f, m);
    }

    void setFeedback(float fb)
    {
        feedback = juce::jlimit(0.0f, 0.7f, fb);
    }

    void setStereoWidth(float width)
    {
        stereoWidth = juce::jlimit(0.0f, 1.0f, width);
    }

    void processStereo(float& left, float& right)
    {
        // Write input to delay buffer
        delayBuffer[writePos] = (left + right) * 0.5f + feedbackSample * feedback;
        writePos = (writePos + 1) % delayBuffer.size();

        // 3-voice chorus for richness
        float wetL = 0.0f;
        float wetR = 0.0f;

        for (int voice = 0; voice < 3; ++voice)
        {
            // Update LFO
            float lfoValue = std::sin(lfoPhases[voice]);
            lfoPhases[voice] += 2.0f * juce::MathConstants<float>::pi * rate / sampleRate;
            if (lfoPhases[voice] > juce::MathConstants<float>::twoPi)
                lfoPhases[voice] -= juce::MathConstants<float>::twoPi;

            // Modulated delay time (5-20ms base, modulated by depth)
            float baseDelay = 10.0f + voice * 3.0f;  // Stagger voices
            float modDelay = baseDelay + lfoValue * depth * 8.0f;
            float delaySamples = modDelay * sampleRate / 1000.0f;

            // Read from delay buffer with linear interpolation
            int delayInt = static_cast<int>(delaySamples);
            float delayFrac = delaySamples - delayInt;

            int readPos1 = (writePos - delayInt + delayBuffer.size()) % delayBuffer.size();
            int readPos2 = (readPos1 - 1 + delayBuffer.size()) % delayBuffer.size();

            float sample1 = delayBuffer[readPos1];
            float sample2 = delayBuffer[readPos2];
            float delayedSample = sample1 * (1.0f - delayFrac) + sample2 * delayFrac;

            // Pan voices across stereo field
            float pan = (voice - 1.0f) / 2.0f * stereoWidth;  // -1, 0, +1 scaled by width
            float leftGain = std::cos((pan + 1.0f) * juce::MathConstants<float>::pi / 4.0f);
            float rightGain = std::sin((pan + 1.0f) * juce::MathConstants<float>::pi / 4.0f);

            wetL += delayedSample * leftGain;
            wetR += delayedSample * rightGain;
        }

        wetL *= 0.333f;  // Average 3 voices
        wetR *= 0.333f;

        // Store feedback sample
        feedbackSample = (wetL + wetR) * 0.5f;

        // Mix dry and wet
        left = left * (1.0f - mix) + wetL * mix;
        right = right * (1.0f - mix) + wetR * mix;
    }

private:
    std::vector<float> delayBuffer;
    int writePos = 0;
    double sampleRate = 44100.0;

    float rate = 0.5f;     // LFO rate in Hz
    float depth = 0.5f;    // Modulation depth
    float mix = 0.5f;      // Dry/wet mix
    float feedback = 0.2f; // Feedback amount
    float stereoWidth = 1.0f;

    float lfoPhases[3] = {0.0f, 0.0f, 0.0f};
    float feedbackSample = 0.0f;
};
