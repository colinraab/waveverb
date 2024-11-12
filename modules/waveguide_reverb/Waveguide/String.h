#ifndef COLIN_STRING_H
#define COLIN_STRING_H

#include <memory>

#include "../Reverb/Delay.h"
#include "juce_dsp/juce_dsp.h"

/*
  ==============================================================================

    String.h
    Created: 4 Nov 2024 1:14:27pm
    Author:  Colin Raab

  ==============================================================================
*/

namespace Colin {

class String {
public:
    String() = default;
    ~String() = default;

    void prepareToPlay(int fs) {
        sampleRate = fs;
        float length = sampleRate / frequency;
        // CHANGE
        forwardLine.prepareToPlay(sampleRate, length, 1.f);
        backwardLine.prepareToPlay(sampleRate, length, 1.f);
        juce::dsp::ProcessSpec spec((double)fs, 512, 2);
        filter.prepare(spec);
    }

    void setFrequency(float freq) {
        frequency = freq;
        length = sampleRate / frequency;
        forwardLine.setLength(length);
        backwardLine.setLength(length);
        offset = juce::roundToInt(length) / 2;
        filter.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(sampleRate, 4 * frequency);
    }

    void setPickupPosition(float position) {
        pickupPosition = position;
    }

    void setDecayTime(float time) {
        if(time == decayTime) return;
        decayTime = time;
        decayCoeff = juce::jmap(decayTime, std::powf(0.999, length), std::powf(0.99999, length));
    }

    void reset() {
        forwardLine.reset();
        backwardLine.reset();
    }

    void addSample(float sample) {
        forwardLine.delay(sample);
        backwardLine.delay(sample);
    }

    float getSample() {
        float forwardOut = forwardLine.getNext();
        float backwardOut = backwardLine.getNext();

        forwardLine.delay(-1 * backwardOut);
        backwardLine.delay(-1 * decayCoeff * filter.processSample(forwardOut));

        return forwardLine.getOffset(offset) + backwardLine.getOffset(offset);
    }

    float getLength() {
        return length;
    }


private:
    Single_Delay forwardLine;
    Single_Delay backwardLine;
    juce::dsp::IIR::Filter<float> filter;
    int sampleRate = 44100;
    float frequency = 440.f;
    float pickupPosition = 0.8f;
    float decayTime = 0.5f;
    float decayCoeff = 1.f;
    float length;
    int offset;
};

class Multi_String {
public:
    Multi_String() = default;
    ~Multi_String() {
        strings.clear();
    }

    void prepareToPlay(int fs) {
        strings.clear();
        lengths.clear();
        counters.clear();
        for(int i = 0; i < NUM_CHANNELS; i++) {
            strings.push_back(new String());
            strings[i]->prepareToPlay(fs);
            strings[i]->setFrequency(261.63); // middle C
            strings[i]->setDecayTime(2.0f);
            lengths.push_back(juce::roundToInt(strings[i]->getLength()));
            counters.push_back(0);
        }
    }

    void setFrequencies(std::vector<float> freqs) {
        lengths.clear();
        for(int i = 0; i < NUM_CHANNELS; i++) {
            strings[i]->setFrequency(freqs[i % freqs.size()]);
            lengths.push_back(juce::roundToInt(strings[i]->getLength()));
        }
    }

    data process(data input) {
        data output;
        for(int i = 0; i < NUM_CHANNELS; i++) {
            output.channels[i] = strings[i]->getSample();
            if(counters[i] >= lengths[i] * 100) {
                counters[i] = 0;
            }
            if(counters[i] < lengths[i]) {
                strings[i]->addSample(input.channels[i] + output.channels[i]);
            }
            counters[i]++;
        }
        return output;
    }

private:
    std::vector<String*> strings;
    std::vector<int> lengths;
    std::vector<int> counters;
};

}

#endif //STRING_H
