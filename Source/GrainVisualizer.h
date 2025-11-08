#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>

/**
 * Grain Particle
 * Represents a single grain in the visualizer
 */
struct GrainParticle
{
    float x = 0.0f;           // Position (0-1)
    float y = 0.0f;           // Height (amplitude)
    float size = 1.0f;        // Particle size
    float pitch = 0.0f;       // Pitch offset (-12 to +12 semitones)
    float alpha = 1.0f;       // Transparency (fades out)
    float age = 0.0f;         // Time since creation
    float lifetime = 1.0f;    // Total lifetime
    juce::Colour color;       // Color based on pitch
    
    bool isActive() const
    {
        return age < lifetime && alpha > 0.0f;
    }
    
    void update(float deltaTime)
    {
        age += deltaTime;
        
        // Fade out over lifetime
        float normalizedAge = age / lifetime;
        alpha = juce::jmax(0.0f, 1.0f - normalizedAge);
        
        // Slight upward drift
        y += deltaTime * 0.1f;
    }
    
    juce::Colour getColor() const
    {
        // Color based on pitch: low = pink, mid = purple, high = green (pastel theme)
        float normalizedPitch = (pitch + 12.0f) / 24.0f; // 0 to 1

        if (normalizedPitch < 0.5f)
        {
            // Pink to Purple
            return juce::Colour(0xffffb3d9).interpolatedWith(
                juce::Colour(0xffd8b5ff),
                normalizedPitch * 2.0f
            );
        }
        else
        {
            // Purple to Green
            return juce::Colour(0xffd8b5ff).interpolatedWith(
                juce::Colour(0xffa8ffb4),
                (normalizedPitch - 0.5f) * 2.0f
            );
        }
    }
};

/**
 * Grain Visualizer
 * Real-time particle system showing active grains
 */
class GrainVisualizer : public juce::Component, private juce::Timer
{
public:
    GrainVisualizer()
    {
        particles.reserve(500); // Pre-allocate for performance
        startTimerHz(60); // 60 FPS
        setOpaque(false);
    }
    
    ~GrainVisualizer() override
    {
        stopTimer();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background with gradient (soft pastel purple)
        juce::ColourGradient gradient(
            juce::Colour(0xfff5f0ff), bounds.getX(), bounds.getY(),
            juce::Colour(0xffe8dcff), bounds.getX(), bounds.getBottom(),
            false
        );
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(bounds, 4.0f);

        // Grid lines (pastel purple)
        g.setColour(juce::Colour(0xffd8b5ff).withAlpha(0.2f));
        int numLines = 10;
        for (int i = 1; i < numLines; ++i)
        {
            float y = bounds.getY() + (bounds.getHeight() * i / numLines);
            g.drawLine(bounds.getX(), y, bounds.getRight(), y, 1.0f);
        }
        
        // Center line (pastel purple)
        g.setColour(juce::Colour(0xffd8b5ff).withAlpha(0.3f));
        g.drawLine(bounds.getX(), bounds.getCentreY(),
                   bounds.getRight(), bounds.getCentreY(), 2.0f);
        
        // Draw particles
        for (const auto& particle : particles)
        {
            if (!particle.isActive())
                continue;
            
            float screenX = bounds.getX() + (particle.x * bounds.getWidth());
            float screenY = bounds.getCentreY() - (particle.y * bounds.getHeight() * 0.4f);
            float screenSize = particle.size * 8.0f;
            
            // Glow effect
            juce::Colour glowColor = particle.getColor().withAlpha(particle.alpha * 0.3f);
            g.setColour(glowColor);
            g.fillEllipse(screenX - screenSize * 1.5f, 
                         screenY - screenSize * 1.5f,
                         screenSize * 3.0f, 
                         screenSize * 3.0f);
            
            // Core particle
            juce::Colour coreColor = particle.getColor().withAlpha(particle.alpha);
            g.setColour(coreColor);
            g.fillEllipse(screenX - screenSize * 0.5f,
                         screenY - screenSize * 0.5f,
                         screenSize,
                         screenSize);
            
            // Bright center
            g.setColour(juce::Colours::white.withAlpha(particle.alpha * 0.5f));
            g.fillEllipse(screenX - screenSize * 0.2f,
                         screenY - screenSize * 0.2f,
                         screenSize * 0.4f,
                         screenSize * 0.4f);
        }
        
        // Info text (dark purple for contrast)
        g.setColour(juce::Colour(0xff6b4f9e).withAlpha(0.8f));
        g.setFont(juce::FontOptions(11.0f));

        juce::String infoText = juce::String("Grains: ") + juce::String(getActiveParticleCount());
        infoText += " | Density: " + juce::String(grainDensity, 1);
        infoText += " | Texture: " + juce::String(texture, 2);

        g.drawText(infoText, bounds.reduced(8.0f).toNearestInt(),
                  juce::Justification::topLeft);

        // Title (pastel purple)
        g.setFont(juce::FontOptions(14.0f));
        g.setColour(juce::Colour(0xff9b7abf));
        g.drawText("GRAIN VISUALIZER", bounds.reduced(8.0f).toNearestInt(),
                  juce::Justification::topRight);

        // Border (pastel purple)
        g.setColour(juce::Colour(0xffd8b5ff).withAlpha(0.6f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 2.0f);
    }
    
    // Called from audio processor to spawn grains
    void spawnGrain(float position, float amplitude, float pitch, float size)
    {
        // Remove old inactive particles
        particles.erase(
            std::remove_if(particles.begin(), particles.end(),
                [](const GrainParticle& p) { return !p.isActive(); }),
            particles.end()
        );
        
        // Don't spawn if too many particles
        if (particles.size() >= 500)
            return;
        
        GrainParticle particle;
        particle.x = position;
        particle.y = amplitude;
        particle.size = size;
        particle.pitch = pitch;
        particle.lifetime = 0.5f + (size * 0.5f); // Larger grains live longer
        particle.color = particle.getColor();
        
        particles.push_back(particle);
    }
    
    // Batch spawn grains (for high density)
    void spawnGrains(int count, float density, float textureAmount)
    {
        grainDensity = density;
        texture = textureAmount;
        
        for (int i = 0; i < count; ++i)
        {
            float randomPos = juce::Random::getSystemRandom().nextFloat();
            float randomAmp = juce::Random::getSystemRandom().nextFloat() * 0.7f;
            float randomPitch = (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 24.0f;
            float randomSize = 0.5f + (juce::Random::getSystemRandom().nextFloat() * 1.5f);
            
            // Texture affects randomness
            randomPos += (juce::Random::getSystemRandom().nextFloat() - 0.5f) * textureAmount * 0.2f;
            randomPos = juce::jlimit(0.0f, 1.0f, randomPos);
            
            spawnGrain(randomPos, randomAmp, randomPitch, randomSize);
        }
    }
    
    // Update from audio processor parameters
    void updateParameters(float density, float grainSize, float position, float textureAmt)
    {
        grainDensity = density;
        grainSizeParam = grainSize;
        grainPosition = position;
        texture = textureAmt;
    }
    
    int getActiveParticleCount() const
    {
        int count = 0;
        for (const auto& p : particles)
        {
            if (p.isActive())
                ++count;
        }
        return count;
    }
    
private:
    void timerCallback() override
    {
        // Update all particles
        float deltaTime = 1.0f / 60.0f;
        
        for (auto& particle : particles)
        {
            particle.update(deltaTime);
        }
        
        // Spawn new grains based on density
        int grainsToSpawn = static_cast<int>(grainDensity * 2.0f);
        if (grainsToSpawn > 0)
        {
            spawnGrains(grainsToSpawn, grainDensity, texture);
        }
        
        repaint();
    }
    
    std::vector<GrainParticle> particles;
    float grainDensity = 0.0f;
    float grainSizeParam = 0.5f;
    float grainPosition = 0.5f;
    float texture = 0.0f;
};

/**
 * Spectral Analyzer
 * Shows frequency content in real-time
 */
class SpectralAnalyzer : public juce::Component, private juce::Timer
{
public:
    SpectralAnalyzer()
    {
        fftOrder = 11;  // 2048 samples
        fftSize = 1 << fftOrder;
        
        fftData.resize(fftSize * 2, 0.0f);
        spectrum.resize(fftSize / 2, 0.0f);
        
        window.resize(fftSize);
        for (int i = 0; i < fftSize; ++i)
        {
            // Hann window
            window[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / fftSize));
        }
        
        forwardFFT = std::make_unique<juce::dsp::FFT>(fftOrder);
        startTimerHz(30); // 30 FPS for spectrum
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background (soft pastel purple)
        juce::ColourGradient gradient(
            juce::Colour(0xfff5f0ff), bounds.getX(), bounds.getY(),
            juce::Colour(0xffe8dcff), bounds.getX(), bounds.getBottom(),
            false
        );
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Draw spectrum
        juce::Path spectrumPath;
        bool firstPoint = true;
        
        float width = bounds.getWidth();
        float height = bounds.getHeight();
        
        for (int i = 1; i < spectrum.size(); ++i)
        {
            // Logarithmic frequency scale
            float freq = (sampleRate * i) / fftSize;
            float logFreq = std::log10(freq / 20.0f) / std::log10(20000.0f / 20.0f);
            
            if (logFreq < 0.0f || logFreq > 1.0f)
                continue;
            
            float x = bounds.getX() + (logFreq * width);
            float magnitude = spectrum[i];
            float db = 20.0f * std::log10(magnitude + 0.0001f);
            float normalizedDb = juce::jmap(db, -60.0f, 0.0f, 0.0f, 1.0f);
            float y = bounds.getBottom() - (normalizedDb * height * 0.9f);
            
            if (firstPoint)
            {
                spectrumPath.startNewSubPath(x, y);
                firstPoint = false;
            }
            else
            {
                spectrumPath.lineTo(x, y);
            }
        }
        
        // Close path to bottom
        spectrumPath.lineTo(bounds.getRight(), bounds.getBottom());
        spectrumPath.lineTo(bounds.getX(), bounds.getBottom());
        spectrumPath.closeSubPath();
        
        // Fill spectrum (pastel purple to pink gradient)
        juce::ColourGradient spectrumGradient(
            juce::Colour(0xffd8b5ff).withAlpha(0.4f), bounds.getX(), bounds.getBottom(),
            juce::Colour(0xffffb3d9).withAlpha(0.6f), bounds.getX(), bounds.getY(),
            false
        );
        g.setGradientFill(spectrumGradient);
        g.fillPath(spectrumPath);

        // Stroke outline (pastel purple)
        g.setColour(juce::Colour(0xff9b7abf));
        g.strokePath(spectrumPath, juce::PathStrokeType(2.0f));
        
        // Frequency labels (dark purple for contrast)
        g.setColour(juce::Colour(0xff6b4f9e).withAlpha(0.8f));
        g.setFont(juce::FontOptions(10.0f));

        std::vector<float> frequencies = { 100.0f, 1000.0f, 10000.0f };
        for (float freq : frequencies)
        {
            float logFreq = std::log10(freq / 20.0f) / std::log10(20000.0f / 20.0f);
            float x = bounds.getX() + (logFreq * width);

            juce::String label = freq < 1000.0f ?
                juce::String(static_cast<int>(freq)) + "Hz" :
                juce::String(freq / 1000.0f, 1) + "k";

            g.drawText(label, x - 20, bounds.getBottom() - 20, 40, 15,
                      juce::Justification::centred);
        }

        // Title (pastel purple)
        g.setFont(juce::FontOptions(14.0f));
        g.setColour(juce::Colour(0xff9b7abf));
        g.drawText("SPECTRUM", bounds.reduced(8.0f).toNearestInt(),
                  juce::Justification::topRight);

        // Border (pastel purple)
        g.setColour(juce::Colour(0xffd8b5ff).withAlpha(0.6f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 2.0f);
    }
    
    void pushSamples(const float* samples, int numSamples)
    {
        const juce::ScopedLock lock(pathMutex);
        
        for (int i = 0; i < numSamples; ++i)
        {
            if (fftDataIndex >= fftSize)
            {
                // Apply window and perform FFT
                for (int j = 0; j < fftSize; ++j)
                {
                    fftData[j] *= window[j];
                }
                
                forwardFFT->performFrequencyOnlyForwardTransform(fftData.data());
                
                // Copy to spectrum with smoothing
                for (int j = 0; j < fftSize / 2; ++j)
                {
                    float magnitude = fftData[j] / fftSize;
                    spectrum[j] = spectrum[j] * 0.8f + magnitude * 0.2f; // Smooth
                }
                
                fftDataIndex = 0;
            }
            
            fftData[fftDataIndex++] = samples[i];
        }
    }
    
    void prepare(double sampleRateToUse)
    {
        sampleRate = sampleRateToUse;
    }
    
private:
    void timerCallback() override
    {
        repaint();
    }
    
    int fftOrder;
    int fftSize;
    std::vector<float> fftData;
    std::vector<float> spectrum;
    std::vector<float> window;
    int fftDataIndex = 0;
    
    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    juce::CriticalSection pathMutex;
    double sampleRate = 44100.0;
};
