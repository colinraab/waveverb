#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "waveguide_reverb/waveguide_reverb.h"
#include <foleys_gui_magic/foleys_gui_magic.h>

#if (MSVC)
#include "ipps.h"
#endif

class PresetListBox;

class PluginProcessor : public foleys::MagicProcessor,
    private juce::AudioProcessorValueTreeState::Listener
{
public:

    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void parameterChanged (const juce::String& param, float value) override;

    void savePresetInternal();
    void loadPresetInternal(int index);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)

    std::atomic<float>* dryWet = nullptr;
    std::atomic<float>* blendReverbWaveguide = nullptr;
    std::atomic<float>* wetGain = nullptr;
    std::atomic<float>* roomSize = nullptr;
    std::atomic<float>* rt60 = nullptr;
    std::atomic<float>* lowPass = nullptr;
    std::atomic<float>* waveguideA = nullptr;
    std::atomic<float>* waveguideB = nullptr;
    std::atomic<float>* waveguideC = nullptr;
    std::atomic<float>* waveguideD = nullptr;

    juce::AudioProcessorValueTreeState treeState {*this, nullptr};
    juce::ValueTree presetNode;
    PresetListBox* presetList   = nullptr;

    Colin::WaveVerb waveVerb;
    void initialize();


    // audio player for testing, can be deleted upon release
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> playSource;
    juce::AudioTransportSource transport;
    void startStopLoop();
    bool isPlaying = false;
    double lastCallTime;
};
