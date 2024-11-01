#ifndef COLIN_WAVEVERB_H
#define COLIN_WAVEVERB_H

#include <math.h>
#include "../Reverb/Delay.h"
#include "../Utility/Biquad.h"
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

class WaveVerb{
public:
    WaveVerb() = default;
    ~WaveVerb() = default;

    void prepareToPlay(double sampleRate) {
        fs = sampleRate;
        diffusion.prepareToPlay(150, fs);
        feedback.prepareToPlay(fs, 150.f, 0.85f);
        setSize(roomSizeMS, rt60MS);
    }

    void setSize(float newSize, float rt) {
        if(newSize != roomSizeMS) {
            roomSizeMS = newSize;
            feedback.setTime(roomSizeMS);
            diffusion.prepareToPlay(roomSizeMS, fs);
        }
        if(rt != rt60MS) {
            rt60MS = rt;
            float loop = rt60MS * 1.5f;
            float loopPerRT60 = rt60MS / loop;
            float dbPerLoop = -60.f/loopPerRT60;
            decay = powf(10.f,dbPerLoop/20.f);
            feedback.setDecay(decay);
        }
    }

    void processBuffer(juce::AudioBuffer<float>& buffer) {
        int numChannels = buffer.getNumChannels();
        int numSamples = buffer.getNumSamples();

        // equal power coefficients for dry/wet mixing
        float inCoeff = 0;
        float outCoeff = 0;
        matrix.cheapEnergyCrossfade(dryWet, outCoeff, inCoeff);

        for(int sample = 0; sample < numSamples; sample++) {
            float sampleL = 0.f;
            float sampleR = 0.f;
            if(numChannels == 1) {
                // mono configuration
                sampleL = buffer.getSample(0, sample);
                sampleR = -1 * sampleL;
            }
            else {
                // stereo configuration
                sampleL = buffer.getSample(0, sample);
                sampleR = buffer.getSample(1, sample);
            }
            data input = matrix.stereoToMulti(sampleL, sampleR);
            /// perform reverb on the 8 channels
            data dout = diffusion.process(input);
            data fout = feedback.process(dout);
            float outsampleL = 0;
            float outsampleR = 0;
            /// down mix back to stereo
            matrix.multiToStereo(fout, outsampleL, outsampleR);

            float left = (outsampleL * outCoeff) + (sampleL * inCoeff);
            float right = (outsampleR * outCoeff) + (sampleR * inCoeff);
            buffer.setSample(0, sample, left);
            buffer.setSample(1, sample, right);
        }
    }

    void setDryWet(float dw) {
        dryWet = dw;
    }

private:
    float roomSizeMS = 150.f; /// in ms
    float rt60MS = 3000.f; /// in ms
    double fs = 44100;
    float dryWet = 1.f; /// wet = 1, dry = 0
    float decay = 0.f;

    Diffuser diffusion;
    Multi_Delay feedback;
    Mix_Matrix matrix;
};

}

#endif
