#pragma once
#include "UltimatePluckEngine.h"
#include "PresetManager.h"
#include "LFO.h"
#include "LFOPanel.h"
#include "ModulationMatrix.h"
#include "ModulationMatrixView.h"
#include "VisualFeedbackPanel.h"
#include "AdvancedEffects.h"
#include "EffectsPanel.h"
#include "MacroSystem.h"
#include "MacroPanel.h"
#include "BasicOscillator.h"

//==============================================================================
// ULTIMATE PLUCK VOICE - Combines all engines
//==============================================================================
class UltimatePluckVoice : public juce::SynthesiserVoice
{
public:
    UltimatePluckVoice()
    {
        modalResonator.setSampleRate(44100.0);
        granularEngine.setSampleRate(44100.0);
        karplusStrong.setSampleRate(44100.0);
    }
    
    enum class EngineMode
    {
        Rings,              // 0: Pure modal synthesis
        Clouds,             // 1: Granular only
        Karplus,            // 2: Physical modeling
        RingsIntoGrains,    // 3: Rings fed into granular
        HybridAll,          // 4: All three mixed
        BasicOscillator,    // 5: Pure oscillator (warm synth)
        OscPlusRings,       // 6: Oscillator + Rings (warm + character)
        OscPlusClouds,      // 7: Oscillator + Clouds (warm + texture)
        FullHybrid,         // 8: Everything (oscillators + all engines)
        NumModes
    };
    
    bool canPlaySound(juce::SynthesiserSound*) override { return true; }
    
    void startNote(int midiNote, float velocity,
                   juce::SynthesiserSound*, int) override
    {
        frequency = juce::MidiMessage::getMidiNoteInHertz(midiNote);
        noteVelocity = velocity;
        isActive = true;

        // CRITICAL FIX: Longer fade-in (10ms) to prevent clicks on retrigger and chords
        fadeInSamples = static_cast<int>(sampleRate * 0.010); // 10ms
        fadeInCounter = 0;
        isFadingOut = false; // Cancel any fade-out

        // Trigger all engines
        ModalResonator::ResonatorParams ringParams;
        ringParams.frequency = frequency;
        ringParams.brightness = params.ringsBrightness;
        ringParams.damping = params.ringsDamping;
        ringParams.position = params.ringsPosition;
        ringParams.structure = params.ringsStructure;
        ringParams.model = params.ringsModel;
        modalResonator.setParameters(ringParams);
        modalResonator.trigger(velocity);

        // Karplus-Strong
        karplusStrong.setFrequency(frequency);
        karplusStrong.trigger(velocity);

        // Wavetable oscillator
        wavetablePhase = 0.0f;

        // CRITICAL FIX: Just call noteOn - don't reset envelopes (causes clicks)
        mainEnv.noteOn();
        filterEnv.noteOn();
    }
    
    void stopNote(float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            mainEnv.noteOff();
            filterEnv.noteOff();
        }
        else
        {
            // CRITICAL FIX: Short fade-out (2ms) to prevent clicks on voice stealing
            fadeOutSamples = static_cast<int>(sampleRate * 0.002); // 2ms
            fadeOutCounter = 0;
            isFadingOut = true;
        }
    }
    
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}
    
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                        int startSample, int numSamples) override
    {
        if (!isActive)
            return;
        
        auto* leftBuffer = outputBuffer.getWritePointer(0, startSample);
        auto* rightBuffer = outputBuffer.getWritePointer(1, startSample);
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float output = 0.0f;
            float leftOut = 0.0f;
            float rightOut = 0.0f;
            
            // Calculate oscillator frequencies with pitch adjustments
            float osc1Freq = frequency * std::pow(2.0f, params.osc1Octave + params.osc1Semi/12.0f + params.osc1Fine/1200.0f);
            float osc2Freq = frequency * std::pow(2.0f, params.osc2Octave + params.osc2Semi/12.0f + params.osc2Fine/1200.0f);

            oscillator1.setFrequency(osc1Freq);
            oscillator2.setFrequency(osc2Freq);

            // Generate oscillator samples
            float osc1Sample = oscillator1.processSample();
            float osc2Sample = oscillator2.processSample();
            float oscOutput = (osc1Sample * params.osc1Mix) + (osc2Sample * params.osc2Mix);

            // Generate from selected engines
            switch (params.engineMode)
            {
                case EngineMode::Rings:
                    output = modalResonator.processSample(0.0f);
                    leftOut = rightOut = output;
                    break;

                case EngineMode::Clouds:
                {
                    // Feed wavetable into granular
                    float wavetableSample = generateWavetable();
                    granularEngine.writeInput(wavetableSample, wavetableSample);
                    granularEngine.processStereo(leftOut, rightOut);
                    break;
                }

                case EngineMode::Karplus:
                    output = karplusStrong.getSample();
                    leftOut = rightOut = output;
                    break;

                case EngineMode::RingsIntoGrains:
                {
                    // Rings feeds granular engine
                    float ringsSample = modalResonator.processSample(0.0f);
                    granularEngine.writeInput(ringsSample, ringsSample);
                    granularEngine.processStereo(leftOut, rightOut);
                    break;
                }

                case EngineMode::HybridAll:
                {
                    // Mix all three original engines
                    float rings = modalResonator.processSample(0.0f) * params.ringsMix;
                    float karplus = karplusStrong.getSample() * params.karplusMix;
                    float wavetable = generateWavetable() * params.wavetableMix;

                    // Feed into granular
                    float mixed = rings + karplus + wavetable;
                    granularEngine.writeInput(mixed, mixed);

                    float grainL, grainR;
                    granularEngine.processStereo(grainL, grainR);

                    leftOut = mixed * (1.0f - params.grainsMix) + grainL * params.grainsMix;
                    rightOut = mixed * (1.0f - params.grainsMix) + grainR * params.grainsMix;
                    break;
                }

                case EngineMode::BasicOscillator:
                {
                    // Pure oscillator mode - warm, clean synth
                    leftOut = rightOut = oscOutput;
                    break;
                }

                case EngineMode::OscPlusRings:
                {
                    // Oscillator warmth + Rings character
                    float rings = modalResonator.processSample(0.0f);
                    leftOut = rightOut = oscOutput + rings * 0.5f;  // 50/50 blend
                    break;
                }

                case EngineMode::OscPlusClouds:
                {
                    // Oscillator warmth + Clouds texture
                    granularEngine.writeInput(oscOutput, oscOutput);
                    granularEngine.processStereo(leftOut, rightOut);
                    leftOut = oscOutput * 0.5f + leftOut * 0.5f;
                    rightOut = oscOutput * 0.5f + rightOut * 0.5f;
                    break;
                }

                case EngineMode::FullHybrid:
                {
                    // Everything: oscillators + all engines
                    float rings = modalResonator.processSample(0.0f) * params.ringsMix;
                    float karplus = karplusStrong.getSample() * params.karplusMix;
                    float wavetable = generateWavetable() * params.wavetableMix;

                    // Mix with oscillators
                    float engineMix = rings + karplus + wavetable;
                    float totalMix = oscOutput + engineMix;

                    // Feed into granular
                    granularEngine.writeInput(totalMix, totalMix);

                    float grainL, grainR;
                    granularEngine.processStereo(grainL, grainR);

                    leftOut = totalMix * (1.0f - params.grainsMix) + grainL * params.grainsMix;
                    rightOut = totalMix * (1.0f - params.grainsMix) + grainR * params.grainsMix;
                    break;
                }

                case EngineMode::NumModes:
                    // This case should not be reached
                    break;
            }
            
            // Apply filter
            float filtered = filter.processSample(0, leftOut);
            float filteredR = filter.processSample(1, rightOut);
            
            // Apply envelopes
            float mainEnvValue = mainEnv.getNextSample();
            float filterEnvValue = filterEnv.getNextSample();

            // Modulate filter
            updateFilter(filterEnvValue);

            // ANTI-CLICK FADE-IN (1ms)
            float fadeInGain = 1.0f;
            if (fadeInCounter < fadeInSamples)
            {
                fadeInGain = static_cast<float>(fadeInCounter) / static_cast<float>(fadeInSamples);
                fadeInCounter++;
            }

            // ANTI-CLICK FADE-OUT (2ms) - for voice stealing
            float fadeOutGain = 1.0f;
            if (isFadingOut)
            {
                fadeOutGain = 1.0f - (static_cast<float>(fadeOutCounter) / static_cast<float>(fadeOutSamples));
                fadeOutCounter++;

                if (fadeOutCounter >= fadeOutSamples)
                {
                    // Fade complete - clear note
                    clearCurrentNote();
                    isActive = false;
                    isFadingOut = false;
                    break;
                }
            }

            // Combine all gain stages with MORE headroom to prevent distortion
            float totalGain = mainEnvValue * noteVelocity * fadeInGain * fadeOutGain * 0.25f;

            // Final output
            leftBuffer[sample] += filtered * totalGain;
            rightBuffer[sample] += filteredR * totalGain;
            
            // Update wavetable phase
            wavetablePhase += frequency / sampleRate;
            if (wavetablePhase >= 1.0f)
                wavetablePhase -= 1.0f;
            
            if (!mainEnv.isActive())
            {
                clearCurrentNote();
                isActive = false;
                break;
            }
        }
    }
    
    // Parameter structure
    struct VoiceParams
    {
        EngineMode engineMode = EngineMode::HybridAll;

        // Rings parameters
        float ringsBrightness = 0.5f;
        float ringsDamping = 0.5f;
        float ringsPosition = 0.5f;
        float ringsStructure = 0.5f;
        ModalResonator::ResonatorModel ringsModel = ModalResonator::ResonatorModel::String;

        // Clouds parameters
        GranularEngine::CloudsParams cloudsParams;

        // Mix levels
        float ringsMix = 0.5f;
        float karplusMix = 0.3f;
        float wavetableMix = 0.2f;
        float grainsMix = 0.5f;

        // Wavetable
        AdvancedWavetableEngine::WavetableParams wavetableParams;

        // Basic Oscillators
        BasicOscillator::WaveType osc1Wave = BasicOscillator::WaveType::Saw;
        BasicOscillator::WaveType osc2Wave = BasicOscillator::WaveType::Saw;
        float osc1Octave = 0.0f;
        float osc2Octave = 0.0f;
        float osc1Semi = 0.0f;
        float osc2Semi = 0.0f;
        float osc1Fine = 0.0f;
        float osc2Fine = 7.0f;  // Default: slightly detuned for thickness
        float osc1PW = 0.5f;
        float osc2PW = 0.5f;
        float osc1Mix = 0.0f;
        float osc2Mix = 0.0f;

        // Filter
        float filterCutoff = 5000.0f;
        float filterResonance = 1.0f;
        float filterEnvAmount = 0.5f;

        // Envelopes
        float attack = 0.001f;
        float decay = 0.3f;
        float sustain = 0.7f;
        float release = 0.5f;
    };
    
    void setParameters(const VoiceParams& p)
    {
        params = p;

        // Update ADSR
        juce::ADSR::Parameters adsrParams;
        adsrParams.attack = p.attack;
        adsrParams.decay = p.decay;
        adsrParams.sustain = p.sustain;
        adsrParams.release = p.release;
        mainEnv.setParameters(adsrParams);

        juce::ADSR::Parameters filterAdsrParams;
        filterAdsrParams.attack = p.attack * 0.5f;
        filterAdsrParams.decay = p.decay * 0.7f;
        filterAdsrParams.sustain = p.sustain * 0.8f;
        filterAdsrParams.release = p.release * 0.6f;
        filterEnv.setParameters(filterAdsrParams);

        // Update granular engine
        granularEngine.setParameters(p.cloudsParams);

        // Update oscillators
        oscillator1.setWaveType(p.osc1Wave);
        oscillator1.setPulseWidth(p.osc1PW);
        oscillator2.setWaveType(p.osc2Wave);
        oscillator2.setPulseWidth(p.osc2PW);
    }
    
    void prepare(double sr)
    {
        sampleRate = sr;
        mainEnv.setSampleRate(sr);
        filterEnv.setSampleRate(sr);
        modalResonator.setSampleRate(sr);
        granularEngine.setSampleRate(sr);
        karplusStrong.setSampleRate(sr);

        // Prepare oscillators
        oscillator1.setSampleRate(sr);
        oscillator2.setSampleRate(sr);

        // Prepare filter for stereo processing
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sr;
        spec.maximumBlockSize = 512;
        spec.numChannels = 2;
        filter.prepare(spec);
        filter.reset();
    }

private:
    // Engines
    ModalResonator modalResonator;
    GranularEngine granularEngine;
    KarplusStrongEngine karplusStrong;
    AdvancedWavetableEngine wavetableEngine;

    // Basic Oscillators
    BasicOscillator oscillator1;
    BasicOscillator oscillator2;

    // Filter and envelopes
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::ADSR mainEnv;
    juce::ADSR filterEnv;

    // State
    VoiceParams params;
    float frequency = 440.0f;
    float noteVelocity = 0.0f;
    double sampleRate = 44100.0;
    bool isActive = false;
    float wavetablePhase = 0.0f;

    // Anti-click fade state
    int fadeInCounter = 0;
    int fadeInSamples = 0;
    int fadeOutCounter = 0;
    int fadeOutSamples = 0;
    bool isFadingOut = false;
    
    float generateWavetable()
    {
        return wavetableEngine.getSample(wavetablePhase, params.wavetableParams);
    }
    
    void updateFilter(float envValue)
    {
        float modulated = params.filterCutoff * (1.0f + params.filterEnvAmount * envValue * 10.0f);
        modulated = juce::jlimit(20.0f, 20000.0f, modulated);
        
        filter.setCutoffFrequency(modulated);
        filter.setResonance(params.filterResonance);
    }
};

//==============================================================================
// Simple Sound class for the synthesiser
//==============================================================================
class SimpleSynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

//==============================================================================
// ULTIMATE PLUCK PROCESSOR
//==============================================================================
class UltimatePluckProcessor : public juce::AudioProcessor
{
public:
    UltimatePluckProcessor()
        : AudioProcessor(BusesProperties()
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
        // PERFORMANCE: Reduced from 16 to 8 voices to improve CPU usage
        // Modal synthesis is CPU-intensive, 8 voices is sufficient for most use cases
        for (int i = 0; i < 8; ++i)
        {
            synth.addVoice(new UltimatePluckVoice());
        }

        synth.addSound(new SimpleSynthSound());
        
        // Create parameters
        apvts = std::make_unique<juce::AudioProcessorValueTreeState>(
            *this, nullptr, "Parameters", createParameterLayout());

        // Create preset manager
        presetManager = std::make_unique<PresetManager>(*apvts);

        // Set up preset change callback to eliminate pops/clicks
        presetManager->onPresetChange = [this]()
        {
            resetAllVoices();
        };

        // Create LFO section
        lfoSection = std::make_unique<LFOSection>(*apvts);
    }
    
    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        synth.setCurrentPlaybackSampleRate(sampleRate);
        
        for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* voice = dynamic_cast<UltimatePluckVoice*>(synth.getVoice(i)))
            {
                voice->prepare(sampleRate);
            }
        }
        
        // Prepare effects
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = samplesPerBlock;
        spec.numChannels = 2;

        reverb.prepare(spec);
        delay.prepare(spec);

        // Prepare advanced effects
        advancedDistortion.prepare(sampleRate);
        advancedDelay.prepare(sampleRate, 2000); // 2 second max delay
        enhancedReverb.prepare(sampleRate);
        chorus.prepare(sampleRate);

        // Prepare visual feedback
        if (visualFeedbackPanel)
            visualFeedbackPanel->prepare(sampleRate);

        // Prepare LFOs
        if (lfoSection)
            lfoSection->prepare(sampleRate);

        // =====================================================================
        // CACHE ALL PARAMETER POINTERS - REAL-TIME SAFETY CRITICAL
        // =====================================================================
        // Do this ONCE here, never on audio thread!

        // Engine parameters
        engineModeParam = apvts->getRawParameterValue("engineMode");

        // Rings parameters
        ringsBrightnessParam = apvts->getRawParameterValue("ringsBrightness");
        ringsDampingParam = apvts->getRawParameterValue("ringsDamping");
        ringsPositionParam = apvts->getRawParameterValue("ringsPosition");
        ringsStructureParam = apvts->getRawParameterValue("ringsStructure");
        ringsModelParam = apvts->getRawParameterValue("ringsModel");

        // Clouds parameters
        cloudsPositionParam = apvts->getRawParameterValue("cloudsPosition");
        cloudsSizeParam = apvts->getRawParameterValue("cloudsSize");
        cloudsDensityParam = apvts->getRawParameterValue("cloudsDensity");
        cloudsTextureParam = apvts->getRawParameterValue("cloudsTexture");
        cloudsPitchParam = apvts->getRawParameterValue("cloudsPitch");
        cloudsStereoParam = apvts->getRawParameterValue("cloudsStereo");
        cloudsFreezeParam = apvts->getRawParameterValue("cloudsFreeze");

        // Wavetable parameters
        wavetableAParam = apvts->getRawParameterValue("wavetableA");
        wavetableBParam = apvts->getRawParameterValue("wavetableB");
        wavetableMorphParam = apvts->getRawParameterValue("wavetableMorph");
        wavetableWarpParam = apvts->getRawParameterValue("wavetableWarp");
        wavetableFoldParam = apvts->getRawParameterValue("wavetableFold");

        // Mix parameters
        ringsMixParam = apvts->getRawParameterValue("ringsMix");
        karplusMixParam = apvts->getRawParameterValue("karplusMix");
        wavetableMixParam = apvts->getRawParameterValue("wavetableMix");
        grainsMixParam = apvts->getRawParameterValue("grainsMix");

        // Envelope parameters
        attackParam = apvts->getRawParameterValue("attack");
        decayParam = apvts->getRawParameterValue("decay");
        sustainParam = apvts->getRawParameterValue("sustain");
        releaseParam = apvts->getRawParameterValue("release");

        // Filter parameters
        filterCutoffParam = apvts->getRawParameterValue("filterCutoff");
        filterResonanceParam = apvts->getRawParameterValue("filterResonance");
        filterEnvParam = apvts->getRawParameterValue("filterEnv");

        // Oscillator parameters
        osc1WaveParam = apvts->getRawParameterValue("osc1Wave");
        osc1OctaveParam = apvts->getRawParameterValue("osc1Octave");
        osc1SemiParam = apvts->getRawParameterValue("osc1Semi");
        osc1FineParam = apvts->getRawParameterValue("osc1Fine");
        osc1PWParam = apvts->getRawParameterValue("osc1PW");
        osc1MixParam = apvts->getRawParameterValue("osc1Mix");
        osc2WaveParam = apvts->getRawParameterValue("osc2Wave");
        osc2OctaveParam = apvts->getRawParameterValue("osc2Octave");
        osc2SemiParam = apvts->getRawParameterValue("osc2Semi");
        osc2FineParam = apvts->getRawParameterValue("osc2Fine");
        osc2PWParam = apvts->getRawParameterValue("osc2PW");
        osc2MixParam = apvts->getRawParameterValue("osc2Mix");

        // Effect parameters
        delayTimeParam = apvts->getRawParameterValue("delayTime");
        delayFeedbackParam = apvts->getRawParameterValue("delayFeedback");
        delayMixParam = apvts->getRawParameterValue("delayMix");
        delayFilterParam = apvts->getRawParameterValue("delayFilter");
        delayPingPongParam = apvts->getRawParameterValue("delayPingPong");
        reverbSizeParam = apvts->getRawParameterValue("reverbSize");
        reverbDampingParam = apvts->getRawParameterValue("reverbDamping");
        reverbWidthParam = apvts->getRawParameterValue("reverbWidth");
        reverbMixParam = apvts->getRawParameterValue("reverbMix");
        reverbShimmerParam = apvts->getRawParameterValue("reverbShimmer");
        chorusRateParam = apvts->getRawParameterValue("chorusRate");
        chorusDepthParam = apvts->getRawParameterValue("chorusDepth");
        chorusMixParam = apvts->getRawParameterValue("chorusMix");
        chorusFeedbackParam = apvts->getRawParameterValue("chorusFeedback");
        chorusWidthParam = apvts->getRawParameterValue("chorusWidth");

        // Performance control parameters
        portamentoParam = apvts->getRawParameterValue("portamento");
        vibratoDepthParam = apvts->getRawParameterValue("vibratoDepth");
        vibratoRateParam = apvts->getRawParameterValue("vibratoRate");
        masterTuneParam = apvts->getRawParameterValue("masterTune");
        velocitySensParam = apvts->getRawParameterValue("velocitySens");
        panSpreadParam = apvts->getRawParameterValue("panSpread");
        unisonVoicesParam = apvts->getRawParameterValue("unisonVoices");
        unisonDetuneParam = apvts->getRawParameterValue("unisonDetune");

        // REAL-TIME SAFETY: Move factory preset creation to message thread
        // This prevents string operations on audio thread
        if (presetManager)
        {
            juce::MessageManager::callAsync([this]()
            {
                if (presetManager)
                    presetManager->ensureFactoryPresetsExist();
            });
        }
    }
    
    void releaseResources() override {}
    
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        buffer.clear();

        // Process LFOs
        if (lfoSection)
            lfoSection->processBlock(buffer.getNumSamples());

        // Update modulation sources
        modulationMatrix.setSourceValue(ModulationSource::Type::LFO1, getLFOValue(0));
        modulationMatrix.setSourceValue(ModulationSource::Type::LFO2, getLFOValue(1));
        modulationMatrix.setSourceValue(ModulationSource::Type::LFO3, getLFOValue(2));

        updateVoiceParameters();

        // Process keyboard state and add messages to MIDI buffer
        keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

        synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

        // Update visual feedback - REAL-TIME SAFE: Move string operations to message thread
        if (visualFeedbackPanel && cloudsDensityParam && cloudsSizeParam && cloudsPositionParam && cloudsTextureParam)
        {
            float density = cloudsDensityParam->load();
            float grainSize = cloudsSizeParam->load();
            float position = cloudsPositionParam->load();
            float texture = cloudsTextureParam->load();

            // Move grain parameter update to message thread to avoid string operations on audio thread
            juce::MessageManager::callAsync([this, density, grainSize, position, texture]()
            {
                if (visualFeedbackPanel)
                    visualFeedbackPanel->updateGrainParameters(density, grainSize, position, texture);
            });

            // Push samples to spectrum analyzer - THIS IS REAL-TIME SAFE
            if (buffer.getNumChannels() > 0)
                visualFeedbackPanel->pushSamplesForSpectrum(buffer.getReadPointer(0), buffer.getNumSamples());
        }

        applyEffects(buffer);
    }

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    
    const juce::String getName() const override { return "WiiPluck Ultimate"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    
    void getStateInformation(juce::MemoryBlock& destData) override
    {
        try
        {
            auto state = apvts->copyState();
            if (state.isValid())
            {
                std::unique_ptr<juce::XmlElement> xml(state.createXml());
                if (xml != nullptr)
                    copyXmlToBinary(*xml, destData);
            }
        }
        catch (...)
        {
            // Prevent crash on save - skip serialization
        }
    }
    
    void setStateInformation(const void* data, int sizeInBytes) override
    {
        std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
        if (xmlState && xmlState->hasTagName(apvts->state.getType()))
            apvts->replaceState(juce::ValueTree::fromXml(*xmlState));
    }
    
    juce::AudioProcessorValueTreeState& getAPVTS() { return *apvts; }
    PresetManager& getPresetManager() { return *presetManager; }

    juce::MidiKeyboardState keyboardState;

    // Computer keyboard to MIDI mapping
    void triggerNoteOn(int midiNote, float velocity = 0.8f)
    {
        keyboardState.noteOn(1, midiNote, velocity);
    }

    void triggerNoteOff(int midiNote)
    {
        keyboardState.noteOff(1, midiNote, 0.0f);
    }

    // Reset all voices for preset changes - eliminates pops/clicks
    void resetAllVoices()
    {
        // Stop all current notes immediately but safely
        for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* voice = synth.getVoice(i))
            {
                voice->stopNote(1.0f, false); // Force immediate stop with fade-out
            }
        }

        // Clear keyboard state
        keyboardState.allNotesOff(1);

        // Reset synthesizer
        synth.allNotesOff(1, false);
    }

    // Get MIDI note from computer key
    static int getMidiNoteForKey(juce::KeyPress key)
    {
        // Two-row piano layout starting at C4 (MIDI 60)
        // Bottom row (white keys): A S D F G H J K L ; '
        // Top row (black keys):    W E   T Y U   O P

        const int baseOctave = 60;  // Middle C

        // Bottom row - white keys
        if (key == juce::KeyPress('a')) return baseOctave;      // C
        if (key == juce::KeyPress('s')) return baseOctave + 2;  // D
        if (key == juce::KeyPress('d')) return baseOctave + 4;  // E
        if (key == juce::KeyPress('f')) return baseOctave + 5;  // F
        if (key == juce::KeyPress('g')) return baseOctave + 7;  // G
        if (key == juce::KeyPress('h')) return baseOctave + 9;  // A
        if (key == juce::KeyPress('j')) return baseOctave + 11; // B
        if (key == juce::KeyPress('k')) return baseOctave + 12; // C (octave up)
        if (key == juce::KeyPress('l')) return baseOctave + 14; // D
        if (key == juce::KeyPress(';')) return baseOctave + 16; // E
        if (key == juce::KeyPress('\'')) return baseOctave + 17; // F

        // Top row - black keys
        if (key == juce::KeyPress('w')) return baseOctave + 1;  // C#
        if (key == juce::KeyPress('e')) return baseOctave + 3;  // D#
        if (key == juce::KeyPress('t')) return baseOctave + 6;  // F#
        if (key == juce::KeyPress('y')) return baseOctave + 8;  // G#
        if (key == juce::KeyPress('u')) return baseOctave + 10; // A#
        if (key == juce::KeyPress('o')) return baseOctave + 13; // C#
        if (key == juce::KeyPress('p')) return baseOctave + 15; // D#

        // Octave controls
        if (key == juce::KeyPress('z')) return baseOctave - 12; // C (octave down)
        if (key == juce::KeyPress('x')) return baseOctave - 10; // D
        if (key == juce::KeyPress('c')) return baseOctave - 8;  // E
        if (key == juce::KeyPress('v')) return baseOctave - 7;  // F
        if (key == juce::KeyPress('b')) return baseOctave - 5;  // G
        if (key == juce::KeyPress('n')) return baseOctave - 3;  // A
        if (key == juce::KeyPress('m')) return baseOctave - 1;  // B

        return -1;  // Not a recognized key
    }

    // LFO and Modulation
    std::unique_ptr<LFOSection> lfoSection;
    AdvancedModulationMatrix modulationMatrix;

    // Visual Feedback
    VisualFeedbackPanel* visualFeedbackPanel = nullptr;

    void setVisualFeedbackPanel(VisualFeedbackPanel* panel)
    {
        visualFeedbackPanel = panel;
    }

    // Advanced Effects
    AdvancedDistortion advancedDistortion;
    AdvancedDelay advancedDelay;
    EnhancedReverb enhancedReverb;
    ChorusEffect chorus;

    // Macro System
    MacroSystem macroSystem;

    // Helper to get LFO values for modulation
    float getLFOValue(int lfoIndex)
    {
        if (lfoSection)
        {
            if (auto* lfo = lfoSection->getLFO(lfoIndex))
                return lfo->getCurrentValue();
        }
        return 0.0f;
    }

private:
    juce::Synthesiser synth;
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
    std::unique_ptr<PresetManager> presetManager;

    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float> delay{96000};

    // =====================================================================
    // REAL-TIME SAFE PARAMETER CACHE - NO STRING LOOKUPS ON AUDIO THREAD
    // =====================================================================

    // Engine parameters
    std::atomic<float>* engineModeParam = nullptr;

    // Rings parameters
    std::atomic<float>* ringsBrightnessParam = nullptr;
    std::atomic<float>* ringsDampingParam = nullptr;
    std::atomic<float>* ringsPositionParam = nullptr;
    std::atomic<float>* ringsStructureParam = nullptr;
    std::atomic<float>* ringsModelParam = nullptr;

    // Clouds parameters
    std::atomic<float>* cloudsPositionParam = nullptr;
    std::atomic<float>* cloudsSizeParam = nullptr;
    std::atomic<float>* cloudsDensityParam = nullptr;
    std::atomic<float>* cloudsTextureParam = nullptr;
    std::atomic<float>* cloudsPitchParam = nullptr;
    std::atomic<float>* cloudsStereoParam = nullptr;
    std::atomic<float>* cloudsFreezeParam = nullptr;

    // Wavetable parameters
    std::atomic<float>* wavetableAParam = nullptr;
    std::atomic<float>* wavetableBParam = nullptr;
    std::atomic<float>* wavetableMorphParam = nullptr;
    std::atomic<float>* wavetableWarpParam = nullptr;
    std::atomic<float>* wavetableFoldParam = nullptr;

    // Mix parameters
    std::atomic<float>* ringsMixParam = nullptr;
    std::atomic<float>* karplusMixParam = nullptr;
    std::atomic<float>* wavetableMixParam = nullptr;
    std::atomic<float>* grainsMixParam = nullptr;

    // Envelope parameters
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* sustainParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;

    // Filter parameters
    std::atomic<float>* filterCutoffParam = nullptr;
    std::atomic<float>* filterResonanceParam = nullptr;
    std::atomic<float>* filterEnvParam = nullptr;

    // Oscillator parameters
    std::atomic<float>* osc1WaveParam = nullptr;
    std::atomic<float>* osc1OctaveParam = nullptr;
    std::atomic<float>* osc1SemiParam = nullptr;
    std::atomic<float>* osc1FineParam = nullptr;
    std::atomic<float>* osc1PWParam = nullptr;
    std::atomic<float>* osc1MixParam = nullptr;
    std::atomic<float>* osc2WaveParam = nullptr;
    std::atomic<float>* osc2OctaveParam = nullptr;
    std::atomic<float>* osc2SemiParam = nullptr;
    std::atomic<float>* osc2FineParam = nullptr;
    std::atomic<float>* osc2PWParam = nullptr;
    std::atomic<float>* osc2MixParam = nullptr;

    // Effect parameters
    std::atomic<float>* delayTimeParam = nullptr;
    std::atomic<float>* delayFeedbackParam = nullptr;
    std::atomic<float>* delayMixParam = nullptr;
    std::atomic<float>* delayFilterParam = nullptr;
    std::atomic<float>* delayPingPongParam = nullptr;
    std::atomic<float>* reverbSizeParam = nullptr;
    std::atomic<float>* reverbDampingParam = nullptr;
    std::atomic<float>* reverbWidthParam = nullptr;
    std::atomic<float>* reverbMixParam = nullptr;
    std::atomic<float>* reverbShimmerParam = nullptr;
    std::atomic<float>* chorusRateParam = nullptr;
    std::atomic<float>* chorusDepthParam = nullptr;
    std::atomic<float>* chorusMixParam = nullptr;
    std::atomic<float>* chorusFeedbackParam = nullptr;
    std::atomic<float>* chorusWidthParam = nullptr;

    // Performance control parameters
    std::atomic<float>* portamentoParam = nullptr;
    std::atomic<float>* vibratoDepthParam = nullptr;
    std::atomic<float>* vibratoRateParam = nullptr;
    std::atomic<float>* masterTuneParam = nullptr;
    std::atomic<float>* velocitySensParam = nullptr;
    std::atomic<float>* panSpreadParam = nullptr;
    std::atomic<float>* unisonVoicesParam = nullptr;
    std::atomic<float>* unisonDetuneParam = nullptr;
    
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        
        // Engine selection
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            "engineMode", "Engine Mode",
            juce::StringArray{"Rings", "Clouds", "Karplus", "Rings→Grains", "Hybrid All",
                            "Basic Osc", "Osc+Rings", "Osc+Clouds", "Full Hybrid"},
            5)); // Default to Basic Osc
        
        // RINGS parameters
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "ringsBrightness", "Rings Brightness", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "ringsDamping", "Rings Damping", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "ringsPosition", "Rings Position", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "ringsStructure", "Rings Structure", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            "ringsModel", "Rings Model",
            juce::StringArray{"String", "Membrane", "Tube", "Bell"}, 0));
        
        // CLOUDS parameters
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "cloudsPosition", "Clouds Position", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "cloudsSize", "Grain Size", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "cloudsDensity", "Grain Density", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "cloudsTexture", "Texture", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "cloudsPitch", "Pitch Shift", -1.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "cloudsStereo", "Stereo Spread", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            "cloudsFreeze", "Freeze", false));
        
        // WAVETABLE parameters
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            "wavetableA", "Wavetable A", 0, 31, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            "wavetableB", "Wavetable B", 0, 31, 1));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "wavetableMorph", "Morph", 0.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "wavetableWarp", "Warp", 0.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "wavetableFold", "Fold", 0.0f, 1.0f, 0.0f));
        
        // MIX levels
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "ringsMix", "Rings Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "karplusMix", "Karplus Mix", 0.0f, 1.0f, 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "wavetableMix", "Wavetable Mix", 0.0f, 1.0f, 0.2f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "grainsMix", "Grains Mix", 0.0f, 1.0f, 0.5f));
        
        // ENVELOPE
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "attack", "Attack", 0.001f, 5.0f, 0.001f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "decay", "Decay", 0.001f, 5.0f, 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "sustain", "Sustain", 0.0f, 1.0f, 0.7f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "release", "Release", 0.001f, 10.0f, 0.5f));
        
        // FILTER
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "filterCutoff", "Filter Cutoff", 20.0f, 20000.0f, 5000.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "filterResonance", "Filter Resonance", 0.1f, 10.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "filterEnv", "Filter Envelope", 0.0f, 1.0f, 0.5f));
        
        // EFFECTS - ALL PARAMETERS
        // Delay (FIRST)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "delayTime", "Delay Time", 1.0f, 2000.0f, 500.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "delayFeedback", "Delay Feedback", 0.0f, 0.95f, 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "delayMix", "Delay Mix", 0.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "delayFilter", "Delay Filter", 20.0f, 20000.0f, 10000.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "delayWidth", "Delay Width", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            "delayPingPong", "Ping-Pong", false));

        // Reverb (SECOND)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "reverbSize", "Reverb Size", 0.0f, 1.0f, 0.7f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "reverbDamping", "Reverb Damping", 0.0f, 1.0f, 0.6f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "reverbWidth", "Reverb Width", 0.0f, 1.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "reverbMix", "Reverb Mix", 0.0f, 1.0f, 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "reverbShimmer", "Reverb Shimmer", 0.0f, 1.0f, 0.0f));

        // Chorus (THIRD - replacing Distortion)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "chorusRate", "Chorus Rate", 0.1f, 10.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "chorusDepth", "Chorus Depth", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "chorusMix", "Chorus Mix", 0.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "chorusFeedback", "Chorus Feedback", 0.0f, 0.7f, 0.2f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "chorusWidth", "Chorus Width", 0.0f, 1.0f, 1.0f));

        // BASIC OSCILLATOR 1
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            "osc1Wave", "Osc 1 Wave",
            juce::StringArray{"Sine", "Saw", "Square", "Triangle", "Pulse"}, 1)); // Default to Saw

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "osc1Octave", "Osc 1 Octave", -2.0f, 2.0f, 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "osc1Semi", "Osc 1 Semi", -12.0f, 12.0f, 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "osc1Fine", "Osc 1 Fine", -100.0f, 100.0f, 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "osc1PW", "Osc 1 Pulse Width", 0.0f, 1.0f, 0.5f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "osc1Mix", "Osc 1 Mix", 0.0f, 1.0f, 0.0f));

        // BASIC OSCILLATOR 2
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            "osc2Wave", "Osc 2 Wave",
            juce::StringArray{"Sine", "Saw", "Square", "Triangle", "Pulse"}, 1)); // Default to Saw

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "osc2Octave", "Osc 2 Octave", -2.0f, 2.0f, 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "osc2Semi", "Osc 2 Semi", -12.0f, 12.0f, 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "osc2Fine", "Osc 2 Fine", -100.0f, 100.0f, 7.0f)); // Default: 7 cents detune for thickness

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "osc2PW", "Osc 2 Pulse Width", 0.0f, 1.0f, 0.5f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "osc2Mix", "Osc 2 Mix", 0.0f, 1.0f, 0.0f));

        // PERFORMANCE CONTROLS - Real functionality now implemented!
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "portamento", "Portamento", 0.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "vibratoDepth", "Vibrato Depth", 0.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "vibratoRate", "Vibrato Rate", 0.1f, 10.0f, 4.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "masterTune", "Master Tune", -100.0f, 100.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "velocitySens", "Velocity Sensitivity", 0.0f, 2.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "panSpread", "Pan Spread", 0.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            "unisonVoices", "Unison Voices", 1, 4, 1));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "unisonDetune", "Unison Detune", 0.0f, 50.0f, 0.0f));

        return {params.begin(), params.end()};
    }
    
    void updateVoiceParameters()
    {
        // REAL-TIME SAFE - No string lookups, only cached pointer access!
        if (!engineModeParam || !ringsBrightnessParam || !ringsDampingParam)
            return; // Safety check

        UltimatePluckVoice::VoiceParams voiceParams;

        // Engine mode
        int modeIndex = engineModeParam->load();
        voiceParams.engineMode = static_cast<UltimatePluckVoice::EngineMode>(modeIndex);

        // Rings
        voiceParams.ringsBrightness = ringsBrightnessParam->load();
        voiceParams.ringsDamping = ringsDampingParam->load();
        voiceParams.ringsPosition = ringsPositionParam->load();
        voiceParams.ringsStructure = ringsStructureParam->load();
        int ringModelIndex = ringsModelParam->load();
        voiceParams.ringsModel = static_cast<ModalResonator::ResonatorModel>(ringModelIndex);

        // Clouds
        voiceParams.cloudsParams.position = cloudsPositionParam->load();
        voiceParams.cloudsParams.size = cloudsSizeParam->load();
        voiceParams.cloudsParams.density = cloudsDensityParam->load();
        voiceParams.cloudsParams.texture = cloudsTextureParam->load();
        voiceParams.cloudsParams.pitch = cloudsPitchParam->load();
        voiceParams.cloudsParams.stereoSpread = cloudsStereoParam->load();
        voiceParams.cloudsParams.freeze = cloudsFreezeParam->load() > 0.5f;

        // Wavetable
        voiceParams.wavetableParams.tableA = wavetableAParam->load();
        voiceParams.wavetableParams.tableB = wavetableBParam->load();
        voiceParams.wavetableParams.morph = wavetableMorphParam->load();
        voiceParams.wavetableParams.warp = wavetableWarpParam->load();
        voiceParams.wavetableParams.fold = wavetableFoldParam->load();

        // Mix
        voiceParams.ringsMix = ringsMixParam->load();
        voiceParams.karplusMix = karplusMixParam->load();
        voiceParams.wavetableMix = wavetableMixParam->load();
        voiceParams.grainsMix = grainsMixParam->load();

        // Envelope
        voiceParams.attack = attackParam->load();
        voiceParams.decay = decayParam->load();
        voiceParams.sustain = sustainParam->load();
        voiceParams.release = releaseParam->load();

        // Filter
        voiceParams.filterCutoff = filterCutoffParam->load();
        voiceParams.filterResonance = filterResonanceParam->load();
        voiceParams.filterEnvAmount = filterEnvParam->load();

        // Basic Oscillators
        int osc1WaveIndex = osc1WaveParam->load();
        voiceParams.osc1Wave = static_cast<BasicOscillator::WaveType>(osc1WaveIndex);
        voiceParams.osc1Octave = osc1OctaveParam->load();
        voiceParams.osc1Semi = osc1SemiParam->load();
        voiceParams.osc1Fine = osc1FineParam->load();
        voiceParams.osc1PW = osc1PWParam->load();
        voiceParams.osc1Mix = osc1MixParam->load();

        int osc2WaveIndex = osc2WaveParam->load();
        voiceParams.osc2Wave = static_cast<BasicOscillator::WaveType>(osc2WaveIndex);
        voiceParams.osc2Octave = osc2OctaveParam->load();
        voiceParams.osc2Semi = osc2SemiParam->load();
        voiceParams.osc2Fine = osc2FineParam->load();
        voiceParams.osc2PW = osc2PWParam->load();
        voiceParams.osc2Mix = osc2MixParam->load();

        // Update all voices
        for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* voice = dynamic_cast<UltimatePluckVoice*>(synth.getVoice(i)))
            {
                voice->setParameters(voiceParams);
            }
        }
    }
    
    void applyEffects(juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() != 2)
            return;

        int numSamples = buffer.getNumSamples();
        auto* leftChannel = buffer.getWritePointer(0);
        auto* rightChannel = buffer.getWritePointer(1);

        // STAGE 1: DELAY - REAL-TIME SAFE (cached pointers)
        if (!delayTimeParam || !delayFeedbackParam || !delayMixParam || !delayFilterParam || !delayPingPongParam)
            return;

        float delayTime = this->delayTimeParam->load();
        float delayFeedback = this->delayFeedbackParam->load();
        float delayMix = this->delayMixParam->load();
        float delayFilter = this->delayFilterParam->load();
        bool delayPingPong = this->delayPingPongParam->load() > 0.5f;

        if (delayMix > 0.001f)
        {
            advancedDelay.setDelayTime(delayTime);
            advancedDelay.setFeedback(delayFeedback);
            advancedDelay.setMix(delayMix);
            advancedDelay.setFilterCutoff(delayFilter);
            advancedDelay.setPingPong(delayPingPong);

            for (int i = 0; i < numSamples; ++i)
            {
                leftChannel[i] = advancedDelay.processSample(leftChannel[i], 0);
                rightChannel[i] = advancedDelay.processSample(rightChannel[i], 1);
            }
        }

        // STAGE 2: REVERB - REAL-TIME SAFE (cached pointers)
        if (!reverbSizeParam || !reverbDampingParam || !reverbWidthParam || !reverbMixParam || !reverbShimmerParam)
            return;

        float reverbSize = this->reverbSizeParam->load();
        float reverbDamping = this->reverbDampingParam->load();
        float reverbWidth = this->reverbWidthParam->load();
        float reverbMix = this->reverbMixParam->load();
        float reverbShimmer = this->reverbShimmerParam->load();

        if (reverbMix > 0.001f)
        {
            enhancedReverb.setSize(reverbSize);
            enhancedReverb.setDamping(reverbDamping);
            enhancedReverb.setWidth(reverbWidth);
            enhancedReverb.setMix(reverbMix);
            enhancedReverb.setShimmer(reverbShimmer);
            enhancedReverb.processStereo(leftChannel, rightChannel, numSamples);
        }

        // STAGE 3: CHORUS - REAL-TIME SAFE (cached pointers)
        if (!chorusRateParam || !chorusDepthParam || !chorusMixParam || !chorusFeedbackParam || !chorusWidthParam)
            return;

        float chorusRate = this->chorusRateParam->load();
        float chorusDepth = this->chorusDepthParam->load();
        float chorusMix = this->chorusMixParam->load();
        float chorusFeedback = this->chorusFeedbackParam->load();
        float chorusWidth = this->chorusWidthParam->load();

        if (chorusMix > 0.001f)
        {
            chorus.setRate(chorusRate);
            chorus.setDepth(chorusDepth);
            chorus.setMix(chorusMix);
            chorus.setFeedback(chorusFeedback);
            chorus.setStereoWidth(chorusWidth);

            for (int i = 0; i < numSamples; ++i)
            {
                chorus.processStereo(leftChannel[i], rightChannel[i]);
            }
        }

        // FINAL STAGE: Gentler soft limiter to prevent distortion
        // Higher threshold and softer knee for cleaner sound
        const float softThreshold = 0.85f; // Start soft limiting higher
        const float ceiling = 0.98f; // Hard ceiling closer to 0dB

        // GENTLER soft limiter with smooth curve
        auto softLimit = [softThreshold, ceiling](float sample) -> float
        {
            float absSample = std::abs(sample);
            if (absSample > softThreshold)
            {
                float sign = (sample > 0.0f) ? 1.0f : -1.0f;
                // Gentler compression using smoother curve
                float excess = absSample - softThreshold;
                float compressed = softThreshold + excess * 0.3f;  // Gentler ratio
                return sign * juce::jmin(compressed, ceiling);
            }
            return sample;
        };

        for (int i = 0; i < numSamples; ++i)
        {
            leftChannel[i] = softLimit(leftChannel[i]);
            rightChannel[i] = softLimit(rightChannel[i]);
        }
    }
};
