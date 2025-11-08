#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <vector>

/*
    ═══════════════════════════════════════════════════════════════════════════
    
    WIISYNTH ULTIMATE PLUCK MACHINE
    
    Combining the best of:
    - Arturia Pigments (dual engines, modulation matrix)
    - Mutable Instruments Rings (modal synthesis, polyphonic resonator)
    - Mutable Instruments Clouds (granular texture synthesis)
    
    THE ULTIMATE PLUCK SYNTHESIZER
    
    ═══════════════════════════════════════════════════════════════════════════
*/

//==============================================================================
// MUTABLE INSTRUMENTS RINGS - MODAL SYNTHESIS ENGINE
//==============================================================================
class ModalResonator
{
public:
    ModalResonator()
    {
        setSampleRate(44100.0);
        setResonatorModel(ResonatorModel::String);
    }
    
    enum class ResonatorModel
    {
        String,          // Plucked string (Rings default)
        Membrane,        // Drum/membrane
        Tube,            // Blown tube
        Bell,            // Metallic bell
        NumModels
    };
    
    struct ResonatorParams
    {
        float frequency = 440.0f;
        float brightness = 0.5f;    // Filter resonance/damping
        float damping = 0.5f;        // Decay time
        float position = 0.5f;       // Strike/pluck position
        float structure = 0.5f;      // Inharmonicity amount
        ResonatorModel model = ResonatorModel::String;
    };
    
    void setSampleRate(double sr)
    {
        sampleRate = sr;
        for (auto& mode : modes)
            mode.setSampleRate(sr);
    }
    
    void setResonatorModel(ResonatorModel model)
    {
        currentModel = model;
        updateModeFrequencies();
    }
    
    void setParameters(const ResonatorParams& p)
    {
        params = p;
        updateModeFrequencies();
    }
    
    void trigger(float velocity)
    {
        // Excite all modes with initial impulse
        float strikePosition = params.position;
        
        for (int i = 0; i < numModes; ++i)
        {
            // Position affects mode amplitude (like real strings)
            float positionGain = std::sin((i + 1) * juce::MathConstants<float>::pi * strikePosition);
            float amplitude = velocity * positionGain * (1.0f / (i + 1));
            
            modes[i].excite(amplitude);
        }
    }
    
    float processSample(float input)
    {
        float output = 0.0f;
        
        // Sum all modal responses
        for (int i = 0; i < numModes; ++i)
        {
            output += modes[i].process(input);
        }
        
        return output * 0.3f; // Scale to prevent clipping
    }
    
private:
    // Individual mode (harmonic/partial)
    class Mode
    {
    public:
        void setSampleRate(double sr)
        {
            sampleRate = sr;
        }
        
        void setFrequency(float freq)
        {
            frequency = freq;
            updateCoefficients();
        }
        
        void setDecay(float decay)
        {
            decayTime = decay;
            updateCoefficients();
        }
        
        void setBrightness(float bright)
        {
            brightness = bright;
            updateCoefficients();
        }
        
        void excite(float amplitude)
        {
            z1 += amplitude;
        }
        
        float process(float input)
        {
            // Resonant filter (like a tuned bandpass)
            float output = b0 * input + b1 * z1 + b2 * z2 - a1 * out1 - a2 * out2;
            
            // Update delays
            z2 = z1;
            z1 = input;
            out2 = out1;
            out1 = output;
            
            return output;
        }
        
    private:
        double sampleRate = 44100.0;
        float frequency = 440.0f;
        float decayTime = 1.0f;
        float brightness = 0.5f;
        
        // Filter coefficients
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        
        // State variables
        float z1 = 0.0f, z2 = 0.0f;
        float out1 = 0.0f, out2 = 0.0f;
        
        void updateCoefficients()
        {
            // Bandpass resonator with decay
            float omega = juce::MathConstants<float>::twoPi * frequency / sampleRate;
            float bandwidth = frequency / (10.0f + brightness * 90.0f);
            float bw = juce::MathConstants<float>::twoPi * bandwidth / sampleRate;
            
            // Decay coefficient
            float decay = std::exp(-1.0f / (decayTime * sampleRate));
            
            // Bandpass coefficients with resonance
            float r = decay;
            b0 = (1.0f - r * r) * std::sin(bw);
            b1 = 0.0f;
            b2 = 0.0f;
            a1 = -2.0f * r * std::cos(omega);
            a2 = r * r;
        }
    };
    
    static constexpr int numModes = 8; // 8 harmonics/partials
    std::array<Mode, numModes> modes;
    
    ResonatorParams params;
    ResonatorModel currentModel = ResonatorModel::String;
    double sampleRate = 44100.0;
    
    void updateModeFrequencies()
    {
        float baseFreq = params.frequency;
        float inharmonicity = params.structure;
        
        for (int i = 0; i < numModes; ++i)
        {
            float harmonic = i + 1;
            
            // Different harmonic series for different models
            float modeFreq = baseFreq;
            
            switch (currentModel)
            {
                case ResonatorModel::String:
                    // Nearly harmonic with slight inharmonicity
                    modeFreq = baseFreq * harmonic * (1.0f + inharmonicity * 0.02f * harmonic * harmonic);
                    break;

                case ResonatorModel::Membrane:
                    // Drum-like inharmonic ratios
                    modeFreq = baseFreq * std::sqrt(harmonic) * (1.0f + inharmonicity);
                    break;

                case ResonatorModel::Tube:
                    // Odd harmonics only (like clarinet)
                    modeFreq = baseFreq * (2 * harmonic - 1);
                    break;

                case ResonatorModel::Bell:
                {
                    // Highly inharmonic (metallic)
                    float bellRatios[] = {1.0f, 2.76f, 5.4f, 8.93f, 13.34f, 18.64f, 24.8f, 31.87f};
                    modeFreq = baseFreq * bellRatios[i] * (1.0f + inharmonicity * 0.1f);
                    break;
                }

                case ResonatorModel::NumModels:
                    // This case should not be reached
                    break;
            }
            
            modes[i].setFrequency(modeFreq);
            modes[i].setDecay(params.damping * 2.0f);
            modes[i].setBrightness(params.brightness);
        }
    }
};

//==============================================================================
// MUTABLE INSTRUMENTS CLOUDS - GRANULAR ENGINE
//==============================================================================
class GranularEngine
{
public:
    struct Grain
    {
        bool active = false;
        float position = 0.0f;      // Position in buffer
        float phase = 0.0f;         // Grain playback phase
        float duration = 0.1f;      // Grain length
        float pitch = 1.0f;         // Playback speed
        float pan = 0.5f;           // Stereo position
        float amplitude = 1.0f;     // Grain volume
    };
    
    struct CloudsParams
    {
        float position = 0.5f;      // Where to read from buffer
        float size = 0.5f;          // Grain size
        float density = 0.5f;       // Grains per second
        float texture = 0.5f;       // Grain overlap/randomness
        float pitch = 0.0f;         // Pitch shift (-12 to +12 semitones)
        float feedback = 0.0f;      // Granular feedback
        float reverb = 0.3f;        // Internal reverb
        float stereoSpread = 0.5f;  // Stereo width
        bool freeze = false;        // Freeze input
    };
    
    GranularEngine()
    {
        // Allocate 4 second buffer at 48kHz
        inputBuffer.setSize(2, 48000 * 4);
        inputBuffer.clear();
    }
    
    void setSampleRate(double sr)
    {
        sampleRate = sr;
    }
    
    void setParameters(const CloudsParams& p)
    {
        params = p;
    }
    
    void writeInput(float leftSample, float rightSample)
    {
        if (params.freeze)
            return; // Don't update buffer when frozen
        
        inputBuffer.setSample(0, writePos, leftSample);
        inputBuffer.setSample(1, writePos, rightSample);
        
        writePos = (writePos + 1) % inputBuffer.getNumSamples();
    }
    
    void processStereo(float& left, float& right)
    {
        // Grain density determines spawn rate
        float spawnProbability = params.density * 0.02f; // Adjusted for sample rate
        
        if (random.nextFloat() < spawnProbability)
        {
            spawnGrain();
        }
        
        // Process all active grains
        left = 0.0f;
        right = 0.0f;
        int activeCount = 0;
        
        for (auto& grain : grains)
        {
            if (!grain.active)
                continue;
            
            // Read from input buffer at grain's position
            float readPos = params.position * inputBuffer.getNumSamples() + 
                          grain.position * inputBuffer.getNumSamples() * 0.1f;
            
            // Handle pitch shifting
            readPos += grain.phase * grain.pitch * inputBuffer.getNumSamples();
            
            int pos = (int)readPos % inputBuffer.getNumSamples();
            float frac = readPos - (int)readPos;
            
            // Linear interpolation
            int nextPos = (pos + 1) % inputBuffer.getNumSamples();
            float sampleL = inputBuffer.getSample(0, pos) * (1 - frac) + 
                          inputBuffer.getSample(0, nextPos) * frac;
            float sampleR = inputBuffer.getSample(1, pos) * (1 - frac) + 
                          inputBuffer.getSample(1, nextPos) * frac;
            
            // Apply grain envelope (Hann window)
            float env = 0.5f * (1.0f - std::cos(grain.phase * juce::MathConstants<float>::twoPi));
            
            // Apply amplitude
            sampleL *= env * grain.amplitude;
            sampleR *= env * grain.amplitude;
            
            // Pan
            float leftGain = std::cos(grain.pan * juce::MathConstants<float>::halfPi);
            float rightGain = std::sin(grain.pan * juce::MathConstants<float>::halfPi);
            
            left += sampleL * leftGain;
            right += sampleR * rightGain;
            
            // Update grain
            grain.phase += 1.0f / (grain.duration * sampleRate);
            
            if (grain.phase >= 1.0f)
                grain.active = false;
            else
                activeCount++;
        }
        
        // Normalize by active grain count
        if (activeCount > 0)
        {
            float norm = 1.0f / std::sqrt(activeCount);
            left *= norm;
            right *= norm;
        }
    }
    
private:
    juce::AudioBuffer<float> inputBuffer;
    std::vector<Grain> grains{64}; // Up to 64 simultaneous grains
    CloudsParams params;
    juce::Random random;
    double sampleRate = 44100.0;
    int writePos = 0;
    
    void spawnGrain()
    {
        for (auto& grain : grains)
        {
            if (grain.active)
                continue;
            
            grain.active = true;
            grain.phase = 0.0f;
            
            // Randomize position based on texture
            float positionSpread = params.texture * 0.2f;
            grain.position = (random.nextFloat() - 0.5f) * positionSpread;
            
            // Grain size from params
            grain.duration = 0.01f + params.size * 0.5f; // 10ms to 510ms
            
            // Pitch from params with slight randomization
            float pitchSemitones = params.pitch * 12.0f;
            pitchSemitones += (random.nextFloat() - 0.5f) * params.texture * 2.0f;
            grain.pitch = std::pow(2.0f, pitchSemitones / 12.0f);
            
            // Random pan based on stereo spread
            grain.pan = 0.5f + (random.nextFloat() - 0.5f) * params.stereoSpread;
            
            // Random amplitude variation
            grain.amplitude = 0.8f + random.nextFloat() * 0.4f;
            
            break;
        }
    }
};

//==============================================================================
// ADVANCED WAVETABLE ENGINE (Pigments-style)
//==============================================================================
class AdvancedWavetableEngine
{
public:
    AdvancedWavetableEngine()
    {
        generateWavetables();
    }
    
    struct WavetableParams
    {
        int tableA = 0;
        int tableB = 1;
        float morph = 0.0f;      // Crossfade between tables
        float warp = 0.0f;       // Waveform warping
        float fold = 0.0f;       // Wavefold distortion
        float formant = 0.0f;    // Formant shift
    };
    
    float getSample(float phase, const WavetableParams& params)
    {
        // Read from both wavetables
        float sampleA = readTable(phase, params.tableA);
        float sampleB = readTable(phase, params.tableB);
        
        // Morph between them
        float morphed = sampleA + params.morph * (sampleB - sampleA);
        
        // Apply warp (phase distortion)
        if (params.warp != 0.0f)
        {
            float warpedPhase = phase + params.warp * std::sin(phase * juce::MathConstants<float>::twoPi);
            warpedPhase = std::fmod(warpedPhase, 1.0f);
            if (warpedPhase < 0) warpedPhase += 1.0f;
            morphed = readTable(warpedPhase, params.tableA);
        }
        
        // Apply wavefold
        if (params.fold > 0.0f)
        {
            float foldAmount = 1.0f + params.fold * 8.0f;
            morphed = std::sin(morphed * foldAmount);
        }
        
        return morphed;
    }
    
    int getNumTables() const { return (int)wavetables.size(); }
    
private:
    static constexpr int tableSize = 2048;
    std::vector<std::array<float, tableSize>> wavetables;
    
    float readTable(float phase, int tableIndex)
    {
        tableIndex = juce::jlimit(0, (int)wavetables.size() - 1, tableIndex);
        
        float pos = phase * tableSize;
        int index1 = (int)pos % tableSize;
        int index2 = (index1 + 1) % tableSize;
        float frac = pos - (int)pos;
        
        return wavetables[tableIndex][index1] + frac * 
              (wavetables[tableIndex][index2] - wavetables[tableIndex][index1]);
    }
    
    void generateWavetables()
    {
        wavetables.resize(32); // 32 wavetables for morphing
        
        for (int table = 0; table < 32; ++table)
        {
            float tablePos = table / 31.0f;
            
            for (int i = 0; i < tableSize; ++i)
            {
                float phase = i / float(tableSize);
                float sample = 0.0f;
                
                // Morph through different harmonic content
                int numHarmonics = 1 + (int)(tablePos * 16);
                
                for (int h = 1; h <= numHarmonics; ++h)
                {
                    float amplitude = 1.0f / h;
                    
                    // Add inharmonicity based on table position
                    float freqMult = h * (1.0f + tablePos * 0.1f * h);
                    
                    sample += amplitude * std::sin(freqMult * phase * juce::MathConstants<float>::twoPi);
                }
                
                wavetables[table][i] = sample / numHarmonics;
            }
        }
    }
};

//==============================================================================
// KARPLUS-STRONG ALGORITHM (Physical modeling pluck)
//==============================================================================
class KarplusStrongEngine
{
public:
    void setSampleRate(double sr)
    {
        sampleRate = sr;
    }
    
    void setFrequency(float freq)
    {
        frequency = freq;
        delayLength = sampleRate / frequency;
        
        // Resize delay line
        delayLine.resize((int)delayLength + 1, 0.0f);
        writePos = 0;
    }
    
    void trigger(float velocity)
    {
        // Fill delay line with noise burst
        for (size_t i = 0; i < delayLine.size(); ++i)
        {
            delayLine[i] = (random.nextFloat() * 2.0f - 1.0f) * velocity;
        }
    }
    
    float getSample()
    {
        // Read from delay line
        float output = delayLine[writePos];
        
        // Karplus-Strong averaging filter
        int prevPos = (writePos - 1 + delayLine.size()) % delayLine.size();
        float averaged = (delayLine[writePos] + delayLine[prevPos]) * 0.5f;
        
        // Apply damping
        averaged *= 0.995f; // Slight decay
        
        // Write back
        delayLine[writePos] = averaged;
        
        // Advance write position
        writePos = (writePos + 1) % delayLine.size();
        
        return output;
    }
    
private:
    std::vector<float> delayLine;
    double sampleRate = 44100.0;
    float frequency = 440.0f;
    float delayLength = 100.0f;
    int writePos = 0;
    juce::Random random;
};

//==============================================================================
// MODULATION MATRIX (Pigments-style)
//==============================================================================
class ModulationMatrix
{
public:
    enum class Source
    {
        LFO1, LFO2, LFO3,
        Envelope1, Envelope2, Envelope3,
        Velocity, Aftertouch, ModWheel,
        Random,
        NumSources
    };
    
    enum class Destination
    {
        Osc1Pitch, Osc2Pitch,
        Osc1Morph, Osc2Morph,
        FilterCutoff, FilterResonance,
        GrainPosition, GrainSize, GrainDensity,
        RingsPosition, RingsDamping, RingsBrightness,
        EffectMix,
        NumDestinations
    };
    
    struct Connection
    {
        Source source;
        Destination dest;
        float amount = 0.0f;
        bool active = false;
    };
    
    void addConnection(Source src, Destination dst, float amt)
    {
        for (auto& conn : connections)
        {
            if (!conn.active)
            {
                conn.source = src;
                conn.dest = dst;
                conn.amount = amt;
                conn.active = true;
                break;
            }
        }
    }
    
    float getModulation(Destination dest) const
    {
        float total = 0.0f;
        
        for (const auto& conn : connections)
        {
            if (conn.active && conn.dest == dest)
            {
                float sourceValue = sourceValues[(int)conn.source];
                total += sourceValue * conn.amount;
            }
        }
        
        return juce::jlimit(-1.0f, 1.0f, total);
    }
    
    void setSourceValue(Source src, float value)
    {
        sourceValues[(int)src] = value;
    }
    
private:
    std::array<Connection, 64> connections; // Up to 64 mod connections
    std::array<float, (int)Source::NumSources> sourceValues{};
};

// Continue in next file...
