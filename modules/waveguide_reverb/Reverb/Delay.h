#ifndef COLIN_DELAY_H
#define COLIN_DELAY_H
#include <math.h>
#include <vector>
#include <cstdlib>
#include "Mix_Matrix.h"
#include "../Utility/LFO.h"

/*
  ==============================================================================

    Delay.h
    Created: 1 Nov 2024 7:45:31pm
    Author:  Colin Raab
 
    very naive implementation at the moment, can definitely be optimized

  ==============================================================================
*/

namespace Colin
{

class Circular_Buffer {
protected:
    int size;
    int position = 0;
    std::vector<float> buffer;
    
public:
    Circular_Buffer() = default;
    Circular_Buffer(int size) {
        this->size = size;
        buffer.resize(size, 0);
    }
    
    ~Circular_Buffer() = default;
    
    void add(const float value) {
        jassert(position < size);
        buffer[position] = value;
        position++;
        if (position == size) position = 0;
    }

    void setAt(int pos, float value) {
        buffer[pos] = value;
    }
    
    float getBack() const {
        //if(position < 0 || position >= size) return 0;
        return buffer[position];
    }

    float getNext(float fractional) {
        return cubicInter(position, fractional);
    }

    float getOffset(int offset) {
        return buffer[validPos(position + offset)];
    }

    float getAt(float pos) {
        const int roundPos = juce::roundToInt(pos);
        const float fractional = pos - static_cast<float>(roundPos);
        return cubicInter(roundPos, fractional);
    }
    
    void resize(const int newSize) {
        buffer.resize(newSize, 0);
        size = newSize;
        if(position >= newSize) {
            position = 0;
            reset();
        }
    }
    
    void reset() {
        std::fill(buffer.begin(), buffer.end(), 0.f);
        //position = 0;
    }
    
    int validPos(const int pos) const {
        if(pos < 0) return size + pos;
        if(pos >= size) return pos - size;
        return pos;
    }
    
    float cubicInter(const int pos, const float fractional) {
        float a = buffer[validPos(pos-2)];
        float b = buffer[validPos(pos-1)];
        float c = buffer[validPos(pos)];
        float d = buffer[validPos(pos+1)];
        float cbDiff = c-b;
        float k1 = (c-a) * 0.5;
        float k3 = k1 + (d-b) * 0.5 - cbDiff * 2;
        float k2 = cbDiff - k3 - k1;
        return b + fractional * (k1 + fractional * (k2 + fractional * k3));
    }

    int getLength() const {
        return size;
    }
};

class Single_Delay {
protected:
    Circular_Buffer buffer;
    int length = 0;
    float decay = 1.f; /// 1 = no decay, 0 = instant decay
    double sampleRate = 44100;
    float fractional = 0.f;
    
public:
    Single_Delay() = default;
    ~Single_Delay() = default;

    /*
    Single_Delay(int fs, float time, float decay) {
        this->fs = fs;
        length = (int)(fs * time);
        buffer.resize(length);
        this->decay = decay;
    }
    */
    
    void prepareToPlay(const double fs, const float time, const float decay) {
        sampleRate = fs;
        length = juce::roundToInt(static_cast<float>(fs) * time);
        buffer.resize(length);
        buffer.reset();
        this->decay = decay;
    }
    
    void reset() {
        buffer.reset();
    }
    
    void delay(const float sample) {
        buffer.add(sample);
    }
    
    void delayDecay(const float sample) {
        buffer.add(sample * decay);
    }
    
    float get() const {
        return buffer.getBack();
    }

    float getNext() {
        return buffer.getNext(fractional);
    }
    
    float getAt(const float position) {
        return buffer.getAt(position);
    }
    
    void setTime(const float newTime) {
        length = juce::roundToInt(sampleRate * newTime);
        buffer.resize(length);
    }

    void setLength(const float l) {
        const int newLength = juce::roundToInt(l);
        if(newLength == length) return;
        length = newLength;
        fractional = l - static_cast<float>(newLength);
        buffer.resize(newLength);
        //buffer.reset();
    }
    
    void setTimeSamples(const int bufferSize) {
        buffer.resize(bufferSize);
    }
    
    void setDecay(const float decay) {
        this->decay = decay;
    }
};

class Multi_Delay {
protected:
    double sampleRate = 44100;
    float decay = 0.85; /// 1 = no decay, 0 = instant decay
    float time = 150; /// in ms
    Mix_Matrix matrix;
    std::vector<Single_Delay*> delays;
    int delaySamples[NUM_CHANNELS] = {0};
    LFO lfos[NUM_CHANNELS];
    float depth = 0.f;
    
public:
    Multi_Delay() = default;
    ~Multi_Delay() = default;

    void prepareToPlay(const double fs, const float t, const float d) {
        sampleRate = fs;
        const float delaySec = t * 0.001f;
        for(size_t i=0; i<NUM_CHANNELS; i++) {
            delays.push_back(new Single_Delay());
            const float r = i * 1.f / NUM_CHANNELS;
            //delaySamples[i] = std::pow(2,r) * delaySec;
            const float dt = std::powf(2.f,r) * delaySec;
            delays[i]->prepareToPlay(sampleRate, dt, d);
            lfos[i].prepareToPlay(sampleRate);
            lfos[i].setType(LFO_type::Sine);
            const float rate = std::powf(2.f,r) / 2.f;
            lfos[i].setRate(rate);
        }
        time = t;
        decay = d;
        setLFODepth(10.f);
    }
    
    void setTime(const float t) {
        const float delaySec = t * 0.001f;
        for(size_t i=0; i<NUM_CHANNELS; i++) {
            const float r = static_cast<float>(i) * 1.f / NUM_CHANNELS;
            const float d = std::powf(2.f,r) * delaySec;
            delays[i]->setTime(d);
        }
    }
    
    void setLFODepth(const float d) {
        if(juce::approximatelyEqual(d, depth)) return;
        depth = d;
        for(size_t i=0; i<NUM_CHANNELS; i++) {
            const float r = static_cast<float>(i) * 1.f / NUM_CHANNELS;
            const float d_ = std::powf(2.f,r) * depth;
            lfos[i].setDepth(d_);
        }
    }
    
    void setDecay(const float d) {
        decay = d;
    }
    
    void delay(const size_t channel, const float sample) const {
        delays[channel]->delay(sample);
    }
    
    float get(const size_t channel) const {
        return delays[channel]->get();
    }
    
    data getAll() {
        data d;
        for(size_t i=0; i<NUM_CHANNELS; i++) {
            d.channels[i] = delays[i]->get();
        }
        return d;
    }
    
    void delayAll(data d) {
        for(size_t i=0; i<NUM_CHANNELS; i++) {
            delays[i]->delay(d.channels[i]);
        }
    }
    
    data process(const data &input) {
        const data output = getAll();
        //for(int i=0; i<NUM_CHANNELS; i++) output.channels[i] = delays[i].getAt(delaySamples[i]);
        const data mixed = matrix.Householder(output);
        for(size_t i=0; i<NUM_CHANNELS; i++) {
            const float sum = input.channels[i] + (mixed.channels[i] * decay * lfos[i].getValue());
            delay(i, sum);
        }
        return output;
    }
};

class DiffusionStep {
protected:
    bool flipPolarity[NUM_CHANNELS] = {false};
    std::vector<Single_Delay*> delays;
    Mix_Matrix matrix;
    int delaySamples[NUM_CHANNELS] = {0};
    std::random_device rd;
    
public:
    int delayRange = 150; /// in ms
    
    DiffusionStep() = default;
    ~DiffusionStep() = default;
    
    void configure(double fs) {
        float delayTime = static_cast<float>(delayRange) * 0.001f;
        //delays.resize(NUM_CHANNELS);
        for(int i=0; i<NUM_CHANNELS; i++) {
            delays.push_back(new Single_Delay());
            const float low = delayTime * static_cast<float>(i) / NUM_CHANNELS;
            const float high = delayTime * static_cast<float>(i+1) / NUM_CHANNELS;
            //delaySamples[i] = randomInRange(low, high);
            float d = randomInRange(low, high);
            if(d<0.001f) d = 0.001f;
            delays[i]->prepareToPlay(fs, d, 1.f);
            flipPolarity[i] = rand() % 2;
        }
    }
    
    data getAll() {
        data d;
        for(size_t i=0; i<NUM_CHANNELS; i++) {
            d.channels[i] = delays[i]->get();
        }
        return d;
    }
    
    void delayAll(const data &input) const {
        for(int i=0; i<NUM_CHANNELS; i++) {
            delays[i]->delay(input.channels[i]);
        }
    }
    
    data process(const data &input) {
        delayAll(input);
        const data output = getAll();
        //for(int i=0; i<NUM_CHANNELS; i++) output.channels[i] = delays[i].getAt(delaySamples[i]);
        data mixed = matrix.Hadamard(output);
        for(int i=0; i<NUM_CHANNELS; i++) {
            if(flipPolarity[i]) mixed.channels[i] *= -1;
        }
        return mixed;
    }

    /*
    float randomInRange(float a, float b) {
        float random = ((float) rand()) / (float) RAND_MAX;
        float diff = b - a;
        float r = random * diff;
        return a + r;
    }
    */

    float randomInRange(float a, float b) {
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(a, b);
        return dist(gen);
    }
};

class Diffuser {
protected:
    static constexpr int NUM_STEPS = 4;
    std::vector<DiffusionStep*> steps;
    
public:
    Diffuser() = default;
    ~Diffuser() = default;
    
    void prepareToPlay(float diffusionMS, double fs) {
        //steps.resize(NUM_STEPS);
        for(size_t i=0; i<NUM_STEPS; i++) {
            steps.push_back(new DiffusionStep());
        }
        for(size_t i=NUM_STEPS; i>0; i--) {
            steps[i-1]->delayRange = juce::roundToInt(diffusionMS);
            steps[i-1]->configure(fs);
            diffusionMS *= 0.5;
        }
    }
    
    data process(const data &input) const {
        data o = input;
        for(size_t i=0; i<NUM_STEPS; i++) {
            o = steps[i]->process(o);
        }
        return o;
    }
};

}

#endif
