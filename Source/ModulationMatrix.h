#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Modulation Source
 * Represents anything that can modulate (LFOs, Envelopes, Velocity, etc.)
 */
struct ModulationSource
{
    enum class Type
    {
        LFO1,
        LFO2,
        LFO3,
        Envelope1,
        Envelope2,
        Velocity,
        Aftertouch,
        ModWheel,
        PitchBend,
        Random
    };
    
    Type type;
    juce::String name;
    juce::Colour color;
    
    ModulationSource(Type t, const juce::String& n, juce::Colour c)
        : type(t), name(n), color(c) {}
};

/**
 * Modulation Destination
 * Represents any parameter that can be modulated
 */
struct ModulationDestination
{
    enum class Type
    {
        // Filter
        FilterCutoff,
        FilterResonance,
        
        // Grains (Clouds)
        GrainDensity,
        GrainSize,
        GrainPitch,
        GrainPosition,
        CloudsTexture,
        CloudsBlend,
        
        // Rings
        RingsStructure,
        RingsBrightness,
        RingsDamping,
        RingsPosition,
        
        // Wavetable
        WavetablePosition,
        WavetableMorph,
        
        // Oscillator
        OscillatorPitch,
        OscillatorDetune,
        OscillatorLevel,
        
        // Effects
        DelayTime,
        DelayFeedback,
        ReverbSize,
        ReverbDamping,
        DistortionAmount,
        
        // Global
        Volume,
        Pan
    };
    
    Type type;
    juce::String name;
    juce::String category;
    
    ModulationDestination(Type t, const juce::String& n, const juce::String& cat)
        : type(t), name(n), category(cat) {}
};

/**
 * Modulation Connection
 * A single source → destination routing with amount
 */
struct ModulationConnection
{
    ModulationSource::Type source;
    ModulationDestination::Type destination;
    float amount = 0.5f;  // -1 to 1
    bool enabled = true;
    juce::Colour connectionColor;
    
    ModulationConnection(ModulationSource::Type src, 
                        ModulationDestination::Type dest,
                        float amt = 0.5f)
        : source(src), destination(dest), amount(amt)
    {
        // Color based on source
        connectionColor = getSourceColor(src);
    }
    
    static juce::Colour getSourceColor(ModulationSource::Type src)
    {
        switch (src)
        {
            case ModulationSource::Type::LFO1: return juce::Colour(0xffffb3d9); // Pastel pink
            case ModulationSource::Type::LFO2: return juce::Colour(0xffa8ffb4); // Pastel green
            case ModulationSource::Type::LFO3: return juce::Colour(0xffd8b5ff); // Pastel purple
            case ModulationSource::Type::Envelope1: return juce::Colour(0xffffccf2); // Light pink
            case ModulationSource::Type::Envelope2: return juce::Colour(0xffc8ffcc); // Light green
            case ModulationSource::Type::Velocity: return juce::Colour(0xffffb3d9); // Pink
            case ModulationSource::Type::Aftertouch: return juce::Colour(0xffe8d5ff); // Light purple
            case ModulationSource::Type::ModWheel: return juce::Colour(0xffa8ffb4); // Green
            case ModulationSource::Type::PitchBend: return juce::Colour(0xffd8b5ff); // Purple
            case ModulationSource::Type::Random: return juce::Colour(0xffffffff);
            default: return juce::Colour(0xffd8b5ff);
        }
    }
};

/**
 * Advanced Modulation Matrix Engine
 * Manages all modulation routings and calculations
 */
class AdvancedModulationMatrix
{
public:
    AdvancedModulationMatrix()
    {
        setupSources();
        setupDestinations();
    }
    
    // Add a new modulation connection
    void addConnection(ModulationSource::Type source, 
                      ModulationDestination::Type destination,
                      float amount = 0.5f)
    {
        // Check if connection already exists
        for (auto& conn : connections)
        {
            if (conn.source == source && conn.destination == destination)
            {
                conn.amount = amount;
                conn.enabled = true;
                return;
            }
        }
        
        // Add new connection
        connections.emplace_back(source, destination, amount);
    }
    
    // Remove a connection
    void removeConnection(ModulationSource::Type source, 
                         ModulationDestination::Type destination)
    {
        connections.erase(
            std::remove_if(connections.begin(), connections.end(),
                [source, destination](const ModulationConnection& conn) {
                    return conn.source == source && conn.destination == destination;
                }),
            connections.end()
        );
    }
    
    // Set source value (called from audio processor)
    void setSourceValue(ModulationSource::Type source, float value)
    {
        sourceValues[static_cast<int>(source)] = value;
    }
    
    // Get modulated value for a destination
    float getModulatedValue(ModulationDestination::Type destination, float baseValue)
    {
        float modulationSum = 0.0f;
        
        for (const auto& conn : connections)
        {
            if (conn.destination == destination && conn.enabled)
            {
                float sourceValue = sourceValues[static_cast<int>(conn.source)];
                modulationSum += sourceValue * conn.amount;
            }
        }
        
        // Apply modulation to base value
        return baseValue + (modulationSum * baseValue); // Multiplicative
    }
    
    // Get all connections
    const std::vector<ModulationConnection>& getConnections() const
    {
        return connections;
    }
    
    // Get connection amount
    float getConnectionAmount(ModulationSource::Type source, 
                             ModulationDestination::Type destination) const
    {
        for (const auto& conn : connections)
        {
            if (conn.source == source && conn.destination == destination)
                return conn.amount;
        }
        return 0.0f;
    }
    
    // Check if connection exists
    bool hasConnection(ModulationSource::Type source, 
                      ModulationDestination::Type destination) const
    {
        for (const auto& conn : connections)
        {
            if (conn.source == source && conn.destination == destination)
                return true;
        }
        return false;
    }
    
    // Get all sources
    const std::vector<ModulationSource>& getSources() const
    {
        return sources;
    }
    
    // Get all destinations
    const std::vector<ModulationDestination>& getDestinations() const
    {
        return destinations;
    }
    
    // Clear all connections
    void clearAllConnections()
    {
        connections.clear();
    }
    
private:
    void setupSources()
    {
        sources = {
            { ModulationSource::Type::LFO1, "LFO 1", juce::Colour(0xffffb3d9) }, // Pastel pink
            { ModulationSource::Type::LFO2, "LFO 2", juce::Colour(0xffa8ffb4) }, // Pastel green
            { ModulationSource::Type::LFO3, "LFO 3", juce::Colour(0xffd8b5ff) }, // Pastel purple
            { ModulationSource::Type::Envelope1, "ENV 1", juce::Colour(0xffffccf2) }, // Light pink
            { ModulationSource::Type::Envelope2, "ENV 2", juce::Colour(0xffc8ffcc) }, // Light green
            { ModulationSource::Type::Velocity, "Velocity", juce::Colour(0xffffb3d9) }, // Pink
            { ModulationSource::Type::Aftertouch, "Aftertouch", juce::Colour(0xffe8d5ff) }, // Light purple
            { ModulationSource::Type::ModWheel, "Mod Wheel", juce::Colour(0xffa8ffb4) }, // Green
            { ModulationSource::Type::PitchBend, "Pitch Bend", juce::Colour(0xffd8b5ff) }, // Purple
            { ModulationSource::Type::Random, "Random", juce::Colour(0xffffffff) }
        };
    }
    
    void setupDestinations()
    {
        destinations = {
            // Filter
            { ModulationDestination::Type::FilterCutoff, "Cutoff", "Filter" },
            { ModulationDestination::Type::FilterResonance, "Resonance", "Filter" },
            
            // Grains
            { ModulationDestination::Type::GrainDensity, "Density", "Grains" },
            { ModulationDestination::Type::GrainSize, "Size", "Grains" },
            { ModulationDestination::Type::GrainPitch, "Pitch", "Grains" },
            { ModulationDestination::Type::GrainPosition, "Position", "Grains" },
            { ModulationDestination::Type::CloudsTexture, "Texture", "Clouds" },
            { ModulationDestination::Type::CloudsBlend, "Blend", "Clouds" },
            
            // Rings
            { ModulationDestination::Type::RingsStructure, "Structure", "Rings" },
            { ModulationDestination::Type::RingsBrightness, "Brightness", "Rings" },
            { ModulationDestination::Type::RingsDamping, "Damping", "Rings" },
            { ModulationDestination::Type::RingsPosition, "Position", "Rings" },
            
            // Wavetable
            { ModulationDestination::Type::WavetablePosition, "Position", "Wavetable" },
            { ModulationDestination::Type::WavetableMorph, "Morph", "Wavetable" },
            
            // Oscillator
            { ModulationDestination::Type::OscillatorPitch, "Pitch", "Oscillator" },
            { ModulationDestination::Type::OscillatorDetune, "Detune", "Oscillator" },
            { ModulationDestination::Type::OscillatorLevel, "Level", "Oscillator" },
            
            // Effects
            { ModulationDestination::Type::DelayTime, "Time", "Delay" },
            { ModulationDestination::Type::DelayFeedback, "Feedback", "Delay" },
            { ModulationDestination::Type::ReverbSize, "Size", "Reverb" },
            { ModulationDestination::Type::ReverbDamping, "Damping", "Reverb" },
            { ModulationDestination::Type::DistortionAmount, "Amount", "Distortion" },
            
            // Global
            { ModulationDestination::Type::Volume, "Volume", "Global" },
            { ModulationDestination::Type::Pan, "Pan", "Global" }
        };
    }
    
    std::vector<ModulationSource> sources;
    std::vector<ModulationDestination> destinations;
    std::vector<ModulationConnection> connections;
    std::array<float, 10> sourceValues = { 0.0f }; // Store current values
};
