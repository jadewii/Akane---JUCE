#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>
#include <functional>

/**
 * Preset Data
 */
struct Preset
{
    juce::String name;
    juce::String author;
    juce::String category;
    juce::StringArray tags;
    juce::ValueTree state;
    int rating = 0;  // 0-5 stars
    bool isFavorite = false;
    bool isFactory = false;  // NEW: Mark as factory preset (locked/read-only)
    juce::String description;
    juce::Time dateCreated;

    Preset() = default;

    Preset(const juce::String& n, const juce::String& cat, const juce::ValueTree& s, bool factory = false)
        : name(n), category(cat), state(s), isFactory(factory), dateCreated(juce::Time::getCurrentTime()) {}
    
    bool matchesSearch(const juce::String& searchText) const
    {
        if (searchText.isEmpty())
            return true;
        
        juce::String search = searchText.toLowerCase();
        
        if (name.toLowerCase().contains(search))
            return true;
        
        if (category.toLowerCase().contains(search))
            return true;
        
        if (author.toLowerCase().contains(search))
            return true;
        
        for (const auto& tag : tags)
        {
            if (tag.toLowerCase().contains(search))
                return true;
        }
        
        return false;
    }
    
    bool matchesCategory(const juce::String& cat) const
    {
        if (cat.isEmpty() || cat == "All")
            return true;
        return category == cat;
    }
    
    bool matchesTags(const juce::StringArray& requiredTags) const
    {
        if (requiredTags.isEmpty())
            return true;
        
        for (const auto& required : requiredTags)
        {
            if (!tags.contains(required))
                return false;
        }
        return true;
    }
};

/**
 * Preset Manager
 */
class PresetManager
{
public:
    PresetManager(juce::AudioProcessorValueTreeState& apvts) : parameters(apvts)
    {
        initializeCategories();
        loadPresetsFromDisk();

        // Create factory presets if no factory presets exist
        bool hasFactoryPresets = false;
        for (const auto& preset : presets)
        {
            if (preset.isFactory)
            {
                hasFactoryPresets = true;
                break;
            }
        }

        if (!hasFactoryPresets)
        {
            createFactoryPresets();
        }
    }

    // Callback for voice reset when loading presets (eliminates pops/clicks)
    std::function<void()> onPresetChange;
    
    void savePreset(const juce::String& name, const juce::String& category)
    {
        auto state = parameters.copyState();

        // Validate state before saving
        if (!state.isValid() || state.getNumChildren() == 0)
        {
            return;
        }

        Preset preset(name, category, state);
        preset.author = juce::SystemStats::getFullUserName();
        presets.push_back(preset);
        savePresetsToDisk();
    }
    
    void loadPreset(int index)
    {
        if (index >= 0 && index < presets.size())
        {
            // VALIDATE: Don't load empty presets
            if (!presets[index].state.isValid() || presets[index].state.getNumChildren() == 0)
            {
                return;
            }

            // ELIMINATE PRESET CHANGE POPS: Reset all voices before parameter change
            if (onPresetChange)
                onPresetChange();

            // REAL-TIME SAFE PRESET LOADING:
            // Load preset parameters (voices have been reset above)
            parameters.replaceState(presets[index].state);
            currentPresetIndex = index;
        }
    }

    void loadPresetSilently(int index)
    {
        if (index >= 0 && index < presets.size())
        {
            // VALIDATE: Don't load empty presets
            if (!presets[index].state.isValid() || presets[index].state.getNumChildren() == 0)
            {
                return;
            }

            // Load without triggering audio or debug output
            parameters.replaceState(presets[index].state);
            currentPresetIndex = index;
        }
    }
    
    void deletePreset(int index, bool developmentMode = false)
    {
        if (index >= 0 && index < presets.size())
        {
            // Don't allow deleting factory presets unless in development mode
            if (presets[index].isFactory && !developmentMode)
            {
                return;
            }

            presets.erase(presets.begin() + index);
            savePresetsToDisk();
        }
    }
    
    void setFavorite(int index, bool favorite)
    {
        if (index >= 0 && index < presets.size())
        {
            presets[index].isFavorite = favorite;
            savePresetsToDisk();
        }
    }
    
    void setRating(int index, int rating)
    {
        if (index >= 0 && index < presets.size())
        {
            presets[index].rating = juce::jlimit(0, 5, rating);
            savePresetsToDisk();
        }
    }
    
    std::vector<Preset> searchPresets(const juce::String& searchText,
                                     const juce::String& category,
                                     const juce::StringArray& tags,
                                     bool favoritesOnly = false,
                                     bool factoryOnly = false,
                                     bool userOnly = false)
    {
        std::vector<Preset> results;

        for (const auto& preset : presets)
        {
            // Filter by factory/user
            if (factoryOnly && !preset.isFactory)
                continue;
            if (userOnly && preset.isFactory)
                continue;

            if (favoritesOnly && !preset.isFavorite)
                continue;

            if (!preset.matchesSearch(searchText))
                continue;

            if (!preset.matchesCategory(category))
                continue;

            if (!preset.matchesTags(tags))
                continue;

            results.push_back(preset);
        }

        return results;
    }
    
    const std::vector<Preset>& getAllPresets() const { return presets; }

    // Public method to create factory presets - call this after APVTS is fully initialized
    void ensureFactoryPresetsExist()
    {
        if (presets.empty())
        {
            createFactoryPresets();
        }
    }

    juce::StringArray getCategories() const { return categories; }
    juce::StringArray getAllTags() const
    {
        juce::StringArray allTags;
        for (const auto& preset : presets)
        {
            for (const auto& tag : preset.tags)
            {
                if (!allTags.contains(tag))
                    allTags.add(tag);
            }
        }
        return allTags;
    }
    
    int getCurrentPresetIndex() const { return currentPresetIndex; }

    juce::StringArray getPresetNames() const
    {
        juce::StringArray names;
        for (const auto& preset : presets)
        {
            names.add(preset.name);
        }
        return names;
    }

private:
    void loadPresetsFromDisk()
    {
        auto presetDir = getPresetDirectory();
        if (!presetDir.exists())
            presetDir.createDirectory();
        
        // Load preset files (.xml)
        for (const auto& file : presetDir.findChildFiles(juce::File::findFiles, false, "*.xml"))
        {
            auto xml = juce::XmlDocument::parse(file);
            if (xml != nullptr)
            {
                Preset preset;
                preset.name = xml->getStringAttribute("name");
                preset.category = xml->getStringAttribute("category");
                preset.author = xml->getStringAttribute("author");
                preset.rating = xml->getIntAttribute("rating");
                preset.isFavorite = xml->getBoolAttribute("favorite");
                preset.isFactory = xml->getBoolAttribute("factory", false);

                if (auto* tagsElement = xml->getChildByName("tags"))
                {
                    for (auto* tagElement : tagsElement->getChildIterator())
                    {
                        preset.tags.add(tagElement->getAllSubText());
                    }
                }
                
                // Try both "state" and "Parameters" element names for compatibility
                if (auto* stateElement = xml->getChildByName("state"))
                {
                    preset.state = juce::ValueTree::fromXml(*stateElement);
                }
                else if (auto* paramsElement = xml->getChildByName("Parameters"))
                {
                    preset.state = juce::ValueTree::fromXml(*paramsElement);
                }
                
                presets.push_back(preset);
            }
        }
    }
    
    void savePresetsToDisk()
    {
        auto presetDir = getPresetDirectory();
        
        for (const auto& preset : presets)
        {
            auto xml = std::make_unique<juce::XmlElement>("Preset");
            xml->setAttribute("name", preset.name);
            xml->setAttribute("category", preset.category);
            xml->setAttribute("author", preset.author);
            xml->setAttribute("rating", preset.rating);
            xml->setAttribute("favorite", preset.isFavorite);
            xml->setAttribute("factory", preset.isFactory);

            auto* tagsElement = xml->createNewChildElement("tags");
            for (const auto& tag : preset.tags)
            {
                tagsElement->createNewChildElement("tag")->addTextElement(tag);
            }
            
            xml->addChildElement(preset.state.createXml().release());
            
            juce::String filename = preset.name.replaceCharacter(' ', '_') + ".xml";
            auto file = presetDir.getChildFile(filename);
            xml->writeTo(file);
        }
    }
    
    juce::File getPresetDirectory()
    {
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("WiiPluck")
            .getChildFile("Presets");
    }
    
    void initializeCategories()
    {
        categories = { "All", "Bass", "Lead", "Pad", "Pluck", "FX", "Arp", "Keys", "Mallet", "Bells", "Ethnic" };
    }

    void createFactoryPresets()
    {
        // CRITICAL: Check if APVTS is ready before creating presets
        auto testState = parameters.copyState();
        if (!testState.isValid() || testState.getNumChildren() == 0)
        {
            return;
        }

        // Helper lambda to set parameter value in ValueTree
        auto setParam = [](juce::ValueTree& state, const juce::String& paramID, float value) {
            for (int i = 0; i < state.getNumChildren(); ++i)
            {
                auto child = state.getChild(i);
                if (child.getProperty("id") == paramID)
                {
                    child.setProperty("value", value, nullptr);
                    return;
                }
            }
        };

        // =================================================================
        // PADS (5)
        // =================================================================

        // PAD 1: Ethereal Pad
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);  // Basic Oscillator
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Octave", 1.0f);  // One octave up
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.4f);
            setParam(state, "attack", 0.8f);
            setParam(state, "decay", 0.3f);
            setParam(state, "sustain", 0.85f);
            setParam(state, "release", 1.2f);
            setParam(state, "filterCutoff", 3500.0f);
            setParam(state, "filterResonance", 0.2f);
            setParam(state, "filterEnv", 0.3f);
            setParam(state, "reverbSize", 0.75f);
            setParam(state, "reverbMix", 0.45f);
            setParam(state, "reverbShimmer", 0.3f);

            Preset preset("Ethereal Pad", "Pad", state, true);  // true = factory preset
            preset.author = "Factory";
            preset.tags.add("warm");
            preset.tags.add("ambient");
            preset.tags.add("soft");
            preset.description = "Soft ethereal pad perfect for ambient soundscapes";
            presets.push_back(preset);
        }

        // PAD 2: Warm Pad
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.7f);
            setParam(state, "osc2Wave", 1.0f);  // Saw
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.6f);
            setParam(state, "attack", 0.5f);
            setParam(state, "decay", 0.4f);
            setParam(state, "sustain", 0.75f);
            setParam(state, "release", 1.0f);
            setParam(state, "filterCutoff", 2800.0f);
            setParam(state, "filterResonance", 0.35f);
            setParam(state, "filterEnv", 0.4f);
            setParam(state, "reverbSize", 0.65f);
            setParam(state, "reverbMix", 0.35f);

            Preset preset("Warm Pad", "Pad", state, true);
            preset.author = "Factory";
            preset.tags.add("warm");
            preset.tags.add("rich");
            preset.tags.add("analog");
            preset.description = "Rich warm pad with classic analog character";
            presets.push_back(preset);
        }

        // PAD 3: Cathedral Pad
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Octave", -1.0f);  // One octave down
            setParam(state, "osc1Mix", 0.5f);
            setParam(state, "osc2Wave", 0.0f);  // Sine
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.5f);
            setParam(state, "attack", 1.0f);
            setParam(state, "decay", 0.5f);
            setParam(state, "sustain", 0.9f);
            setParam(state, "release", 1.5f);
            setParam(state, "filterCutoff", 4000.0f);
            setParam(state, "filterResonance", 0.25f);
            setParam(state, "filterEnv", 0.2f);
            setParam(state, "reverbSize", 0.9f);
            setParam(state, "reverbMix", 0.55f);
            setParam(state, "reverbShimmer", 0.5f);

            Preset preset("Cathedral Pad", "Pad", state, true);
            preset.author = "Factory";
            preset.tags.add("spacious");
            preset.tags.add("reverb");
            preset.tags.add("atmospheric");
            preset.description = "Spacious cathedral reverb with shimmer";
            presets.push_back(preset);
        }

        // PAD 4: Analog Pad
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.65f);
            setParam(state, "osc2Wave", 4.0f);  // Pulse
            setParam(state, "osc2PW", 0.3f);  // Narrow pulse
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.55f);
            setParam(state, "attack", 0.6f);
            setParam(state, "decay", 0.35f);
            setParam(state, "sustain", 0.8f);
            setParam(state, "release", 0.9f);
            setParam(state, "filterCutoff", 3200.0f);
            setParam(state, "filterResonance", 0.4f);
            setParam(state, "filterEnv", 0.5f);
            setParam(state, "reverbSize", 0.55f);
            setParam(state, "reverbMix", 0.3f);

            Preset preset("Analog Pad", "Pad", state, true);
            preset.author = "Factory";
            preset.tags.add("analog");
            preset.tags.add("vintage");
            preset.tags.add("warm");
            preset.description = "Classic analog synth pad sound";
            presets.push_back(preset);
        }

        // PAD 5: Cosmic Pad
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 7.0f);  // Osc + Clouds
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Octave", 1.0f);
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.5f);
            setParam(state, "cloudsDensity", 0.6f);
            setParam(state, "cloudsSize", 0.7f);
            setParam(state, "cloudsTexture", 0.65f);
            setParam(state, "cloudsPitch", 0.0f);
            setParam(state, "grainsMix", 0.4f);
            setParam(state, "attack", 0.9f);
            setParam(state, "decay", 0.4f);
            setParam(state, "sustain", 0.85f);
            setParam(state, "release", 1.3f);
            setParam(state, "filterCutoff", 3800.0f);
            setParam(state, "filterResonance", 0.28f);
            setParam(state, "reverbSize", 0.7f);
            setParam(state, "reverbMix", 0.4f);

            Preset preset("Cosmic Pad", "Pad", state, true);
            preset.author = "Factory";
            preset.tags.add("textured");
            preset.tags.add("granular");
            preset.tags.add("evolving");
            preset.description = "Evolving cosmic pad with granular texture";
            presets.push_back(preset);
        }

        // =================================================================
        // PLUCKS (5)
        // =================================================================

        // PLUCK 1: Koto Pluck
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.5f);
            setParam(state, "ringsStructure", 0.38f);
            setParam(state, "ringsBrightness", 0.4f);
            setParam(state, "ringsDamping", 0.4f);
            setParam(state, "ringsMix", 0.4f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.4f);
            setParam(state, "sustain", 0.3f);
            setParam(state, "release", 0.6f);
            setParam(state, "filterCutoff", 8000.0f);
            setParam(state, "filterResonance", 0.25f);
            setParam(state, "reverbSize", 0.35f);
            setParam(state, "reverbMix", 0.2f);

            Preset preset("Koto Pluck", "Pluck", state, true);
            preset.author = "Factory";
            preset.tags.add("plucked");
            preset.tags.add("warm");
            preset.tags.add("ethnic");
            preset.description = "Warm koto pluck with hybrid synthesis";
            presets.push_back(preset);
        }

        // PLUCK 2: Glass Pluck
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Octave", 1.0f);
            setParam(state, "osc1Mix", 0.7f);
            setParam(state, "osc2Wave", 0.0f);  // Sine
            setParam(state, "osc2Octave", 2.0f);
            setParam(state, "osc2Mix", 0.3f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.25f);
            setParam(state, "sustain", 0.1f);
            setParam(state, "release", 0.4f);
            setParam(state, "filterCutoff", 10000.0f);
            setParam(state, "filterResonance", 0.3f);
            setParam(state, "reverbSize", 0.4f);
            setParam(state, "reverbMix", 0.25f);

            Preset preset("Glass Pluck", "Pluck", state, true);
            preset.author = "Factory";
            preset.tags.add("bright");
            preset.tags.add("bell");
            preset.tags.add("plucked");
            preset.description = "Bright crystalline pluck with fast decay";
            presets.push_back(preset);
        }

        // PLUCK 3: Dulcimer Pluck
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 4.0f);  // Pulse
            setParam(state, "osc1PW", 0.4f);
            setParam(state, "osc1Mix", 0.65f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.45f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.35f);
            setParam(state, "sustain", 0.25f);
            setParam(state, "release", 0.5f);
            setParam(state, "filterCutoff", 6500.0f);
            setParam(state, "filterResonance", 0.28f);
            setParam(state, "reverbSize", 0.38f);
            setParam(state, "reverbMix", 0.22f);

            Preset preset("Dulcimer Pluck", "Pluck", state, true);
            preset.author = "Factory";
            preset.tags.add("plucked");
            preset.tags.add("wooden");
            preset.tags.add("percussive");
            preset.description = "Hammered dulcimer with wooden character";
            presets.push_back(preset);
        }

        // PLUCK 4: Kalimba Pluck
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Octave", 2.0f);
            setParam(state, "osc2Mix", 0.2f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.2f);
            setParam(state, "sustain", 0.15f);
            setParam(state, "release", 0.35f);
            setParam(state, "filterCutoff", 7500.0f);
            setParam(state, "filterResonance", 0.32f);
            setParam(state, "reverbSize", 0.42f);
            setParam(state, "reverbMix", 0.28f);

            Preset preset("Kalimba Pluck", "Pluck", state, true);
            preset.author = "Factory";
            preset.tags.add("plucked");
            preset.tags.add("mellow");
            preset.tags.add("thumb-piano");
            preset.description = "Soft kalimba thumb piano sound";
            presets.push_back(preset);
        }

        // PLUCK 5: Harp Pluck
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.7f);
            setParam(state, "osc2Wave", 0.0f);  // Sine
            setParam(state, "osc2Octave", 1.0f);
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.4f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.45f);
            setParam(state, "sustain", 0.2f);
            setParam(state, "release", 0.7f);
            setParam(state, "filterCutoff", 8500.0f);
            setParam(state, "filterResonance", 0.26f);
            setParam(state, "reverbSize", 0.5f);
            setParam(state, "reverbMix", 0.32f);

            Preset preset("Harp Pluck", "Pluck", state, true);
            preset.author = "Factory";
            preset.tags.add("plucked");
            preset.tags.add("elegant");
            preset.tags.add("classical");
            preset.description = "Elegant harp with natural resonance";
            presets.push_back(preset);
        }

        // =================================================================
        // LEADS (4)
        // =================================================================

        // LEAD 1: Lush Lead
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.75f);
            setParam(state, "osc2Wave", 1.0f);  // Saw
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.65f);
            setParam(state, "attack", 0.05f);
            setParam(state, "decay", 0.3f);
            setParam(state, "sustain", 0.7f);
            setParam(state, "release", 0.4f);
            setParam(state, "filterCutoff", 4500.0f);
            setParam(state, "filterResonance", 0.45f);
            setParam(state, "filterEnv", 0.65f);
            setParam(state, "reverbSize", 0.45f);
            setParam(state, "reverbMix", 0.25f);

            Preset preset("Lush Lead", "Lead", state, true);
            preset.author = "Factory";
            preset.tags.add("lead");
            preset.tags.add("thick");
            preset.tags.add("analog");
            preset.description = "Thick lush lead with filter sweep";
            presets.push_back(preset);
        }

        // LEAD 2: Singing Lead
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 2.0f);  // Square
            setParam(state, "osc1Mix", 0.7f);
            setParam(state, "osc2Wave", 4.0f);  // Pulse
            setParam(state, "osc2PW", 0.25f);
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.5f);
            setParam(state, "attack", 0.08f);
            setParam(state, "decay", 0.25f);
            setParam(state, "sustain", 0.75f);
            setParam(state, "release", 0.35f);
            setParam(state, "filterCutoff", 3800.0f);
            setParam(state, "filterResonance", 0.5f);
            setParam(state, "filterEnv", 0.6f);
            setParam(state, "reverbSize", 0.4f);
            setParam(state, "reverbMix", 0.2f);

            Preset preset("Singing Lead", "Lead", state, true);
            preset.author = "Factory";
            preset.tags.add("lead");
            preset.tags.add("vocal");
            preset.tags.add("expressive");
            preset.description = "Expressive vocal-like lead sound";
            presets.push_back(preset);
        }

        // LEAD 3: Dreamy Lead
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.65f);
            setParam(state, "osc2Wave", 0.0f);  // Sine
            setParam(state, "osc2Octave", 1.0f);
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.55f);
            setParam(state, "attack", 0.15f);
            setParam(state, "decay", 0.3f);
            setParam(state, "sustain", 0.8f);
            setParam(state, "release", 0.6f);
            setParam(state, "filterCutoff", 5000.0f);
            setParam(state, "filterResonance", 0.35f);
            setParam(state, "filterEnv", 0.5f);
            setParam(state, "reverbSize", 0.6f);
            setParam(state, "reverbMix", 0.35f);

            Preset preset("Dreamy Lead", "Lead", state, true);
            preset.author = "Factory";
            preset.tags.add("lead");
            preset.tags.add("soft");
            preset.tags.add("ambient");
            preset.description = "Soft dreamy lead for melodic lines";
            presets.push_back(preset);
        }

        // LEAD 4: Electric Lead
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "osc2Wave", 2.0f);  // Square
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.6f);
            setParam(state, "attack", 0.01f);
            setParam(state, "decay", 0.2f);
            setParam(state, "sustain", 0.65f);
            setParam(state, "release", 0.3f);
            setParam(state, "filterCutoff", 5500.0f);
            setParam(state, "filterResonance", 0.55f);
            setParam(state, "filterEnv", 0.7f);
            setParam(state, "distDrive", 0.3f);
            setParam(state, "distMix", 0.2f);
            setParam(state, "reverbSize", 0.35f);
            setParam(state, "reverbMix", 0.15f);

            Preset preset("Electric Lead", "Lead", state, true);
            preset.author = "Factory";
            preset.tags.add("lead");
            preset.tags.add("edgy");
            preset.tags.add("aggressive");
            preset.description = "Edgy electric lead with bite";
            presets.push_back(preset);
        }

        // =================================================================
        // BASS (3)
        // =================================================================

        // BASS 1: Deep Bass
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Octave", -1.0f);
            setParam(state, "osc1Mix", 0.9f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Octave", -1.0f);
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.3f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.25f);
            setParam(state, "sustain", 0.5f);
            setParam(state, "release", 0.2f);
            setParam(state, "filterCutoff", 1500.0f);
            setParam(state, "filterResonance", 0.3f);
            setParam(state, "filterEnv", 0.4f);

            Preset preset("Deep Bass", "Bass", state, true);
            preset.author = "Factory";
            preset.tags.add("bass");
            preset.tags.add("sub");
            preset.tags.add("deep");
            preset.description = "Deep sub bass with sine wave foundation";
            presets.push_back(preset);
        }

        // BASS 2: Warm Bass
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Octave", -1.0f);
            setParam(state, "osc1Mix", 0.75f);
            setParam(state, "osc2Wave", 1.0f);  // Saw
            setParam(state, "osc2Octave", -1.0f);
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.65f);
            setParam(state, "attack", 0.005f);
            setParam(state, "decay", 0.3f);
            setParam(state, "sustain", 0.6f);
            setParam(state, "release", 0.25f);
            setParam(state, "filterCutoff", 2000.0f);
            setParam(state, "filterResonance", 0.4f);
            setParam(state, "filterEnv", 0.5f);

            Preset preset("Warm Bass", "Bass", state, true);
            preset.author = "Factory";
            preset.tags.add("bass");
            preset.tags.add("warm");
            preset.tags.add("analog");
            preset.description = "Warm analog bass with detuned saws";
            presets.push_back(preset);
        }

        // BASS 3: Growl Bass
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 4.0f);  // Pulse
            setParam(state, "osc1Octave", -1.0f);
            setParam(state, "osc1PW", 0.2f);
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "osc2Wave", 1.0f);  // Saw
            setParam(state, "osc2Octave", -1.0f);
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.6f);
            setParam(state, "attack", 0.002f);
            setParam(state, "decay", 0.2f);
            setParam(state, "sustain", 0.55f);
            setParam(state, "release", 0.2f);
            setParam(state, "filterCutoff", 1800.0f);
            setParam(state, "filterResonance", 0.6f);
            setParam(state, "filterEnv", 0.7f);

            Preset preset("Growl Bass", "Bass", state, true);
            preset.author = "Factory";
            preset.tags.add("bass");
            preset.tags.add("aggressive");
            preset.tags.add("resonant");
            preset.description = "Aggressive growling bass with heavy filter";
            presets.push_back(preset);
        }

        // =================================================================
        // KEYS (3)
        // =================================================================

        // KEYS 1: Warm Keys
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.7f);
            setParam(state, "osc2Wave", 1.0f);  // Saw
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.3f);
            setParam(state, "attack", 0.01f);
            setParam(state, "decay", 0.35f);
            setParam(state, "sustain", 0.6f);
            setParam(state, "release", 0.5f);
            setParam(state, "filterCutoff", 6000.0f);
            setParam(state, "filterResonance", 0.28f);
            setParam(state, "filterEnv", 0.35f);
            setParam(state, "reverbSize", 0.38f);
            setParam(state, "reverbMix", 0.18f);

            Preset preset("Warm Keys", "Keys", state, true);
            preset.author = "Factory";
            preset.tags.add("keys");
            preset.tags.add("warm");
            preset.tags.add("piano");
            preset.description = "Warm electric piano style keys";
            presets.push_back(preset);
        }

        // KEYS 2: Bell Piano
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Mix", 0.75f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Octave", 2.0f);
            setParam(state, "osc2Mix", 0.25f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.4f);
            setParam(state, "sustain", 0.4f);
            setParam(state, "release", 0.6f);
            setParam(state, "filterCutoff", 9000.0f);
            setParam(state, "filterResonance", 0.32f);
            setParam(state, "reverbSize", 0.45f);
            setParam(state, "reverbMix", 0.3f);

            Preset preset("Bell Piano", "Keys", state, true);
            preset.author = "Factory";
            preset.tags.add("keys");
            preset.tags.add("bell");
            preset.tags.add("bright");
            preset.description = "Bell-like electric piano with shimmer";
            presets.push_back(preset);
        }

        // KEYS 3: Vintage Keys
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 4.0f);  // Pulse
            setParam(state, "osc1PW", 0.45f);
            setParam(state, "osc1Mix", 0.65f);
            setParam(state, "osc2Wave", 1.0f);  // Saw
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.45f);
            setParam(state, "attack", 0.008f);
            setParam(state, "decay", 0.3f);
            setParam(state, "sustain", 0.55f);
            setParam(state, "release", 0.45f);
            setParam(state, "filterCutoff", 5500.0f);
            setParam(state, "filterResonance", 0.35f);
            setParam(state, "filterEnv", 0.4f);
            setParam(state, "reverbSize", 0.4f);
            setParam(state, "reverbMix", 0.22f);

            Preset preset("Vintage Keys", "Keys", state, true);
            preset.author = "Factory";
            preset.tags.add("keys");
            preset.tags.add("vintage");
            preset.tags.add("retro");
            preset.description = "Vintage analog synth keys sound";
            presets.push_back(preset);
        }

        // =================================================================
        // ASIAN PLUCKS (4) - Using Rings hybrid mode for authentic character
        // =================================================================

        // ASIAN 1: Koto (Japanese)
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings hybrid
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.5f);
            setParam(state, "osc2Wave", 0.0f);  // Sine
            setParam(state, "osc2Octave", 1.0f);
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.3f);
            setParam(state, "ringsStructure", 0.38f);
            setParam(state, "ringsBrightness", 0.42f);
            setParam(state, "ringsDamping", 0.35f);
            setParam(state, "ringsPosition", 0.4f);
            setParam(state, "ringsMix", 0.45f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.4f);
            setParam(state, "sustain", 0.3f);
            setParam(state, "release", 0.6f);
            setParam(state, "filterCutoff", 8000.0f);
            setParam(state, "filterResonance", 0.25f);
            setParam(state, "reverbSize", 0.35f);
            setParam(state, "reverbMix", 0.2f);

            Preset preset("Japanese Koto", "Pluck", state, true);
            preset.author = "Factory";
            preset.tags.add("asian");
            preset.tags.add("japanese");
            preset.tags.add("plucked");
            preset.description = "Warm Japanese koto with hybrid synthesis";
            presets.push_back(preset);
        }

        // ASIAN 2: Guzheng (Chinese)
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.45f);
            setParam(state, "osc2Wave", 0.0f);  // Sine
            setParam(state, "osc2Octave", 2.0f);
            setParam(state, "osc2Mix", 0.2f);
            setParam(state, "ringsStructure", 0.48f);
            setParam(state, "ringsBrightness", 0.55f);
            setParam(state, "ringsDamping", 0.32f);
            setParam(state, "ringsPosition", 0.45f);
            setParam(state, "ringsMix", 0.5f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.45f);
            setParam(state, "sustain", 0.25f);
            setParam(state, "release", 0.7f);
            setParam(state, "filterCutoff", 9000.0f);
            setParam(state, "filterResonance", 0.28f);
            setParam(state, "reverbSize", 0.4f);
            setParam(state, "reverbMix", 0.22f);

            Preset preset("Chinese Guzheng", "Pluck", state, true);
            preset.author = "Factory";
            preset.tags.add("asian");
            preset.tags.add("chinese");
            preset.tags.add("plucked");
            preset.description = "Bright Chinese guzheng zither";
            presets.push_back(preset);
        }

        // ASIAN 3: Shamisen (Japanese)
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 4.0f);  // Pulse
            setParam(state, "osc1PW", 0.35f);
            setParam(state, "osc1Mix", 0.55f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.35f);
            setParam(state, "ringsStructure", 0.32f);
            setParam(state, "ringsBrightness", 0.48f);
            setParam(state, "ringsDamping", 0.5f);
            setParam(state, "ringsPosition", 0.38f);
            setParam(state, "ringsMix", 0.4f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.25f);
            setParam(state, "sustain", 0.2f);
            setParam(state, "release", 0.4f);
            setParam(state, "filterCutoff", 7000.0f);
            setParam(state, "filterResonance", 0.3f);
            setParam(state, "reverbSize", 0.3f);
            setParam(state, "reverbMix", 0.15f);

            Preset preset("Japanese Shamisen", "Pluck", state, true);
            preset.author = "Factory";
            preset.tags.add("asian");
            preset.tags.add("japanese");
            preset.tags.add("percussive");
            preset.description = "Percussive Japanese shamisen lute";
            presets.push_back(preset);
        }

        // ASIAN 4: Pipa (Chinese Lute)
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.4f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Octave", 1.0f);
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.35f);
            setParam(state, "ringsStructure", 0.42f);
            setParam(state, "ringsBrightness", 0.58f);
            setParam(state, "ringsDamping", 0.36f);
            setParam(state, "ringsPosition", 0.42f);
            setParam(state, "ringsMix", 0.5f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.35f);
            setParam(state, "sustain", 0.28f);
            setParam(state, "release", 0.55f);
            setParam(state, "filterCutoff", 8500.0f);
            setParam(state, "filterResonance", 0.27f);
            setParam(state, "reverbSize", 0.38f);
            setParam(state, "reverbMix", 0.18f);

            Preset preset("Chinese Pipa", "Pluck", state, true);
            preset.author = "Factory";
            preset.tags.add("asian");
            preset.tags.add("chinese");
            preset.tags.add("bright");
            preset.description = "Bright Chinese pipa lute";
            presets.push_back(preset);
        }

        // =================================================================
        // ADDITIONAL 100 FACTORY PRESETS - EXPANDED COLLECTION
        // =================================================================

        // =================================================================
        // ARPS (10)
        // =================================================================

        // ARP 1: Fast Arp Lead
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "osc2Wave", 2.0f);  // Square
            setParam(state, "osc2Fine", 12.0f);
            setParam(state, "osc2Mix", 0.4f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.15f);
            setParam(state, "sustain", 0.4f);
            setParam(state, "release", 0.2f);
            setParam(state, "filterCutoff", 6000.0f);
            setParam(state, "filterResonance", 0.4f);
            setParam(state, "filterEnv", 0.6f);
            setParam(state, "delayTime", 125.0f);
            setParam(state, "delayFeedback", 0.3f);
            setParam(state, "delayMix", 0.2f);

            Preset preset("Fast Arp Lead", "Arp", state, true);
            preset.author = "Factory";
            preset.tags.add("arp");
            preset.tags.add("bright");
            preset.tags.add("fast");
            preset.description = "Bright arpeggiated lead with delay";
            presets.push_back(preset);
        }

        // ARP 2: Pluck Arp
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "ringsStructure", 0.35f);
            setParam(state, "ringsBrightness", 0.6f);
            setParam(state, "ringsDamping", 0.3f);
            setParam(state, "ringsMix", 0.4f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.25f);
            setParam(state, "sustain", 0.2f);
            setParam(state, "release", 0.3f);
            setParam(state, "filterCutoff", 8000.0f);
            setParam(state, "filterResonance", 0.3f);
            setParam(state, "delayTime", 250.0f);
            setParam(state, "delayFeedback", 0.35f);
            setParam(state, "delayMix", 0.25f);

            Preset preset("Pluck Arp", "Arp", state, true);
            preset.author = "Factory";
            preset.tags.add("arp");
            preset.tags.add("plucked");
            preset.tags.add("melodic");
            preset.description = "Plucked arpeggiated sound with rings";
            presets.push_back(preset);
        }

        // ARP 3: Analog Arp
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 4.0f);  // Pulse
            setParam(state, "osc1PW", 0.3f);
            setParam(state, "osc1Mix", 0.7f);
            setParam(state, "osc2Wave", 1.0f);  // Saw
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.5f);
            setParam(state, "attack", 0.005f);
            setParam(state, "decay", 0.2f);
            setParam(state, "sustain", 0.3f);
            setParam(state, "release", 0.25f);
            setParam(state, "filterCutoff", 4500.0f);
            setParam(state, "filterResonance", 0.5f);
            setParam(state, "filterEnv", 0.7f);

            Preset preset("Analog Arp", "Arp", state, true);
            preset.author = "Factory";
            preset.tags.add("arp");
            preset.tags.add("analog");
            preset.tags.add("vintage");
            preset.description = "Classic analog arpeggiated synth";
            presets.push_back(preset);
        }

        // ARP 4: Bell Arp
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Octave", 2.0f);
            setParam(state, "osc2Mix", 0.3f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.4f);
            setParam(state, "sustain", 0.2f);
            setParam(state, "release", 0.6f);
            setParam(state, "filterCutoff", 12000.0f);
            setParam(state, "filterResonance", 0.25f);
            setParam(state, "reverbSize", 0.6f);
            setParam(state, "reverbMix", 0.4f);

            Preset preset("Bell Arp", "Arp", state, true);
            preset.author = "Factory";
            preset.tags.add("arp");
            preset.tags.add("bell");
            preset.tags.add("bright");
            preset.description = "Bell-like arpeggiated tones";
            presets.push_back(preset);
        }

        // ARP 5: Granular Arp
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 7.0f);  // Osc + Clouds
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "cloudsDensity", 0.4f);
            setParam(state, "cloudsSize", 0.3f);
            setParam(state, "cloudsTexture", 0.5f);
            setParam(state, "grainsMix", 0.5f);
            setParam(state, "attack", 0.01f);
            setParam(state, "decay", 0.3f);
            setParam(state, "sustain", 0.3f);
            setParam(state, "release", 0.4f);
            setParam(state, "filterCutoff", 7000.0f);
            setParam(state, "filterResonance", 0.35f);

            Preset preset("Granular Arp", "Arp", state, true);
            preset.author = "Factory";
            preset.tags.add("arp");
            preset.tags.add("granular");
            preset.tags.add("textured");
            preset.description = "Textured arpeggiated sound with grains";
            presets.push_back(preset);
        }

        // ARP 6-10: Continue with more arp variations...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 2.0f);  // Square
            setParam(state, "osc1Mix", 0.75f);
            setParam(state, "osc2Wave", 0.0f);  // Sine
            setParam(state, "osc2Octave", 1.0f);
            setParam(state, "osc2Mix", 0.4f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.18f);
            setParam(state, "sustain", 0.35f);
            setParam(state, "release", 0.22f);
            setParam(state, "filterCutoff", 5500.0f);
            setParam(state, "filterResonance", 0.45f);
            setParam(state, "filterEnv", 0.65f);
            setParam(state, "chorusRate", 0.8f);
            setParam(state, "chorusDepth", 0.3f);
            setParam(state, "chorusMix", 0.2f);

            Preset preset("Square Arp", "Arp", state, true);
            preset.author = "Factory";
            preset.tags.add("arp");
            preset.tags.add("square");
            preset.tags.add("digital");
            preset.description = "Digital square wave arpeggio";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.7f);
            setParam(state, "osc2Wave", 4.0f);  // Pulse
            setParam(state, "osc2PW", 0.25f);
            setParam(state, "osc2Fine", 19.0f);
            setParam(state, "osc2Mix", 0.3f);
            setParam(state, "attack", 0.002f);
            setParam(state, "decay", 0.12f);
            setParam(state, "sustain", 0.5f);
            setParam(state, "release", 0.15f);
            setParam(state, "filterCutoff", 9000.0f);
            setParam(state, "filterResonance", 0.3f);

            Preset preset("Soft Arp", "Arp", state, true);
            preset.author = "Factory";
            preset.tags.add("arp");
            preset.tags.add("soft");
            preset.tags.add("mellow");
            preset.description = "Soft melodic arpeggio";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "osc2Wave", 1.0f);  // Saw
            setParam(state, "osc2Octave", 1.0f);
            setParam(state, "osc2Fine", -7.0f);
            setParam(state, "osc2Mix", 0.6f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.08f);
            setParam(state, "sustain", 0.6f);
            setParam(state, "release", 0.1f);
            setParam(state, "filterCutoff", 3500.0f);
            setParam(state, "filterResonance", 0.6f);
            setParam(state, "filterEnv", 0.8f);

            Preset preset("Acid Arp", "Arp", state, true);
            preset.author = "Factory";
            preset.tags.add("arp");
            preset.tags.add("acid");
            preset.tags.add("aggressive");
            preset.description = "Aggressive acid-style arpeggio";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Mix", 0.5f);
            setParam(state, "ringsStructure", 0.6f);
            setParam(state, "ringsBrightness", 0.7f);
            setParam(state, "ringsDamping", 0.25f);
            setParam(state, "ringsMix", 0.5f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.35f);
            setParam(state, "sustain", 0.15f);
            setParam(state, "release", 0.5f);
            setParam(state, "filterCutoff", 10000.0f);
            setParam(state, "filterResonance", 0.2f);

            Preset preset("Crystal Arp", "Arp", state, true);
            preset.author = "Factory";
            preset.tags.add("arp");
            preset.tags.add("crystal");
            preset.tags.add("bright");
            preset.description = "Crystalline arpeggio with rings";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 4.0f);  // Pulse
            setParam(state, "osc1PW", 0.1f);
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "osc2Wave", 4.0f);  // Pulse
            setParam(state, "osc2PW", 0.9f);
            setParam(state, "osc2Fine", 5.0f);
            setParam(state, "osc2Mix", 0.6f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.22f);
            setParam(state, "sustain", 0.25f);
            setParam(state, "release", 0.18f);
            setParam(state, "filterCutoff", 4000.0f);
            setParam(state, "filterResonance", 0.55f);
            setParam(state, "filterEnv", 0.75f);

            Preset preset("Pulse Arp", "Arp", state, true);
            preset.author = "Factory";
            preset.tags.add("arp");
            preset.tags.add("pulse");
            preset.tags.add("rhythmic");
            preset.description = "Rhythmic pulse wave arpeggio";
            presets.push_back(preset);
        }

        // =================================================================
        // FX SOUNDS (10)
        // =================================================================

        // FX 1: Sweep FX
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 7.0f);  // Osc + Clouds
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.5f);
            setParam(state, "cloudsDensity", 0.8f);
            setParam(state, "cloudsSize", 0.9f);
            setParam(state, "cloudsTexture", 0.7f);
            setParam(state, "cloudsPitch", 0.5f);
            setParam(state, "grainsMix", 0.7f);
            setParam(state, "attack", 2.0f);
            setParam(state, "decay", 1.0f);
            setParam(state, "sustain", 0.8f);
            setParam(state, "release", 3.0f);
            setParam(state, "filterCutoff", 2000.0f);
            setParam(state, "filterResonance", 0.7f);
            setParam(state, "filterEnv", 0.9f);
            setParam(state, "reverbSize", 0.9f);
            setParam(state, "reverbMix", 0.6f);

            Preset preset("Sweep FX", "FX", state, true);
            preset.author = "Factory";
            preset.tags.add("fx");
            preset.tags.add("sweep");
            preset.tags.add("atmospheric");
            preset.description = "Sweeping atmospheric effect";
            presets.push_back(preset);
        }

        // FX 2: Noise FX
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 1.0f);  // Clouds only
            setParam(state, "cloudsDensity", 1.0f);
            setParam(state, "cloudsSize", 0.1f);
            setParam(state, "cloudsTexture", 0.9f);
            setParam(state, "cloudsPitch", -0.5f);
            setParam(state, "attack", 0.5f);
            setParam(state, "decay", 1.5f);
            setParam(state, "sustain", 0.3f);
            setParam(state, "release", 2.0f);
            setParam(state, "filterCutoff", 15000.0f);
            setParam(state, "filterResonance", 0.1f);
            setParam(state, "delayTime", 333.0f);
            setParam(state, "delayFeedback", 0.6f);
            setParam(state, "delayMix", 0.4f);

            Preset preset("Noise FX", "FX", state, true);
            preset.author = "Factory";
            preset.tags.add("fx");
            preset.tags.add("noise");
            preset.tags.add("textured");
            preset.description = "Textured noise effect";
            presets.push_back(preset);
        }

        // FX 3-10: More FX sounds...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 4.0f);  // Hybrid All
            setParam(state, "ringsMix", 0.3f);
            setParam(state, "karplusMix", 0.2f);
            setParam(state, "wavetableMix", 0.3f);
            setParam(state, "grainsMix", 0.8f);
            setParam(state, "cloudsDensity", 0.7f);
            setParam(state, "cloudsSize", 0.8f);
            setParam(state, "cloudsTexture", 0.6f);
            setParam(state, "attack", 1.0f);
            setParam(state, "decay", 2.0f);
            setParam(state, "sustain", 0.7f);
            setParam(state, "release", 3.0f);
            setParam(state, "filterCutoff", 8000.0f);
            setParam(state, "filterResonance", 0.3f);
            setParam(state, "reverbSize", 0.85f);
            setParam(state, "reverbMix", 0.5f);

            Preset preset("Evolving FX", "FX", state, true);
            preset.author = "Factory";
            preset.tags.add("fx");
            preset.tags.add("evolving");
            preset.tags.add("hybrid");
            preset.description = "Evolving hybrid texture";
            presets.push_back(preset);
        }

        // Continue adding more FX presets...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 0.0f);  // Rings only
            setParam(state, "ringsStructure", 0.9f);
            setParam(state, "ringsBrightness", 0.8f);
            setParam(state, "ringsDamping", 0.1f);
            setParam(state, "attack", 0.1f);
            setParam(state, "decay", 3.0f);
            setParam(state, "sustain", 0.5f);
            setParam(state, "release", 4.0f);
            setParam(state, "filterCutoff", 12000.0f);
            setParam(state, "filterResonance", 0.4f);
            setParam(state, "reverbSize", 0.95f);
            setParam(state, "reverbMix", 0.7f);
            setParam(state, "reverbShimmer", 0.8f);

            Preset preset("Metallic FX", "FX", state, true);
            preset.author = "Factory";
            preset.tags.add("fx");
            preset.tags.add("metallic");
            preset.tags.add("resonant");
            preset.description = "Metallic resonant effect";
            presets.push_back(preset);
        }

        // Add 6 more FX presets to reach 10 total...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 2.0f);  // Square
            setParam(state, "osc1Octave", -2.0f);
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "attack", 0.01f);
            setParam(state, "decay", 0.05f);
            setParam(state, "sustain", 0.0f);
            setParam(state, "release", 0.1f);
            setParam(state, "filterCutoff", 200.0f);
            setParam(state, "filterResonance", 0.9f);
            setParam(state, "filterEnv", 1.0f);

            Preset preset("Kick FX", "FX", state, true);
            preset.author = "Factory";
            preset.tags.add("fx");
            preset.tags.add("percussive");
            preset.tags.add("kick");
            preset.description = "Synthesized kick drum effect";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Octave", 2.0f);
            setParam(state, "osc1Mix", 0.9f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.02f);
            setParam(state, "sustain", 0.0f);
            setParam(state, "release", 0.05f);
            setParam(state, "filterCutoff", 15000.0f);
            setParam(state, "filterResonance", 0.1f);
            setParam(state, "reverbSize", 0.3f);
            setParam(state, "reverbMix", 0.2f);

            Preset preset("Percussion FX", "FX", state, true);
            preset.author = "Factory";
            preset.tags.add("fx");
            preset.tags.add("percussive");
            preset.tags.add("hit");
            preset.description = "Sharp percussive hit";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 7.0f);  // Osc + Clouds
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Octave", -1.0f);
            setParam(state, "osc1Mix", 0.4f);
            setParam(state, "cloudsDensity", 0.9f);
            setParam(state, "cloudsSize", 0.6f);
            setParam(state, "cloudsTexture", 0.8f);
            setParam(state, "cloudsPitch", -0.8f);
            setParam(state, "grainsMix", 0.9f);
            setParam(state, "attack", 0.8f);
            setParam(state, "decay", 2.0f);
            setParam(state, "sustain", 0.6f);
            setParam(state, "release", 5.0f);
            setParam(state, "filterCutoff", 5000.0f);
            setParam(state, "filterResonance", 0.2f);

            Preset preset("Ambient FX", "FX", state, true);
            preset.author = "Factory";
            preset.tags.add("fx");
            preset.tags.add("ambient");
            preset.tags.add("dark");
            preset.description = "Dark ambient soundscape";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 4.0f);  // Pulse
            setParam(state, "osc1PW", 0.05f);
            setParam(state, "osc1Mix", 0.7f);
            setParam(state, "osc2Wave", 4.0f);  // Pulse
            setParam(state, "osc2PW", 0.95f);
            setParam(state, "osc2Fine", 1.0f);
            setParam(state, "osc2Mix", 0.7f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.1f);
            setParam(state, "sustain", 0.8f);
            setParam(state, "release", 0.2f);
            setParam(state, "filterCutoff", 8000.0f);
            setParam(state, "filterResonance", 0.8f);
            setParam(state, "filterEnv", 0.5f);

            Preset preset("Laser FX", "FX", state, true);
            preset.author = "Factory";
            preset.tags.add("fx");
            preset.tags.add("laser");
            preset.tags.add("sci-fi");
            preset.description = "Sci-fi laser effect";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 1.0f);  // Clouds only
            setParam(state, "cloudsDensity", 0.3f);
            setParam(state, "cloudsSize", 0.95f);
            setParam(state, "cloudsTexture", 0.4f);
            setParam(state, "cloudsPitch", 0.2f);
            setParam(state, "cloudsStereo", 1.0f);
            setParam(state, "attack", 1.5f);
            setParam(state, "decay", 0.5f);
            setParam(state, "sustain", 0.9f);
            setParam(state, "release", 2.5f);
            setParam(state, "filterCutoff", 10000.0f);
            setParam(state, "filterResonance", 0.15f);
            setParam(state, "reverbSize", 0.8f);
            setParam(state, "reverbMix", 0.5f);

            Preset preset("Wind FX", "FX", state, true);
            preset.author = "Factory";
            preset.tags.add("fx");
            preset.tags.add("wind");
            preset.tags.add("natural");
            preset.description = "Wind-like atmospheric effect";
            presets.push_back(preset);
        }

        // =================================================================
        // MALLETS (10)
        // =================================================================

        // MALLET 1: Vibraphone
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "osc2Wave", 0.0f);  // Sine
            setParam(state, "osc2Octave", 1.0f);
            setParam(state, "osc2Fine", 3.0f);
            setParam(state, "osc2Mix", 0.4f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.8f);
            setParam(state, "sustain", 0.6f);
            setParam(state, "release", 1.2f);
            setParam(state, "filterCutoff", 8000.0f);
            setParam(state, "filterResonance", 0.2f);
            setParam(state, "reverbSize", 0.5f);
            setParam(state, "reverbMix", 0.3f);
            setParam(state, "chorusRate", 4.5f);
            setParam(state, "chorusDepth", 0.4f);
            setParam(state, "chorusMix", 0.3f);

            Preset preset("Vibraphone", "Mallet", state, true);
            preset.author = "Factory";
            preset.tags.add("mallet");
            preset.tags.add("vibraphone");
            preset.tags.add("warm");
            preset.description = "Warm vibraphone with tremolo";
            presets.push_back(preset);
        }

        // MALLET 2: Marimba
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "ringsStructure", 0.3f);
            setParam(state, "ringsBrightness", 0.35f);
            setParam(state, "ringsDamping", 0.6f);
            setParam(state, "ringsMix", 0.4f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.6f);
            setParam(state, "sustain", 0.3f);
            setParam(state, "release", 0.8f);
            setParam(state, "filterCutoff", 6000.0f);
            setParam(state, "filterResonance", 0.25f);
            setParam(state, "reverbSize", 0.4f);
            setParam(state, "reverbMix", 0.25f);

            Preset preset("Marimba", "Mallet", state, true);
            preset.author = "Factory";
            preset.tags.add("mallet");
            preset.tags.add("marimba");
            preset.tags.add("wooden");
            preset.description = "Wooden marimba with natural decay";
            presets.push_back(preset);
        }

        // Continue with 8 more mallet instruments...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.7f);
            setParam(state, "osc2Wave", 0.0f);  // Sine
            setParam(state, "osc2Octave", 2.0f);
            setParam(state, "osc2Mix", 0.5f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.3f);
            setParam(state, "sustain", 0.2f);
            setParam(state, "release", 0.5f);
            setParam(state, "filterCutoff", 10000.0f);
            setParam(state, "filterResonance", 0.3f);
            setParam(state, "reverbSize", 0.45f);
            setParam(state, "reverbMix", 0.3f);

            Preset preset("Xylophone", "Mallet", state, true);
            preset.author = "Factory";
            preset.tags.add("mallet");
            preset.tags.add("xylophone");
            preset.tags.add("bright");
            preset.description = "Bright xylophone sound";
            presets.push_back(preset);
        }

        // Add 7 more mallet presets to reach 10 total...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 0.0f);  // Rings only
            setParam(state, "ringsStructure", 0.7f);
            setParam(state, "ringsBrightness", 0.6f);
            setParam(state, "ringsDamping", 0.4f);
            setParam(state, "ringsModel", 3.0f);  // Bell
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 1.0f);
            setParam(state, "sustain", 0.4f);
            setParam(state, "release", 1.5f);
            setParam(state, "filterCutoff", 12000.0f);
            setParam(state, "filterResonance", 0.2f);
            setParam(state, "reverbSize", 0.6f);
            setParam(state, "reverbMix", 0.4f);

            Preset preset("Glockenspiel", "Mallet", state, true);
            preset.author = "Factory";
            preset.tags.add("mallet");
            preset.tags.add("glockenspiel");
            preset.tags.add("metallic");
            preset.description = "Metallic glockenspiel bells";
            presets.push_back(preset);
        }

        // Continue adding the remaining 6 mallets...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Mix", 0.5f);
            setParam(state, "ringsStructure", 0.5f);
            setParam(state, "ringsBrightness", 0.5f);
            setParam(state, "ringsDamping", 0.5f);
            setParam(state, "ringsMix", 0.5f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.7f);
            setParam(state, "sustain", 0.35f);
            setParam(state, "release", 1.0f);
            setParam(state, "filterCutoff", 9000.0f);
            setParam(state, "filterResonance", 0.25f);
            setParam(state, "reverbSize", 0.5f);
            setParam(state, "reverbMix", 0.35f);

            Preset preset("Celesta", "Mallet", state, true);
            preset.author = "Factory";
            preset.tags.add("mallet");
            preset.tags.add("celesta");
            preset.tags.add("delicate");
            preset.description = "Delicate celesta sound";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 4.0f);  // Pulse
            setParam(state, "osc1PW", 0.6f);
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.4f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.4f);
            setParam(state, "sustain", 0.25f);
            setParam(state, "release", 0.6f);
            setParam(state, "filterCutoff", 7500.0f);
            setParam(state, "filterResonance", 0.3f);
            setParam(state, "reverbSize", 0.4f);
            setParam(state, "reverbMix", 0.25f);

            Preset preset("Steel Drums", "Mallet", state, true);
            preset.author = "Factory";
            preset.tags.add("mallet");
            preset.tags.add("steel");
            preset.tags.add("tropical");
            preset.description = "Caribbean steel drums";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.3f);
            setParam(state, "ringsStructure", 0.45f);
            setParam(state, "ringsBrightness", 0.7f);
            setParam(state, "ringsDamping", 0.3f);
            setParam(state, "ringsMix", 0.7f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.5f);
            setParam(state, "sustain", 0.3f);
            setParam(state, "release", 0.7f);
            setParam(state, "filterCutoff", 11000.0f);
            setParam(state, "filterResonance", 0.2f);
            setParam(state, "reverbSize", 0.45f);
            setParam(state, "reverbMix", 0.3f);

            Preset preset("Gamelan", "Mallet", state, true);
            preset.author = "Factory";
            preset.tags.add("mallet");
            preset.tags.add("gamelan");
            preset.tags.add("ethnic");
            preset.description = "Indonesian gamelan percussion";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Octave", 3.0f);
            setParam(state, "osc2Mix", 0.3f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.15f);
            setParam(state, "sustain", 0.1f);
            setParam(state, "release", 0.3f);
            setParam(state, "filterCutoff", 13000.0f);
            setParam(state, "filterResonance", 0.25f);
            setParam(state, "reverbSize", 0.35f);
            setParam(state, "reverbMix", 0.2f);

            Preset preset("Music Box", "Mallet", state, true);
            preset.author = "Factory";
            preset.tags.add("mallet");
            preset.tags.add("music-box");
            preset.tags.add("delicate");
            preset.description = "Delicate music box sound";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 0.0f);  // Rings only
            setParam(state, "ringsStructure", 0.8f);
            setParam(state, "ringsBrightness", 0.8f);
            setParam(state, "ringsDamping", 0.2f);
            setParam(state, "ringsModel", 2.0f);  // Tube
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 1.2f);
            setParam(state, "sustain", 0.5f);
            setParam(state, "release", 2.0f);
            setParam(state, "filterCutoff", 14000.0f);
            setParam(state, "filterResonance", 0.15f);
            setParam(state, "reverbSize", 0.7f);
            setParam(state, "reverbMix", 0.5f);

            Preset preset("Tubular Bells", "Mallet", state, true);
            preset.author = "Factory";
            preset.tags.add("mallet");
            preset.tags.add("tubular");
            preset.tags.add("orchestral");
            preset.description = "Orchestral tubular bells";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 2.0f);  // Square
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "osc2Wave", 0.0f);  // Sine
            setParam(state, "osc2Octave", 2.0f);
            setParam(state, "osc2Mix", 0.4f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.25f);
            setParam(state, "sustain", 0.15f);
            setParam(state, "release", 0.4f);
            setParam(state, "filterCutoff", 8500.0f);
            setParam(state, "filterResonance", 0.35f);
            setParam(state, "reverbSize", 0.4f);
            setParam(state, "reverbMix", 0.25f);

            Preset preset("Crotales", "Mallet", state, true);
            preset.author = "Factory";
            preset.tags.add("mallet");
            preset.tags.add("crotales");
            preset.tags.add("bright");
            preset.description = "Bright crotales cymbals";
            presets.push_back(preset);
        }

        // =================================================================
        // BELLS (10)
        // =================================================================

        // BELL 1: Church Bell
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 0.0f);  // Rings only
            setParam(state, "ringsStructure", 0.8f);
            setParam(state, "ringsBrightness", 0.7f);
            setParam(state, "ringsDamping", 0.1f);
            setParam(state, "ringsModel", 3.0f);  // Bell
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 2.0f);
            setParam(state, "sustain", 0.6f);
            setParam(state, "release", 4.0f);
            setParam(state, "filterCutoff", 10000.0f);
            setParam(state, "filterResonance", 0.2f);
            setParam(state, "reverbSize", 0.9f);
            setParam(state, "reverbMix", 0.6f);

            Preset preset("Church Bell", "Bells", state, true);
            preset.author = "Factory";
            preset.tags.add("bell");
            preset.tags.add("church");
            preset.tags.add("resonant");
            preset.description = "Deep resonant church bell";
            presets.push_back(preset);
        }

        // BELL 2: Wind Chimes
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 0.0f);  // Rings only
            setParam(state, "ringsStructure", 0.9f);
            setParam(state, "ringsBrightness", 0.9f);
            setParam(state, "ringsDamping", 0.3f);
            setParam(state, "ringsModel", 2.0f);  // Tube
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 1.5f);
            setParam(state, "sustain", 0.4f);
            setParam(state, "release", 2.5f);
            setParam(state, "filterCutoff", 15000.0f);
            setParam(state, "filterResonance", 0.15f);
            setParam(state, "reverbSize", 0.7f);
            setParam(state, "reverbMix", 0.5f);

            Preset preset("Wind Chimes", "Bells", state, true);
            preset.author = "Factory";
            preset.tags.add("bell");
            preset.tags.add("chimes");
            preset.tags.add("delicate");
            preset.description = "Delicate metallic wind chimes";
            presets.push_back(preset);
        }

        // BELL 3-10: Continue with more bell types...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Mix", 0.9f);
            setParam(state, "osc2Wave", 0.0f);  // Sine
            setParam(state, "osc2Octave", 2.0f);
            setParam(state, "osc2Mix", 0.4f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.8f);
            setParam(state, "sustain", 0.5f);
            setParam(state, "release", 1.5f);
            setParam(state, "filterCutoff", 12000.0f);
            setParam(state, "filterResonance", 0.25f);
            setParam(state, "reverbSize", 0.6f);
            setParam(state, "reverbMix", 0.4f);

            Preset preset("Glass Bell", "Bells", state, true);
            preset.author = "Factory";
            preset.tags.add("bell");
            preset.tags.add("glass");
            preset.tags.add("pure");
            preset.description = "Pure glass bell tone";
            presets.push_back(preset);
        }

        // Add 7 more bell presets...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.4f);
            setParam(state, "ringsStructure", 0.7f);
            setParam(state, "ringsBrightness", 0.8f);
            setParam(state, "ringsDamping", 0.25f);
            setParam(state, "ringsMix", 0.6f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 1.2f);
            setParam(state, "sustain", 0.5f);
            setParam(state, "release", 2.0f);
            setParam(state, "filterCutoff", 11000.0f);
            setParam(state, "filterResonance", 0.2f);
            setParam(state, "reverbSize", 0.55f);
            setParam(state, "reverbMix", 0.35f);

            Preset preset("Temple Bell", "Bells", state, true);
            preset.author = "Factory";
            preset.tags.add("bell");
            preset.tags.add("temple");
            preset.tags.add("zen");
            preset.description = "Meditative temple bell";
            presets.push_back(preset);
        }

        // Continue adding remaining bell presets...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Octave", 1.0f);
            setParam(state, "osc2Fine", 5.0f);
            setParam(state, "osc2Mix", 0.3f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.6f);
            setParam(state, "sustain", 0.4f);
            setParam(state, "release", 1.0f);
            setParam(state, "filterCutoff", 9500.0f);
            setParam(state, "filterResonance", 0.3f);
            setParam(state, "reverbSize", 0.5f);
            setParam(state, "reverbMix", 0.3f);

            Preset preset("Silver Bell", "Bells", state, true);
            preset.author = "Factory";
            preset.tags.add("bell");
            preset.tags.add("silver");
            preset.tags.add("bright");
            preset.description = "Bright silver bell tone";
            presets.push_back(preset);
        }

        // Add remaining bells (5 more)...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 0.0f);  // Rings only
            setParam(state, "ringsStructure", 0.6f);
            setParam(state, "ringsBrightness", 0.6f);
            setParam(state, "ringsDamping", 0.4f);
            setParam(state, "ringsModel", 3.0f);  // Bell
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 1.8f);
            setParam(state, "sustain", 0.3f);
            setParam(state, "release", 3.0f);
            setParam(state, "filterCutoff", 8000.0f);
            setParam(state, "filterResonance", 0.4f);
            setParam(state, "reverbSize", 0.8f);
            setParam(state, "reverbMix", 0.55f);

            Preset preset("Bronze Bell", "Bells", state, true);
            preset.author = "Factory";
            preset.tags.add("bell");
            preset.tags.add("bronze");
            preset.tags.add("warm");
            preset.description = "Warm bronze bell with character";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Mix", 0.7f);
            setParam(state, "osc2Wave", 0.0f);  // Sine
            setParam(state, "osc2Octave", 3.0f);
            setParam(state, "osc2Mix", 0.4f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.4f);
            setParam(state, "sustain", 0.2f);
            setParam(state, "release", 0.8f);
            setParam(state, "filterCutoff", 16000.0f);
            setParam(state, "filterResonance", 0.2f);
            setParam(state, "reverbSize", 0.45f);
            setParam(state, "reverbMix", 0.25f);

            Preset preset("Sleigh Bells", "Bells", state, true);
            preset.author = "Factory";
            preset.tags.add("bell");
            preset.tags.add("sleigh");
            preset.tags.add("festive");
            preset.description = "Festive sleigh bells";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 2.0f);  // Square
            setParam(state, "osc1Mix", 0.3f);
            setParam(state, "ringsStructure", 0.85f);
            setParam(state, "ringsBrightness", 0.75f);
            setParam(state, "ringsDamping", 0.2f);
            setParam(state, "ringsMix", 0.7f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 1.0f);
            setParam(state, "sustain", 0.45f);
            setParam(state, "release", 1.8f);
            setParam(state, "filterCutoff", 13000.0f);
            setParam(state, "filterResonance", 0.18f);
            setParam(state, "reverbSize", 0.6f);
            setParam(state, "reverbMix", 0.4f);

            Preset preset("Crystal Bell", "Bells", state, true);
            preset.author = "Factory";
            preset.tags.add("bell");
            preset.tags.add("crystal");
            preset.tags.add("ethereal");
            preset.description = "Ethereal crystal bell";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "osc2Wave", 0.0f);  // Sine
            setParam(state, "osc2Octave", 2.0f);
            setParam(state, "osc2Fine", 12.0f);
            setParam(state, "osc2Mix", 0.3f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.9f);
            setParam(state, "sustain", 0.35f);
            setParam(state, "release", 1.4f);
            setParam(state, "filterCutoff", 10500.0f);
            setParam(state, "filterResonance", 0.25f);
            setParam(state, "reverbSize", 0.5f);
            setParam(state, "reverbMix", 0.3f);

            Preset preset("Handbells", "Bells", state, true);
            preset.author = "Factory";
            preset.tags.add("bell");
            preset.tags.add("handbell");
            preset.tags.add("choir");
            preset.description = "Choir handbells ensemble";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 0.0f);  // Rings only
            setParam(state, "ringsStructure", 0.95f);
            setParam(state, "ringsBrightness", 0.85f);
            setParam(state, "ringsDamping", 0.15f);
            setParam(state, "ringsModel", 2.0f);  // Tube
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 2.5f);
            setParam(state, "sustain", 0.5f);
            setParam(state, "release", 4.0f);
            setParam(state, "filterCutoff", 14000.0f);
            setParam(state, "filterResonance", 0.15f);
            setParam(state, "reverbSize", 0.85f);
            setParam(state, "reverbMix", 0.6f);

            Preset preset("Singing Bowl", "Bells", state, true);
            preset.author = "Factory";
            preset.tags.add("bell");
            preset.tags.add("singing");
            preset.tags.add("meditation");
            preset.description = "Tibetan singing bowl";
            presets.push_back(preset);
        }

        // =================================================================
        // ETHNIC INSTRUMENTS (10)
        // =================================================================

        // ETHNIC 1: Erhu
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Fine", 5.0f);
            setParam(state, "osc2Mix", 0.4f);
            setParam(state, "attack", 0.05f);
            setParam(state, "decay", 0.3f);
            setParam(state, "sustain", 0.8f);
            setParam(state, "release", 0.6f);
            setParam(state, "filterCutoff", 4500.0f);
            setParam(state, "filterResonance", 0.6f);
            setParam(state, "filterEnv", 0.4f);
            setParam(state, "reverbSize", 0.4f);
            setParam(state, "reverbMix", 0.2f);

            Preset preset("Erhu", "Ethnic", state, true);
            preset.author = "Factory";
            preset.tags.add("ethnic");
            preset.tags.add("chinese");
            preset.tags.add("bowed");
            preset.description = "Chinese two-string fiddle";
            presets.push_back(preset);
        }

        // ETHNIC 2: Tabla
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Octave", -1.0f);
            setParam(state, "osc1Mix", 0.9f);
            setParam(state, "osc2Wave", 2.0f);  // Square
            setParam(state, "osc2Octave", 1.0f);
            setParam(state, "osc2Mix", 0.2f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.1f);
            setParam(state, "sustain", 0.0f);
            setParam(state, "release", 0.15f);
            setParam(state, "filterCutoff", 3000.0f);
            setParam(state, "filterResonance", 0.7f);
            setParam(state, "filterEnv", 0.8f);

            Preset preset("Tabla", "Ethnic", state, true);
            preset.author = "Factory";
            preset.tags.add("ethnic");
            preset.tags.add("indian");
            preset.tags.add("percussion");
            preset.description = "Indian tabla drum";
            presets.push_back(preset);
        }

        // Continue with 8 more ethnic instruments...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 4.0f);  // Pulse
            setParam(state, "osc1PW", 0.3f);
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "ringsStructure", 0.4f);
            setParam(state, "ringsBrightness", 0.6f);
            setParam(state, "ringsDamping", 0.3f);
            setParam(state, "ringsMix", 0.4f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.3f);
            setParam(state, "sustain", 0.2f);
            setParam(state, "release", 0.5f);
            setParam(state, "filterCutoff", 7000.0f);
            setParam(state, "filterResonance", 0.3f);
            setParam(state, "reverbSize", 0.35f);
            setParam(state, "reverbMix", 0.2f);

            Preset preset("Sitar", "Ethnic", state, true);
            preset.author = "Factory";
            preset.tags.add("ethnic");
            preset.tags.add("indian");
            preset.tags.add("plucked");
            preset.description = "Indian sitar with sympathetic strings";
            presets.push_back(preset);
        }

        // Add remaining ethnic instruments (7 more)...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.7f);
            setParam(state, "osc2Wave", 4.0f);  // Pulse
            setParam(state, "osc2PW", 0.25f);
            setParam(state, "osc2Fine", 12.0f);
            setParam(state, "osc2Mix", 0.5f);
            setParam(state, "attack", 0.02f);
            setParam(state, "decay", 0.2f);
            setParam(state, "sustain", 0.7f);
            setParam(state, "release", 0.4f);
            setParam(state, "filterCutoff", 5500.0f);
            setParam(state, "filterResonance", 0.4f);
            setParam(state, "filterEnv", 0.3f);
            setParam(state, "reverbSize", 0.4f);
            setParam(state, "reverbMix", 0.25f);

            Preset preset("Duduk", "Ethnic", state, true);
            preset.author = "Factory";
            preset.tags.add("ethnic");
            preset.tags.add("armenian");
            preset.tags.add("wind");
            preset.description = "Armenian duduk woodwind";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Mix", 0.5f);
            setParam(state, "ringsStructure", 0.55f);
            setParam(state, "ringsBrightness", 0.7f);
            setParam(state, "ringsDamping", 0.35f);
            setParam(state, "ringsMix", 0.5f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.35f);
            setParam(state, "sustain", 0.25f);
            setParam(state, "release", 0.6f);
            setParam(state, "filterCutoff", 8500.0f);
            setParam(state, "filterResonance", 0.25f);
            setParam(state, "reverbSize", 0.4f);
            setParam(state, "reverbMix", 0.25f);

            Preset preset("Mbira", "Ethnic", state, true);
            preset.author = "Factory";
            preset.tags.add("ethnic");
            preset.tags.add("african");
            preset.tags.add("thumb-piano");
            preset.description = "African mbira thumb piano";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "osc2Wave", 1.0f);  // Saw
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.3f);
            setParam(state, "attack", 0.01f);
            setParam(state, "decay", 0.25f);
            setParam(state, "sustain", 0.6f);
            setParam(state, "release", 0.5f);
            setParam(state, "filterCutoff", 6000.0f);
            setParam(state, "filterResonance", 0.35f);
            setParam(state, "filterEnv", 0.4f);
            setParam(state, "reverbSize", 0.45f);
            setParam(state, "reverbMix", 0.3f);

            Preset preset("Oud", "Ethnic", state, true);
            preset.author = "Factory";
            preset.tags.add("ethnic");
            preset.tags.add("middle-eastern");
            preset.tags.add("lute");
            preset.description = "Middle Eastern oud lute";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 2.0f);  // Square
            setParam(state, "osc1Mix", 0.7f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Octave", 1.0f);
            setParam(state, "osc2Fine", 5.0f);
            setParam(state, "osc2Mix", 0.4f);
            setParam(state, "attack", 0.03f);
            setParam(state, "decay", 0.2f);
            setParam(state, "sustain", 0.7f);
            setParam(state, "release", 0.4f);
            setParam(state, "filterCutoff", 5000.0f);
            setParam(state, "filterResonance", 0.5f);
            setParam(state, "filterEnv", 0.35f);
            setParam(state, "reverbSize", 0.4f);
            setParam(state, "reverbMix", 0.25f);

            Preset preset("Didgeridoo", "Ethnic", state, true);
            preset.author = "Factory";
            preset.tags.add("ethnic");
            preset.tags.add("australian");
            preset.tags.add("wind");
            preset.description = "Australian didgeridoo";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 4.0f);  // Pulse
            setParam(state, "osc1PW", 0.4f);
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "ringsStructure", 0.45f);
            setParam(state, "ringsBrightness", 0.55f);
            setParam(state, "ringsDamping", 0.4f);
            setParam(state, "ringsMix", 0.4f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.4f);
            setParam(state, "sustain", 0.3f);
            setParam(state, "release", 0.7f);
            setParam(state, "filterCutoff", 7500.0f);
            setParam(state, "filterResonance", 0.3f);
            setParam(state, "reverbSize", 0.4f);
            setParam(state, "reverbMix", 0.25f);

            Preset preset("Balalaika", "Ethnic", state, true);
            preset.author = "Factory";
            preset.tags.add("ethnic");
            preset.tags.add("russian");
            preset.tags.add("plucked");
            preset.description = "Russian balalaika";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "osc2Wave", 2.0f);  // Square
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.4f);
            setParam(state, "attack", 0.01f);
            setParam(state, "decay", 0.3f);
            setParam(state, "sustain", 0.7f);
            setParam(state, "release", 0.5f);
            setParam(state, "filterCutoff", 4800.0f);
            setParam(state, "filterResonance", 0.45f);
            setParam(state, "filterEnv", 0.4f);
            setParam(state, "reverbSize", 0.4f);
            setParam(state, "reverbMix", 0.25f);

            Preset preset("Bagpipes", "Ethnic", state, true);
            preset.author = "Factory";
            preset.tags.add("ethnic");
            preset.tags.add("scottish");
            preset.tags.add("wind");
            preset.description = "Scottish Highland bagpipes";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.5f);
            setParam(state, "ringsStructure", 0.5f);
            setParam(state, "ringsBrightness", 0.6f);
            setParam(state, "ringsDamping", 0.3f);
            setParam(state, "ringsMix", 0.5f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.5f);
            setParam(state, "sustain", 0.3f);
            setParam(state, "release", 0.8f);
            setParam(state, "filterCutoff", 9000.0f);
            setParam(state, "filterResonance", 0.25f);
            setParam(state, "reverbSize", 0.45f);
            setParam(state, "reverbMix", 0.3f);

            Preset preset("Banjo", "Ethnic", state, true);
            preset.author = "Factory";
            preset.tags.add("ethnic");
            preset.tags.add("american");
            preset.tags.add("plucked");
            preset.description = "American folk banjo";
            presets.push_back(preset);
        }

        // =================================================================
        // ADDITIONAL PAD VARIATIONS (10)
        // =================================================================

        // PAD 6: Morphing Pad
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 7.0f);  // Osc + Clouds
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Octave", -1.0f);
            setParam(state, "osc2Mix", 0.4f);
            setParam(state, "cloudsDensity", 0.5f);
            setParam(state, "cloudsSize", 0.8f);
            setParam(state, "cloudsTexture", 0.7f);
            setParam(state, "grainsMix", 0.6f);
            setParam(state, "attack", 1.2f);
            setParam(state, "decay", 0.5f);
            setParam(state, "sustain", 0.85f);
            setParam(state, "release", 1.8f);
            setParam(state, "filterCutoff", 4000.0f);
            setParam(state, "filterResonance", 0.25f);
            setParam(state, "filterEnv", 0.4f);
            setParam(state, "reverbSize", 0.8f);
            setParam(state, "reverbMix", 0.5f);

            Preset preset("Morphing Pad", "Pad", state, true);
            preset.author = "Factory";
            preset.tags.add("pad");
            preset.tags.add("morphing");
            preset.tags.add("granular");
            preset.description = "Morphing granular pad texture";
            presets.push_back(preset);
        }

        // PAD 7: Glass Pad
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.7f);
            setParam(state, "osc2Wave", 0.0f);  // Sine
            setParam(state, "osc2Octave", 2.0f);
            setParam(state, "osc2Fine", 12.0f);
            setParam(state, "osc2Mix", 0.3f);
            setParam(state, "attack", 0.9f);
            setParam(state, "decay", 0.4f);
            setParam(state, "sustain", 0.8f);
            setParam(state, "release", 1.5f);
            setParam(state, "filterCutoff", 6000.0f);
            setParam(state, "filterResonance", 0.15f);
            setParam(state, "filterEnv", 0.2f);
            setParam(state, "reverbSize", 0.7f);
            setParam(state, "reverbMix", 0.4f);
            setParam(state, "reverbShimmer", 0.6f);

            Preset preset("Glass Pad", "Pad", state, true);
            preset.author = "Factory";
            preset.tags.add("pad");
            preset.tags.add("glass");
            preset.tags.add("transparent");
            preset.description = "Transparent glass-like pad";
            presets.push_back(preset);
        }

        // Continue with remaining pad variations (8 more)...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 8.0f);  // Full Hybrid
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.4f);
            setParam(state, "osc2Wave", 4.0f);  // Pulse
            setParam(state, "osc2PW", 0.3f);
            setParam(state, "osc2Mix", 0.3f);
            setParam(state, "ringsMix", 0.2f);
            setParam(state, "karplusMix", 0.1f);
            setParam(state, "wavetableMix", 0.2f);
            setParam(state, "grainsMix", 0.5f);
            setParam(state, "attack", 1.5f);
            setParam(state, "decay", 0.6f);
            setParam(state, "sustain", 0.9f);
            setParam(state, "release", 2.0f);
            setParam(state, "filterCutoff", 3000.0f);
            setParam(state, "filterResonance", 0.3f);
            setParam(state, "reverbSize", 0.85f);
            setParam(state, "reverbMix", 0.6f);

            Preset preset("Epic Pad", "Pad", state, true);
            preset.author = "Factory";
            preset.tags.add("pad");
            preset.tags.add("epic");
            preset.tags.add("cinematic");
            preset.description = "Epic cinematic hybrid pad";
            presets.push_back(preset);
        }

        // Add remaining pad variations...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 2.0f);  // Square
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "osc2Wave", 2.0f);  // Square
            setParam(state, "osc2Octave", -1.0f);
            setParam(state, "osc2Fine", 5.0f);
            setParam(state, "osc2Mix", 0.5f);
            setParam(state, "attack", 0.7f);
            setParam(state, "decay", 0.4f);
            setParam(state, "sustain", 0.8f);
            setParam(state, "release", 1.2f);
            setParam(state, "filterCutoff", 2500.0f);
            setParam(state, "filterResonance", 0.4f);
            setParam(state, "filterEnv", 0.5f);
            setParam(state, "reverbSize", 0.6f);
            setParam(state, "reverbMix", 0.35f);

            Preset preset("Retro Pad", "Pad", state, true);
            preset.author = "Factory";
            preset.tags.add("pad");
            preset.tags.add("retro");
            preset.tags.add("vintage");
            preset.description = "Retro square wave pad";
            presets.push_back(preset);
        }

        // Add 6 more pad presets...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Mix", 0.5f);
            setParam(state, "ringsStructure", 0.7f);
            setParam(state, "ringsBrightness", 0.3f);
            setParam(state, "ringsDamping", 0.8f);
            setParam(state, "ringsMix", 0.5f);
            setParam(state, "attack", 1.0f);
            setParam(state, "decay", 0.6f);
            setParam(state, "sustain", 0.9f);
            setParam(state, "release", 2.5f);
            setParam(state, "filterCutoff", 5000.0f);
            setParam(state, "filterResonance", 0.2f);
            setParam(state, "reverbSize", 0.9f);
            setParam(state, "reverbMix", 0.6f);

            Preset preset("Dark Pad", "Pad", state, true);
            preset.author = "Factory";
            preset.tags.add("pad");
            preset.tags.add("dark");
            preset.tags.add("moody");
            preset.description = "Dark atmospheric pad";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.5f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Octave", 2.0f);
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.4f);
            setParam(state, "attack", 0.6f);
            setParam(state, "decay", 0.5f);
            setParam(state, "sustain", 0.75f);
            setParam(state, "release", 1.0f);
            setParam(state, "filterCutoff", 4500.0f);
            setParam(state, "filterResonance", 0.3f);
            setParam(state, "filterEnv", 0.4f);
            setParam(state, "chorusRate", 0.3f);
            setParam(state, "chorusDepth", 0.5f);
            setParam(state, "chorusMix", 0.3f);

            Preset preset("Chorus Pad", "Pad", state, true);
            preset.author = "Factory";
            preset.tags.add("pad");
            preset.tags.add("chorus");
            preset.tags.add("swirling");
            preset.description = "Swirling chorus pad";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 4.0f);  // Pulse
            setParam(state, "osc1PW", 0.15f);
            setParam(state, "osc1Mix", 0.7f);
            setParam(state, "osc2Wave", 4.0f);  // Pulse
            setParam(state, "osc2PW", 0.85f);
            setParam(state, "osc2Fine", 3.0f);
            setParam(state, "osc2Mix", 0.6f);
            setParam(state, "attack", 0.8f);
            setParam(state, "decay", 0.4f);
            setParam(state, "sustain", 0.8f);
            setParam(state, "release", 1.3f);
            setParam(state, "filterCutoff", 3200.0f);
            setParam(state, "filterResonance", 0.35f);
            setParam(state, "filterEnv", 0.45f);

            Preset preset("Hollow Pad", "Pad", state, true);
            preset.author = "Factory";
            preset.tags.add("pad");
            preset.tags.add("hollow");
            preset.tags.add("spacious");
            preset.description = "Hollow spacious pad";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 1.0f);  // Clouds only
            setParam(state, "cloudsDensity", 0.7f);
            setParam(state, "cloudsSize", 0.9f);
            setParam(state, "cloudsTexture", 0.4f);
            setParam(state, "cloudsPitch", 0.3f);
            setParam(state, "attack", 2.0f);
            setParam(state, "decay", 1.0f);
            setParam(state, "sustain", 0.8f);
            setParam(state, "release", 3.0f);
            setParam(state, "filterCutoff", 6000.0f);
            setParam(state, "filterResonance", 0.2f);
            setParam(state, "reverbSize", 0.9f);
            setParam(state, "reverbMix", 0.7f);

            Preset preset("Texture Pad", "Pad", state, true);
            preset.author = "Factory";
            preset.tags.add("pad");
            preset.tags.add("textured");
            preset.tags.add("evolving");
            preset.description = "Textured evolving pad";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Octave", -1.0f);
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "osc2Wave", 0.0f);  // Sine
            setParam(state, "osc2Octave", 1.0f);
            setParam(state, "osc2Fine", 3.0f);
            setParam(state, "osc2Mix", 0.3f);
            setParam(state, "attack", 1.5f);
            setParam(state, "decay", 0.7f);
            setParam(state, "sustain", 0.9f);
            setParam(state, "release", 2.0f);
            setParam(state, "filterCutoff", 2000.0f);
            setParam(state, "filterResonance", 0.25f);
            setParam(state, "filterEnv", 0.3f);
            setParam(state, "reverbSize", 0.8f);
            setParam(state, "reverbMix", 0.5f);

            Preset preset("Sub Pad", "Pad", state, true);
            preset.author = "Factory";
            preset.tags.add("pad");
            preset.tags.add("sub");
            preset.tags.add("deep");
            preset.description = "Deep sub bass pad";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "osc2Wave", 0.0f);  // Sine
            setParam(state, "osc2Octave", 3.0f);
            setParam(state, "osc2Mix", 0.2f);
            setParam(state, "attack", 0.4f);
            setParam(state, "decay", 0.3f);
            setParam(state, "sustain", 0.85f);
            setParam(state, "release", 1.0f);
            setParam(state, "filterCutoff", 8000.0f);
            setParam(state, "filterResonance", 0.2f);
            setParam(state, "filterEnv", 0.25f);
            setParam(state, "reverbSize", 0.6f);
            setParam(state, "reverbMix", 0.4f);
            setParam(state, "reverbShimmer", 0.7f);

            Preset preset("Bright Pad", "Pad", state, true);
            preset.author = "Factory";
            preset.tags.add("pad");
            preset.tags.add("bright");
            preset.tags.add("shimmering");
            preset.description = "Bright shimmering pad";
            presets.push_back(preset);
        }

        // =================================================================
        // ADDITIONAL LEAD VARIATIONS (10)
        // =================================================================

        // LEAD 5: Distorted Lead
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.9f);
            setParam(state, "osc2Wave", 2.0f);  // Square
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.7f);
            setParam(state, "attack", 0.01f);
            setParam(state, "decay", 0.15f);
            setParam(state, "sustain", 0.7f);
            setParam(state, "release", 0.3f);
            setParam(state, "filterCutoff", 6000.0f);
            setParam(state, "filterResonance", 0.6f);
            setParam(state, "filterEnv", 0.8f);

            Preset preset("Distorted Lead", "Lead", state, true);
            preset.author = "Factory";
            preset.tags.add("lead");
            preset.tags.add("distorted");
            preset.tags.add("aggressive");
            preset.description = "Aggressive distorted lead";
            presets.push_back(preset);
        }

        // LEAD 6: Retro Lead
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 4.0f);  // Pulse
            setParam(state, "osc1PW", 0.2f);
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "osc2Wave", 4.0f);  // Pulse
            setParam(state, "osc2PW", 0.8f);
            setParam(state, "osc2Fine", 5.0f);
            setParam(state, "osc2Mix", 0.6f);
            setParam(state, "attack", 0.02f);
            setParam(state, "decay", 0.2f);
            setParam(state, "sustain", 0.6f);
            setParam(state, "release", 0.3f);
            setParam(state, "filterCutoff", 4000.0f);
            setParam(state, "filterResonance", 0.5f);
            setParam(state, "filterEnv", 0.7f);

            Preset preset("Retro Lead", "Lead", state, true);
            preset.author = "Factory";
            preset.tags.add("lead");
            preset.tags.add("retro");
            preset.tags.add("80s");
            preset.description = "Retro 80s style lead";
            presets.push_back(preset);
        }

        // Continue with 8 more lead variations...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "ringsStructure", 0.6f);
            setParam(state, "ringsBrightness", 0.7f);
            setParam(state, "ringsDamping", 0.4f);
            setParam(state, "ringsMix", 0.4f);
            setParam(state, "attack", 0.05f);
            setParam(state, "decay", 0.25f);
            setParam(state, "sustain", 0.7f);
            setParam(state, "release", 0.4f);
            setParam(state, "filterCutoff", 5500.0f);
            setParam(state, "filterResonance", 0.4f);
            setParam(state, "filterEnv", 0.6f);

            Preset preset("Bell Lead", "Lead", state, true);
            preset.author = "Factory";
            preset.tags.add("lead");
            preset.tags.add("bell");
            preset.tags.add("metallic");
            preset.description = "Metallic bell-like lead";
            presets.push_back(preset);
        }

        // Add remaining lead variations...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 7.0f);  // Osc + Clouds
            setParam(state, "osc1Wave", 3.0f);  // Triangle
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "cloudsDensity", 0.3f);
            setParam(state, "cloudsSize", 0.4f);
            setParam(state, "cloudsTexture", 0.6f);
            setParam(state, "grainsMix", 0.4f);
            setParam(state, "attack", 0.08f);
            setParam(state, "decay", 0.3f);
            setParam(state, "sustain", 0.75f);
            setParam(state, "release", 0.5f);
            setParam(state, "filterCutoff", 7000.0f);
            setParam(state, "filterResonance", 0.3f);
            setParam(state, "filterEnv", 0.5f);

            Preset preset("Granular Lead", "Lead", state, true);
            preset.author = "Factory";
            preset.tags.add("lead");
            preset.tags.add("granular");
            preset.tags.add("textured");
            preset.description = "Textured granular lead";
            presets.push_back(preset);
        }

        // Add 6 more lead presets...
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Octave", 1.0f);
            setParam(state, "osc2Fine", 7.0f);
            setParam(state, "osc2Mix", 0.4f);
            setParam(state, "attack", 0.03f);
            setParam(state, "decay", 0.2f);
            setParam(state, "sustain", 0.8f);
            setParam(state, "release", 0.4f);
            setParam(state, "filterCutoff", 8000.0f);
            setParam(state, "filterResonance", 0.25f);
            setParam(state, "filterEnv", 0.4f);

            Preset preset("Smooth Lead", "Lead", state, true);
            preset.author = "Factory";
            preset.tags.add("lead");
            preset.tags.add("smooth");
            preset.tags.add("pure");
            preset.description = "Smooth pure lead tone";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 2.0f);  // Square
            setParam(state, "osc1Mix", 0.7f);
            setParam(state, "osc2Wave", 1.0f);  // Saw
            setParam(state, "osc2Octave", 1.0f);
            setParam(state, "osc2Fine", 12.0f);
            setParam(state, "osc2Mix", 0.5f);
            setParam(state, "attack", 0.01f);
            setParam(state, "decay", 0.18f);
            setParam(state, "sustain", 0.65f);
            setParam(state, "release", 0.25f);
            setParam(state, "filterCutoff", 4800.0f);
            setParam(state, "filterResonance", 0.6f);
            setParam(state, "filterEnv", 0.75f);

            Preset preset("Sync Lead", "Lead", state, true);
            preset.author = "Factory";
            preset.tags.add("lead");
            preset.tags.add("sync");
            preset.tags.add("harsh");
            preset.description = "Hard sync lead sound";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Octave", 2.0f);
            setParam(state, "osc2Mix", 0.3f);
            setParam(state, "attack", 0.06f);
            setParam(state, "decay", 0.2f);
            setParam(state, "sustain", 0.7f);
            setParam(state, "release", 0.4f);
            setParam(state, "filterCutoff", 6500.0f);
            setParam(state, "filterResonance", 0.35f);
            setParam(state, "filterEnv", 0.55f);
            setParam(state, "delayTime", 375.0f);
            setParam(state, "delayFeedback", 0.4f);
            setParam(state, "delayMix", 0.3f);

            Preset preset("Echo Lead", "Lead", state, true);
            preset.author = "Factory";
            preset.tags.add("lead");
            preset.tags.add("echo");
            preset.tags.add("spatial");
            preset.description = "Spatial lead with echo";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 4.0f);  // Pulse
            setParam(state, "osc1PW", 0.1f);
            setParam(state, "osc1Mix", 0.8f);
            setParam(state, "osc2Wave", 4.0f);  // Pulse
            setParam(state, "osc2PW", 0.9f);
            setParam(state, "osc2Fine", 3.0f);
            setParam(state, "osc2Mix", 0.7f);
            setParam(state, "attack", 0.02f);
            setParam(state, "decay", 0.15f);
            setParam(state, "sustain", 0.6f);
            setParam(state, "release", 0.2f);
            setParam(state, "filterCutoff", 7500.0f);
            setParam(state, "filterResonance", 0.7f);
            setParam(state, "filterEnv", 0.8f);

            Preset preset("Nasal Lead", "Lead", state, true);
            preset.author = "Factory";
            preset.tags.add("lead");
            preset.tags.add("nasal");
            preset.tags.add("cutting");
            preset.description = "Cutting nasal lead tone";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 2.0f);  // Karplus only
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.3f);
            setParam(state, "sustain", 0.5f);
            setParam(state, "release", 0.4f);
            setParam(state, "filterCutoff", 9000.0f);
            setParam(state, "filterResonance", 0.3f);
            setParam(state, "filterEnv", 0.4f);
            setParam(state, "delayTime", 250.0f);
            setParam(state, "delayFeedback", 0.3f);
            setParam(state, "delayMix", 0.25f);

            Preset preset("Pluck Lead", "Lead", state, true);
            preset.author = "Factory";
            preset.tags.add("lead");
            preset.tags.add("pluck");
            preset.tags.add("percussive");
            preset.description = "Percussive plucked lead";
            presets.push_back(preset);
        }

        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.5f);
            setParam(state, "osc2Wave", 1.0f);  // Saw
            setParam(state, "osc2Octave", 1.0f);
            setParam(state, "osc2Fine", -5.0f);
            setParam(state, "osc2Mix", 0.5f);
            setParam(state, "attack", 0.04f);
            setParam(state, "decay", 0.25f);
            setParam(state, "sustain", 0.7f);
            setParam(state, "release", 0.4f);
            setParam(state, "filterCutoff", 5000.0f);
            setParam(state, "filterResonance", 0.45f);
            setParam(state, "filterEnv", 0.65f);
            setParam(state, "chorusRate", 1.2f);
            setParam(state, "chorusDepth", 0.4f);
            setParam(state, "chorusMix", 0.25f);

            Preset preset("Detuned Lead", "Lead", state, true);
            preset.author = "Factory";
            preset.tags.add("lead");
            preset.tags.add("detuned");
            preset.tags.add("wide");
            preset.description = "Wide detuned lead sound";
            presets.push_back(preset);
        }


        // FINAL 5 PRESETS - Specialty sounds to complete the 100 preset collection

        // SPECIALTY 1: Vocal Formant
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.7f);
            setParam(state, "osc2Wave", 2.0f);  // Square
            setParam(state, "osc2Octave", 1.0f);
            setParam(state, "osc2Mix", 0.5f);
            setParam(state, "attack", 0.1f);
            setParam(state, "decay", 0.3f);
            setParam(state, "sustain", 0.8f);
            setParam(state, "release", 0.4f);
            setParam(state, "filterCutoff", 2500.0f);
            setParam(state, "filterResonance", 0.8f);
            setParam(state, "filterEnv", 0.6f);
            setParam(state, "chorusRate", 0.8f);
            setParam(state, "chorusDepth", 0.3f);
            setParam(state, "chorusMix", 0.2f);
            Preset preset("Vocal Formant", "FX", state, true);
            preset.author = "Factory";
            preset.tags.add("vocal");
            preset.tags.add("formant");
            preset.tags.add("human");
            preset.description = "Human vocal formant synthesis";
            presets.push_back(preset);
        }

        // SPECIALTY 2: Glitch Percussion
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 7.0f);  // Osc + Clouds
            setParam(state, "osc1Wave", 5.0f);  // Noise
            setParam(state, "osc1Mix", 0.4f);
            setParam(state, "cloudsDensity", 0.9f);
            setParam(state, "cloudsSize", 0.1f);
            setParam(state, "cloudsTexture", 0.9f);
            setParam(state, "grainsMix", 0.8f);
            setParam(state, "attack", 0.001f);
            setParam(state, "decay", 0.08f);
            setParam(state, "sustain", 0.1f);
            setParam(state, "release", 0.15f);
            setParam(state, "filterCutoff", 8000.0f);
            setParam(state, "filterResonance", 0.6f);
            setParam(state, "filterEnv", 0.8f);
            setParam(state, "distortion", 0.5f);
            Preset preset("Glitch Percussion", "FX", state, true);
            preset.author = "Factory";
            preset.tags.add("glitch");
            preset.tags.add("percussion");
            preset.tags.add("digital");
            preset.description = "Glitchy digital percussion";
            presets.push_back(preset);
        }

        // SPECIALTY 3: Drone Machine
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Octave", -1.0f);
            setParam(state, "osc1Mix", 0.6f);
            setParam(state, "osc2Wave", 1.0f);  // Saw
            setParam(state, "osc2Octave", -1.0f);
            setParam(state, "osc2Fine", 3.0f);
            setParam(state, "osc2Mix", 0.6f);
            setParam(state, "ringsStructure", 0.4f);
            setParam(state, "ringsBrightness", 0.3f);
            setParam(state, "ringsDamping", 0.8f);
            setParam(state, "ringsMix", 0.5f);
            setParam(state, "attack", 1.0f);
            setParam(state, "decay", 0.5f);
            setParam(state, "sustain", 1.0f);
            setParam(state, "release", 2.0f);
            setParam(state, "filterCutoff", 1200.0f);
            setParam(state, "filterResonance", 0.5f);
            setParam(state, "filterEnv", 0.2f);
            setParam(state, "reverbSize", 0.9f);
            setParam(state, "reverbMix", 0.4f);
            Preset preset("Drone Machine", "Pad", state, true);
            preset.author = "Factory";
            preset.tags.add("drone");
            preset.tags.add("machine");
            preset.tags.add("industrial");
            preset.description = "Industrial drone machine";
            presets.push_back(preset);
        }

        // SPECIALTY 4: Cosmic Bell
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 6.0f);  // Osc + Rings
            setParam(state, "osc1Wave", 0.0f);  // Sine
            setParam(state, "osc1Mix", 0.3f);
            setParam(state, "ringsStructure", 0.9f);
            setParam(state, "ringsBrightness", 0.8f);
            setParam(state, "ringsDamping", 0.2f);
            setParam(state, "ringsMix", 0.9f);
            setParam(state, "attack", 0.01f);
            setParam(state, "decay", 4.0f);
            setParam(state, "sustain", 0.3f);
            setParam(state, "release", 8.0f);
            setParam(state, "filterCutoff", 12000.0f);
            setParam(state, "filterResonance", 0.2f);
            setParam(state, "filterEnv", 0.3f);
            setParam(state, "delayTime", 875.0f);
            setParam(state, "delayFeedback", 0.7f);
            setParam(state, "delayMix", 0.5f);
            setParam(state, "chorusRate", 0.1f);
            setParam(state, "chorusDepth", 0.4f);
            setParam(state, "chorusMix", 0.3f);
            setParam(state, "reverbSize", 0.95f);
            setParam(state, "reverbMix", 0.6f);
            Preset preset("Cosmic Bell", "Bells", state, true);
            preset.author = "Factory";
            preset.tags.add("cosmic");
            preset.tags.add("bell");
            preset.tags.add("ethereal");
            preset.description = "Ethereal cosmic bell";
            presets.push_back(preset);
        }

        // SPECIALTY 5: Vintage Keys
        {
            auto state = parameters.copyState();
            setParam(state, "engineMode", 5.0f);
            setParam(state, "osc1Wave", 1.0f);  // Saw
            setParam(state, "osc1Mix", 0.5f);
            setParam(state, "osc2Wave", 3.0f);  // Triangle
            setParam(state, "osc2Octave", -1.0f);
            setParam(state, "osc2Mix", 0.6f);
            setParam(state, "attack", 0.02f);
            setParam(state, "decay", 0.8f);
            setParam(state, "sustain", 0.7f);
            setParam(state, "release", 1.5f);
            setParam(state, "filterCutoff", 4000.0f);
            setParam(state, "filterResonance", 0.3f);
            setParam(state, "filterEnv", 0.4f);
            setParam(state, "chorusRate", 0.6f);
            setParam(state, "chorusDepth", 0.5f);
            setParam(state, "chorusMix", 0.3f);
            setParam(state, "reverbSize", 0.6f);
            setParam(state, "reverbMix", 0.25f);
            Preset preset("Vintage Keys", "Keys", state, true);
            preset.author = "Factory";
            preset.tags.add("vintage");
            preset.tags.add("keys");
            preset.tags.add("retro");
            preset.description = "Vintage electric piano sound";
            presets.push_back(preset);
        }

        // Save all factory presets to disk
        savePresetsToDisk();
    }

    juce::AudioProcessorValueTreeState& parameters;
    std::vector<Preset> presets;
    juce::StringArray categories;
    int currentPresetIndex = -1;
};
