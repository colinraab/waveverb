#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================

namespace IDs {
    static juce::String dryWet {"dryWet"};
    static juce::String blendReverbWaveguide {"blend"};
    static juce::String wetGain {"wetGain"};
    static juce::String roomSize {"roomSize"};
    static juce::String rt60 {"rt60"};
    static juce::String lowPass {"lowPass"};
    static juce::String modelType {"modelType"};
    static juce::String rootNote {"rootNote"};
    static juce::String chordType {"chordType"};

}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    auto freqRange = juce::NormalisableRange<float> {20.0f, 20000.0f,
        [](float start, float end, float normalised)
        {
            return start + (std::pow (2.0f, normalised * 10.0f) - 1.0f) * (end - start) / 1023.0f;
        },
        [](float start, float end, float value)
        {
            return (std::log (((value - start) * 1023.0f / (end - start)) + 1.0f) / std::log ( 2.0f)) / 10.0f;
        },
        [](float start, float end, float value)
        {
            if (value > 3000.0f)
                return juce::jlimit (start, end, 100.0f * juce::roundToInt (value / 100.0f));

            if (value > 1000.0f)
                return juce::jlimit (start, end, 10.0f * juce::roundToInt (value / 10.0f));

            return juce::jlimit (start, end, float (juce::roundToInt (value)));
        }};

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto global = std::make_unique<juce::AudioProcessorParameterGroup>("Global", TRANS ("Global"), "|");
    global->addChild (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID (IDs::dryWet, 1), "Dry Wet", juce::NormalisableRange<float>(0.0f, 100.0f, 1.f), 100.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID (IDs::blendReverbWaveguide, 1), "Blend", juce::NormalisableRange<float>(0.0f, 100.0f, 1.f), 0.f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID (IDs::wetGain, 1), "Wet Gain", juce::NormalisableRange<float>(-12.0f, 24.0f, 1.0f), 0.0f));

    auto reverb = std::make_unique<juce::AudioProcessorParameterGroup>("Reverb", TRANS ("Reverb"), "|");
    reverb->addChild (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID (IDs::lowPass, 1), "Low Pass", freqRange, 440.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID (IDs::roomSize, 1), "Room Size", juce::NormalisableRange<float>(50.0f, 300.0f, 5.0f), 150.0f),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID (IDs::rt60, 1), "RT 60", juce::NormalisableRange<float>(0.2f, 10.0f, 0.1f), 2.0f));

    auto waveguide = std::make_unique<juce::AudioProcessorParameterGroup>("Waveguide", TRANS ("Waveguide"), "|");
        waveguide->addChild (std::make_unique<juce::AudioParameterChoice>(juce::ParameterID (IDs::modelType, 1), "Model Type", juce::StringArray("None", "String", "Closed Tube", "Open Tube"), 0),
        std::make_unique<juce::AudioParameterChoice>(juce::ParameterID (IDs::rootNote, 1), "Root Note", juce::StringArray("C", "C#", "D", "D#", "E", "F", "G", "G#", "A", "A#", "B"), 0),
        std::make_unique<juce::AudioParameterChoice>(juce::ParameterID (IDs::chordType, 1), "Chord Type", juce::StringArray("Single Note", "Major", "Minor", "Dominant", "Major 7", "Minor 7"), 0));
    layout.add (std::move (global), std::move (reverb), std::move (waveguide));

    return layout;
}


//==============================================================================


PluginProcessor::PluginProcessor()
     : foleys::MagicProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
                       treeState (*this, nullptr, "Parameters", createParameterLayout())
{
    FOLEYS_SET_SOURCE_PATH(__FILE__);


    // global params
    dryWet = treeState.getRawParameterValue (IDs::dryWet);
    jassert(dryWet != nullptr);
    blendReverbWaveguide = treeState.getRawParameterValue(IDs::blendReverbWaveguide);
    jassert(blendReverbWaveguide != nullptr);
    wetGain = treeState.getRawParameterValue(IDs::wetGain);
    jassert(wetGain != nullptr);

    // reverb params
    roomSize = treeState.getRawParameterValue (IDs::roomSize);
    jassert(roomSize != nullptr);
    rt60 = treeState.getRawParameterValue (IDs::rt60);
    jassert(rt60 != nullptr);
    lowPass = treeState.getRawParameterValue (IDs::lowPass);
    jassert(lowPass != nullptr);

    // waveguide params
    treeState.addParameterListener(IDs::modelType, this);
    treeState.addParameterListener(IDs::rootNote, this);
    treeState.addParameterListener(IDs::chordType, this);

    magicState.setGuiValueTree(BinaryData::magic_xml, BinaryData::magic_xmlSize);

    // can delete upon release
    formatManager.registerBasicFormats();
    magicState.addTrigger("startStop", [&] {startStopLoop();});
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================

const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================

void PluginProcessor::parameterChanged (const juce::String& param, float value) {
    if(param == IDs::modelType) {
        waveVerb.setModel(juce::roundToInt(value));
    }
    else if(param == IDs::rootNote) {
        waveVerb.setRoot(juce::roundToInt(value));
    }
    else if(param == IDs::chordType) {
        waveVerb.setChord(juce::roundToInt(value));
    }
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate, samplesPerBlock);

    magicState.prepareToPlay (sampleRate, samplesPerBlock);
    waveVerb.prepareToPlay(sampleRate);

    // can delete upon release
    transport.prepareToPlay(samplesPerBlock, sampleRate);
    juce::File loop (juce::File::getSpecialLocation(juce::File::SpecialLocationType::userMusicDirectory).getChildFile("test_guitar_loop.wav"));
    juce::AudioFormatReader* reader = formatManager.createReaderFor(loop);
    std::unique_ptr<juce::AudioFormatReaderSource> tempSource (new juce::AudioFormatReaderSource (reader, true));
    tempSource->setLooping(true);
    transport.setSource(tempSource.get());
    playSource.reset(tempSource.release());
    transport.setPosition (0.0);
    isPlaying = false;
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // can delete upon release
    juce::AudioSourceChannelInfo sourceInfo(buffer);
    transport.getNextAudioBlock(sourceInfo);

    waveVerb.setSize(*roomSize, *rt60);
    waveVerb.setDryWet(*dryWet);
    waveVerb.setOutputGain(*wetGain);
    waveVerb.setBlend(*blendReverbWaveguide );

    if(*dryWet != 0.f) {
        waveVerb.processBuffer(buffer);
    }
}



// can be deleted upon release
void PluginProcessor::startStopLoop() {
    auto now = juce::Time::currentTimeMillis();
    auto elapsed = now - lastCallTime;

    if (elapsed >= 1000) {
        lastCallTime = now;
        if(isPlaying) {
            transport.setPosition(0.0);
            transport.stop();
            isPlaying = false;
        }
        else {
            transport.start();
            isPlaying = true;
        }
    }
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
