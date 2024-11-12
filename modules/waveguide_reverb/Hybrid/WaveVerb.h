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
        Single_Note = 0,
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

    void prepareToPlay(double sampleRate) {
        fs = sampleRate;
        diffusion.prepareToPlay(150.f, fs);
        feedback.prepareToPlay(fs, 150.f, 0.85f);
        strings.prepareToPlay(fs);
        setSize(roomSizeMS, rt60MS);
        matrix.cheapEnergyCrossfade(dryWet, outCoeff, inCoeff);
    }

    void processBuffer(juce::AudioBuffer<float>& buffer) {
        int numChannels = buffer.getNumChannels();
        int numSamples = buffer.getNumSamples();

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
            data dout = diffusion.process(input);
            data fout = feedback.process(dout);
            data mout = fout;
            if(modelType == ModelType::String) {
                mout = strings.process(fout);
            }

            data mixed = matrix.intermix(fout, mout, blendInCoeff, blendOutCoeff);
            float outSampleL = 0;
            float outSampleR = 0;
            matrix.multiToStereo(mixed, outSampleL, outSampleR);

            outSampleL *= outputGain;
            outSampleR *= outputGain;
            float left = (outSampleL * outCoeff) + (sampleL * inCoeff);
            float right = (outSampleR * outCoeff) + (sampleR * inCoeff);
            buffer.setSample(0, sample, left);
            buffer.setSample(1, sample, right);
        }
    }

    void setSize(float newSize, float rt) {
        rt = rt * 1000.f; // convert from seconds to milliseconds
        if(newSize != roomSizeMS) {
            roomSizeMS = newSize;
            feedback.setTime(roomSizeMS);
            diffusion.prepareToPlay(roomSizeMS, fs);
        }
        if(rt != rt60MS) {
            rt60MS = rt;
            float loop = roomSizeMS * 1.5f;
            float loopPerRT60 = rt60MS / (loop);
            float dbPerLoop = -60.f/loopPerRT60;
            decay = powf(10.f,dbPerLoop * 0.05f);
            feedback.setDecay(decay);
        }
    }

    void setDryWet(float dw) {
        dw = dw / 100.f; // 0-100 to 0-1
        if (dryWet == dw) return;
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
        root += 60;
        if(rootNote == root) return;
        rootNote = root;
        updateFrequencies();
    }

    void setChord(int chord) {
        if(chordType == chord) return;
        chordType = chord;
        if(chord == ChordType::Single_Note) {
            chordDegrees = {0, 0, 0, 0};
        }
        else if(chord == ChordType::Major) {
            chordDegrees = {0, 4, 7, 12};
        }
        else if(chord == ChordType::Minor) {
            chordDegrees = {0, 3, 7, 12};
        }
        else if(chord == ChordType::Dominant) {
            chordDegrees = {0, 4, 7, 11};
        }
        else if(chord == ChordType::Major_Seven) {
            chordDegrees = {0, 4, 7, 10};
        }
        else if(chord == ChordType::Minor_Seven) {
            chordDegrees = {0, 3, 7, 10};
        }
        updateFrequencies();
    }

    void setBlend(float b) {
        b = b / 100.f;
        blend = b;
        matrix.cheapEnergyCrossfade(blend, blendOutCoeff, blendInCoeff);
    }

    float midiToFreq(int midi) {
        return std::powf(2, (midi - 69) / 12.f) * 440.f;
    }

private:
    float roomSizeMS = 150.f; /// in ms
    float rt60MS = 3000.f; /// in ms
    double fs = 44100;
    float dryWet = 1.f; /// wet = 1, dry = 0
    float outputGain = 1.f;
    float blend = 0.5f; /// reverb = 0, strings = 1
    float decay = 0.f;
    float inCoeff = 0.f;
    float outCoeff = 0.f;
    float blendInCoeff = 0.f;
    float blendOutCoeff = 0.f;
    int rootNote = 60;
    int chordType = 0;
    int modelType = 0;
    std::vector<int> chordDegrees = {0, 0, 0, 0};

    Diffuser diffusion;
    Multi_Delay feedback;
    Mix_Matrix matrix;
    Multi_String strings;

    void updateFrequencies() {
        std::vector<float> frequencies;
        frequencies.resize(chordDegrees.size());
        for(int i=0; i < chordDegrees.size(); i++) {
            float freq = midiToFreq(rootNote + chordDegrees[i]);
            frequencies.push_back(freq);
        }
        strings.setFrequencies(frequencies);
    }
};

}

#endif
