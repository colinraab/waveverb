#ifndef COLIN_STRING_H
#define COLIN_STRING_H

#include <memory>

#include "../Reverb/Delay.h"
#include "../Utility/Biquad.h"
#include "juce_dsp/juce_dsp.h"

/*
  ==============================================================================

    String.h
    Created: 4 Nov 2024 1:14:27pm
    Author:  Colin Raab

  ==============================================================================
*/

namespace Colin {

class Waveguide_String {
public:
    Waveguide_String() = default;
    ~Waveguide_String() = default;

    void prepareToPlay(const double fs) {
        sampleRate = fs;
        juce::dsp::ProcessSpec spec(fs, 512, 2);
        LPF.prepare(spec);
        APFforward.prepare(spec);
        APFbackward.prepare(spec);
        bodyFilters.resize(16);
        for(size_t i=0; i<16; i++) {
            bodyFilters[i].setCoefficients(bodyB[0], bodyB[1], bodyB[2], bodyA[i][1], bodyA[i][2]);
        }
    }

    void setFrequency(const float freq) {
        if(juce::approximatelyEqual(freq, frequency)) return;
        frequency = freq;
        if(juce::approximatelyEqual(frequency, 0.f)) {
            enabled = false;
            return;
        }
        enabled = true;
        updateParameters();
    }

    void setPickupPosition(const float position) {
        if(juce::approximatelyEqual(position, pickupPosition)) return;
        jassert(pickupPosition >= 0.f && pickupPosition <= 1.f);
        pickupPosition = position;
        updateParameters();
    }

    void setTriggerPosition(const float position) {
        if(juce::approximatelyEqual(position, triggerPosition)) return;
        jassert(triggerPosition >= 0.f && triggerPosition <= 1.f);
        triggerPosition = position;
        updateParameters();
    }

    void setDecayTime(const float time) {
        if(juce::approximatelyEqual(time, decayTime)) return;
        jassert(decayTime >= 0.f && decayTime <= 1.f);
        decayTime = time;
        updateParameters();
    }

    void resetAfterDecay() {
        shouldReset = true;
    }

    void reset() {
        forwardLine.reset();
        backwardLine.reset();
        shouldReset = false;
    }

    void addSample(const float sample) {
        forwardLine.add(sample);
        backwardLine.add(sample);
    }

    float getSample() {
        if(!enabled) return 0.f;

        const float forwardOut = APFforward.processSample(forwardLine.getBack());
        const float backwardOut = APFbackward.processSample(backwardLine.getBack());

        if(shouldReset) {
            if(std::abs(forwardOut) < 0.000001f && std::abs(backwardOut) < 0.000001f) {
                reset();
                enabled = false;
                return 0.f;
            }
        }

        forwardLine.add(-1.f * backwardOut);
        backwardLine.add(static_cast<float>(-1.0 * decayCoeff * LPF.processSample(forwardOut)));

        const float waveguideOut = forwardLine.getOffset(forwardPickupIndex) + backwardLine.getOffset(backwardPickupIndex);
        const float bodyOut = applyBodyFilters(waveguideOut);
        return bodyOut;
    }

    float applyBodyFilters(const float sample) {
        float output = 0.f;
        for(size_t i=0; i<16; i++) {
            output += bodyFilters[i].processAudioSample(sample);
        }
        output /= 40.f; // scaling to prevent values exceeding -1 to 1
        return output;
    }

    float getLength() {
        return length;
    }

    void trigger(const float velocity) {
        for (int i=0; i<=forwardPickupIndex; i++) {
            const float val = juce::jmap(static_cast<float>(i), 0.f, static_cast<float>(forwardTriggerIndex), 0.f, velocity / 2.f);
            forwardLine.setAt(i, val);
            backwardLine.setAt(juce::roundToInt(length) - 1 - i, val);
        }
        for (int i=forwardTriggerIndex; i<=juce::roundToInt(length); i++) {
            const float val = juce::jmap(static_cast<float>(i), static_cast<float>(forwardTriggerIndex), length - 1, velocity / 2.f, 0.f);
            forwardLine.setAt(i, val);
            backwardLine.setAt(juce::roundToInt(length) - 1 - i, val);
        }
    }

private:
    Circular_Buffer forwardLine;
    Circular_Buffer backwardLine;
    juce::dsp::IIR::Filter<float> LPF;
    juce::dsp::IIR::Filter<float> APFforward;
    juce::dsp::IIR::Filter<float> APFbackward;
    double sampleRate = 44100;
    int forwardPickupIndex;
    int backwardPickupIndex;
    int forwardTriggerIndex;
    float fractional = 0.f;
    float frequency;
    float pickupPosition = 0.8f;
    float triggerPosition = 0.2f;
    float decayTime = 0.9f;
    double decayCoeff;
    float length;
    bool shouldReset = false;
    bool enabled;

    // Violin Body Modal Filters
    const double bodyA[16][3] = {{1,	-1.99447342188475,	0.995775119117750},
        {1,	-1.99379494705379,	0.995907928516203},
        {1,	-1.99201110254198,	0.995637097398487},
        {1,	-1.98804163175910,	0.993001329351293},
        {1,	-1.46308934126173,	0.878718397467588},
        {1,	-0.979975405041940,	0.448677575918752},
        {1,	-1.77581910819218,	0.963024144711050},
        {1,	-1.58662082867726,	0.832304710805051},
        {1,	-1.86069429491259,	0.970471532013134},
        {1,	-1.90165908404357,	0.976976367371324},
        {1,	-1.91280411519008,	0.969939023072752},
        {1,	-1.95275127750774,	0.995617571258647},
        {1,	-1.97139026017112,	0.980949861130503},
        {1,	-1.97971903689756,	0.994452995604781},
        {1,	-1.97550615542992,	0.994395384672155},
        {1,	-1.96686686949208,	0.989964816263091}};
    const double bodyB[3] = {1, 0, -1};
    std::vector<Biquad_Filter> bodyFilters;

    void updateParameters() {
        length = static_cast<float>(sampleRate) / frequency;
        const int roundLength = juce::roundToInt(length);
        fractional = (length - static_cast<float>(roundLength)) / length;
        forwardLine.resize(roundLength);
        backwardLine.resize(roundLength);
        forwardPickupIndex = juce::roundToInt(juce::jmap(pickupPosition, 0.f, static_cast<float>(roundLength) / 2.f - 1));
        backwardPickupIndex = roundLength - 1 - forwardPickupIndex;
        forwardTriggerIndex = juce::roundToInt(juce::jmap(triggerPosition, 0.f, static_cast<float>(roundLength) / 2.f - 1));
        LPF.reset();
        LPF.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(sampleRate, 4 * frequency);
        APFforward.reset();
        APFforward.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderAllPass(sampleRate, frequency);
        APFbackward.reset();
        APFbackward.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderAllPass(sampleRate, frequency);
        decayCoeff = juce::jmap(static_cast<double>(decayTime), std::pow(0.9999, static_cast<double>(roundLength)), std::pow(0.999999, static_cast<double>(roundLength)));
        if(decayCoeff >= 0.9999) decayCoeff = 0.9999;
        reset();
    }
};

class Multi_String {
public:
    Multi_String() = default;
    ~Multi_String() {
        strings.clear();
    }

    void prepareToPlay(double fs) {
        strings.clear();
        lengths.clear();
        counters.clear();
        for(size_t i = 0; i < NUM_CHANNELS; i++) {
            auto s = std::make_unique<Waveguide_String>();
            strings.push_back(std::move(s));
            strings[i]->prepareToPlay(fs);
            strings[i]->setFrequency(261.63f); // middle C
            lengths.push_back(juce::roundToInt(strings[i]->getLength()));
            counters.push_back(0);
        }
    }

    data process(data input) {
        data output;
        for(size_t i = 0; i < NUM_CHANNELS; i++) {
            auto sample = strings[i]->getSample();
            jassert(std::abs(sample) < 1.f);
            output.channels[i] = sample; // / NUM_CHANNELS
            if(counters[i] >= juce::roundToInt(triggerRate) * lengths[i]) {
                if(input.channels[i] >= 0.01f) {
                    float velocity = juce::jmap(input.channels[i], 0.f, 1.f, 0.f, 0.7f);
                    strings[i]->trigger(velocity);
                    counters[i] = 0;
                }
            }
            counters[i]++;
        }
        return output;
    }

    void setFrequencies(std::vector<float> freqs) {
        lengths.clear();
        for(size_t i = 0; i < NUM_CHANNELS; i++) {
            strings[i]->setFrequency(freqs[i % freqs.size()]);
            lengths.push_back(juce::roundToInt(strings[i]->getLength() + i * 5));
            //lengths.push_back(juce::roundToInt(strings[i]->getLength()));
        }
    }

    void resetAfterDecay() {
        for(size_t i = 0; i < NUM_CHANNELS; i++) {
            strings[i]->resetAfterDecay();
        }
    }

    void setDecay(const float d) const {
        //d = juce::jmap(d, 0.f, 1.f, 0.7f, 2.5f);
        for(size_t i = 0; i < NUM_CHANNELS; i++) {
            strings[i]->setDecayTime(d);
        }
    }

    void setRate(float r) {
        r = juce::jmap(r, 0.f, 1.f, 500.f, 50.f);
        triggerRate = r;
    }

    void setTriggerPosition(const float position) const {
        for(size_t i = 0; i < NUM_CHANNELS; i++) {
            strings[i]->setTriggerPosition(position);
        }
    }

    void setPickupPosition(const float position) const {
        for(size_t i = 0; i < NUM_CHANNELS; i++) {
            strings[i]->setPickupPosition(position);
        }
    }

    bool isInitialised() const {
        if(strings.empty()) return false;
        return true;
    }

private:
    std::vector<std::unique_ptr<Waveguide_String>> strings;
    std::vector<int> lengths;
    std::vector<int> counters;
    float triggerRate;
};

}

#endif
