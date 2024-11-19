#ifndef COLIN_WAVEVERB_H
#define COLIN_WAVEVERB_H

#include <math.h>
#include "../Reverb/Delay.h"
#include "../Utility/Biquad.h"
#include "../Waveguide/String.h"
#include "juce_audio_basics/juce_audio_basics.h"

/*
  ==============================================================================

    WaveVerb.h
    Created: 3 Nov 2024 5:26:13pm
    Author:  Colin Raab

  ==============================================================================
*/

namespace Colin
{
    enum ModelType {
        None = 0,
        String,
        Closed_Tube,
        Open_Tube
    };

    enum RootNote {
        C = 0,
        C_Sharp,
        D,
        D_Sharp,
        E,
        F,
        F_Sharp,
        G,
        G_Sharp,
        A,
        A_Sharp,
        B
    };

    enum ChordType {
        Midi_Input = 0,
        Single_Note,
        Major,
        Minor,
        Dominant,
        Major_Seven,
        Minor_Seven
    };

class WaveVerb {
public:
    WaveVerb() = default;
    ~WaveVerb() = default;

    void prepareToPlay(const double fs) {
        sampleRate = fs;
        diffusion.prepareToPlay(150.f, fs);
        feedback.prepareToPlay(fs, 150.f, 0.85f);
        strings.prepareToPlay(fs);
        updateFrequencies();
        setSize(roomSizeMS, rt60MS);
        matrix.cheapEnergyCrossfade(dryWet, outCoeff, inCoeff);
    }

    void processBuffer(juce::AudioBuffer<float>& buffer) {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();

        for(int sample = 0; sample < numSamples; sample++) {
            float sampleL = 0.f;
            float sampleR = 0.f;
            if(numChannels == 1) {
                // mono configuration
                sampleL = buffer.getSample(0, sample);
                sampleR = -1.f * sampleL;
            }
            else {
                // stereo configuration
                sampleL = buffer.getSample(0, sample);
                sampleR = buffer.getSample(1, sample);
            }

            data input = matrix.stereoToMulti(sampleL, sampleR);
            data mout = input;
            if(modelType == ModelType::String) {
                mout = strings.process(input);
            }
            //mout.scale(blendOutCoeff);
            //input = matrix.combine(input, mout);
            input = matrix.intermix(input, mout, blendInCoeff, blendOutCoeff);
            data dout = diffusion.process(input);
            data fout = feedback.process(dout);

            //data mixed = matrix.intermix(fout, mout, blendInCoeff, blendOutCoeff);
            float outSampleL = 0;
            float outSampleR = 0;
            matrix.multiToStereo(fout, outSampleL, outSampleR);

            outSampleL *= outputGain;
            outSampleR *= outputGain;
            float left = (outSampleL * outCoeff) + (sampleL * inCoeff);
            float right = (outSampleR * outCoeff) + (sampleR * inCoeff);
            jassert(std::abs(left) <= 1 || std::abs(right) <= 1);
            buffer.setSample(0, sample, left);
            buffer.setSample(1, sample, right);
        }
    }

    void processMidi(juce::MidiBuffer& midiMessages) {
        for(const auto midiMessage : midiMessages) {
            auto midiEvent = midiMessage.getMessage();
            if(midiEvent.isNoteOn()) {
                midiNotes.push_back(midiEvent.getNoteNumber());
            }
            else if(midiEvent.isNoteOff()) {
                for(std::vector<int>::iterator it = midiNotes.begin(); it != midiNotes.end(); it++) {
                    if(*it == midiEvent.getNoteNumber()) {
                        midiNotes.erase(it);
                    }
                }
            }
        }
    }

    void setSize(float newSize, float rt) {
        rt = rt * 1000.f; // convert from seconds to milliseconds
        if(!juce::approximatelyEqual(newSize, roomSizeMS)) {
            roomSizeMS = newSize;
            feedback.setTime(roomSizeMS);
            diffusion.prepareToPlay(roomSizeMS, sampleRate);
        }
        if(!juce::approximatelyEqual(rt, rt60MS)) {
            rt60MS = rt;
            float loop = roomSizeMS * 1.5f;
            float loopPerRT60 = rt60MS / (loop);
            float dbPerLoop = -60.f/loopPerRT60;
            reverbDecay = powf(10.f,dbPerLoop * 0.05f);
            feedback.setDecay(reverbDecay);
        }
    }

    void setDryWet(float dw) {
        dw = dw / 100.f; // 0-100 to 0-1
        if (juce::approximatelyEqual(dw, dryWet)) return;
        dryWet = dw;
        // equal power coefficients for dry/wet mixing
        matrix.cheapEnergyCrossfade(dryWet, outCoeff, inCoeff);
    }

    void setOutputGain(float gain) {
        outputGain = juce::Decibels::decibelsToGain(gain);
    }

    void setModel(int type) {
        modelType = type;
    }

    void setRoot(int root) {
        root += 48;
        if(rootNote == root) return;
        rootNote = root;
        updateFrequencies();
    }

    void setChord(int chord) {
        if(chordType == chord) return;
        chordType = chord;
        if(chordType == ChordType::Midi_Input) {
            chordDegrees.clear();
        }
        else if(chord == ChordType::Single_Note) {
            chordDegrees.resize(4);
            chordDegrees = {0, 0, 0, 0};
        }
        else if(chord == ChordType::Major) {
            chordDegrees.resize(4);
            chordDegrees = {0, 4, 7, 12};
        }
        else if(chord == ChordType::Minor) {
            chordDegrees.resize(4);
            chordDegrees = {0, 3, 7, 12};
        }
        else if(chord == ChordType::Dominant) {
            chordDegrees.resize(4);
            chordDegrees = {0, 4, 7, 11};
        }
        else if(chord == ChordType::Major_Seven) {
            chordDegrees.resize(4);
            chordDegrees = {0, 4, 7, 10};
        }
        else if(chord == ChordType::Minor_Seven) {
            chordDegrees.resize(4);
            chordDegrees = {0, 3, 7, 10};
        }
        updateFrequencies();
    }

    void setBlend(float b) {
        b = b / 100.f;
        blend = b;
        matrix.cheapEnergyCrossfade(blend, blendOutCoeff, blendInCoeff);
    }

    void setWaveguideDecay(float d) {
        if(juce::approximatelyEqual(d, waveguideDecay)) return;
        waveguideDecay = d;
        strings.setDecay(waveguideDecay);
    }

    void setWaveguideRate(float r) {
        if(juce::approximatelyEqual(r, waveguideRate)) return;
        waveguideRate = r;
        strings.setRate(r);
    }

    void setWaveguidePickup(float p) {
        strings.setPickupPosition(p);
    }

    void setWaveguideTrigger(float t) {
        strings.setTriggerPosition(t);
    }

    void reset() {

    }

private:
    double sampleRate = 44100;
    float dryWet = 1.f; /// wet = 1, dry = 0
    float outputGain = 1.f;
    float blend = 0.5f; /// reverb = 0, strings = 1
    float inCoeff = 0.f;
    float outCoeff = 0.f;
    float blendInCoeff = 0.f;
    float blendOutCoeff = 0.f;

    // reverb parameters
    float roomSizeMS = 150.f; /// in ms
    float rt60MS = 3000.f; /// in ms
    float reverbDecay = 0.f;

    // waveguide parameters
    int rootNote = 48;
    int chordType = 0;
    int modelType = 0;
    std::vector<int> chordDegrees = {0, 0, 0, 0};
    std::vector<int> midiNotes;
    float waveguideDecay = 0.f;
    float waveguideRate = 0.f;

    Diffuser diffusion;
    Multi_Delay feedback;
    Mix_Matrix matrix;
    Multi_String strings;

    void updateFrequencies() {
        if(!strings.isInitialised()) return;
        std::vector<float> frequencies = getFrequencies();
        if(frequencies.empty()) {
            strings.resetAfterDecay();
        }
        else strings.setFrequencies(frequencies);
    }

    std::vector<float> getFrequencies() {
        std::vector<float> frequencies;
        if(!midiNotes.empty()) {
            for(const int midiNote : midiNotes) {
                float freq = midiToFreq(midiNote);
                frequencies.push_back(freq);
            }
        }
        else {
            for(const int chordDegree : chordDegrees) {
                float freq = midiToFreq(rootNote + chordDegree);
                frequencies.push_back(freq);
            }
        }
        return frequencies;
    }

    static float midiToFreq(const int midi) {
        return std::powf(2, (static_cast<float>(midi) - 69.f) / 12.f) * 440.f;
    }
};

}

#endif
