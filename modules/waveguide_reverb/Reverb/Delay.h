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

class Buffer {
protected:
    int size;
    int position = 0;
    std::vector<float> buffer;
    
public:
    Buffer() = default;
    Buffer(int size) {
        this->size = size;
        buffer.resize(size, 0);
    }
    
    ~Buffer() = default;
    
    void add(float value) {
        buffer[position] = value;
        position++;
        if (position == size) position = 0;
    }
    
    float get() {
        //if(position < 0 || position >= size) return 0;
        return buffer[position];
    }

    float getNext(float fractional) {
        return cubicInter(position, fractional);
    }

    float getOffset(int offset, float fractional) {
        return cubicInter(position + offset, fractional);
    }

    float get(float pos) {
        int roundPos = juce::roundToInt(pos);
        float fractional = pos - static_cast<float>(roundPos);
        return cubicInter(roundPos, fractional);
    }
    
    void resize(int newSize) {
        buffer.resize(newSize, 0);
        size = newSize;
        if(position >= newSize) {
            position = 0;
            reset();
        }
    }
    
    void reset() {
        std::fill(buffer.begin(), buffer.end(), 0);
    }
    
    int validPos(int pos) {
        if(pos < 0) return size + pos;
        if(pos >= size) return pos - size;
        return pos;
    }
    
    float cubicInter(int pos, float fractional) {
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

    int getLength() {
        return buffer.size();
    }
    
    /// unused functions
    float getAt(int pos) {
        int newpos = position - pos;
        if(newpos < 0) newpos += size;
        return buffer[newpos];
    }
    
    float get_last() {
        return buffer[size-1];
    }
};

class Single_Delay {
protected:
    Buffer buffer;
    int length = 0;
    float decay = 1.f; /// 1 = no decay, 0 = instant decay
    int fs = 44100;
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
    
    void prepareToPlay(int fs, float time, float decay) {
        this->fs = fs;
        length = fs * time;
        buffer.resize(length);
        buffer.reset();
        this->decay = decay;
    }
    
    void reset() {
        buffer.reset();
    }
    
    void delay(float sample) {
        buffer.add(sample);
    }
    
    void delayDecay(float sample) {
        buffer.add(sample * decay);
    }
    
    float get() {
        return buffer.get();
    }

    float getNext() {
        return buffer.getNext(fractional);
    }

    float getOffset(int offset) {
        return buffer.getOffset(offset, fractional);
    }
    
    float getAt(int position) {
        return buffer.getAt(position);
    }
    
    void setTime(float newTime) {
        length = juce::roundToInt(fs * newTime);
        buffer.resize(length);
    }

    void setLength(float l) {
        int newLength = juce::roundToInt(l);
        if(newLength == length) return;
        length = newLength;
        fractional = l - newLength;
        buffer.resize(newLength);
        //buffer.reset();
    }
    
    void setTimeSamples(int bufferSize) {
        buffer.resize(bufferSize);
    }
    
    void setDecay(float decay) {
        this->decay = decay;
    }
};

class Multi_Delay {
protected:
    int fs = 44100;
    float decay = 0.85; /// 1 = no decay, 0 = instant decay
    float time = 150; /// in ms
    Mix_Matrix matrix;
    std::vector<Single_Delay*> delays;
    int delaySamples[NUM_CHANNELS] = {0};
    LFO lfos[NUM_CHANNELS];
    float depth = 10;
    
public:
    Multi_Delay() = default;
    ~Multi_Delay() = default;
    /*
    Multi_Delay(int numDelays, int fs) {
        delays.resize(NUM_CHANNELS);
        for(int i=0; i<NUM_CHANNELS; i++) {
            delays[i].prepareToPlay(fs, 0, 1);
        }
    }
    */
    
    void prepareToPlay(int fs, float time, float decay) {
        //delays.resize(NUM_CHANNELS);
        float delaySec = time * 0.001;
        for(int i=0; i<NUM_CHANNELS; i++) {
            delays.push_back(new Single_Delay());
            float r = i * 1.0 / NUM_CHANNELS;
            //delaySamples[i] = std::pow(2,r) * delaySec;
            float d = std::pow(2,r) * delaySec;
            delays[i]->prepareToPlay(fs, d, decay);
            lfos[i].prepareToPlay(fs);
            lfos[i].setType(LFO_type::Sine);
            float rate = std::pow(2,r) / 2;
            lfos[i].setRate(rate);
        }
        this->time = time;
        this->decay = decay;
    }
    
    void setTime(float time) {
        float delaySec = time * 0.001;
        for(int i=0; i<NUM_CHANNELS; i++) {
            float r = i * 1.0 / NUM_CHANNELS;
            float d = std::pow(2,r) * delaySec;
            delays[i]->setTime(d);
        }
    }
    
    void setLFODepth(float depth) {
        if(depth == this->depth) return;
        for(int i=0; i<NUM_CHANNELS; i++) {
            float r = i * 1.0 / NUM_CHANNELS;
            float d = std::pow(2,r) * depth;
            lfos[i].setDepth(d);
        }
        this->depth = depth;
    }
    
    void setDecay(float decay) {
        this->decay = decay;
    }
    
    void delay(int channel, float sample) {
        delays[channel]->delay(sample);
    }
    
    float get(int channel) {
        return delays[channel]->get();
    }
    
    data getAll() {
        data d;
        for(int i=0; i<NUM_CHANNELS; i++) {
            d.channels[i] = delays[i]->get();
        }
        return d;
    }
    
    void delayAll(data d) {
        for(int i=0; i<NUM_CHANNELS; i++) {
            delays[i]->delay(d.channels[i]);
        }
    }
    
    data process(data input) {
        data output = getAll();
        //for(int i=0; i<NUM_CHANNELS; i++) output.channels[i] = delays[i].getAt(delaySamples[i]);
        data mixed = matrix.Householder(output);
        for(int i=0; i<NUM_CHANNELS; i++) {
            float sum = input.channels[i] + (mixed.channels[i] * decay * lfos[i].getValue());
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
    
    void configure(int fs) {
        float delayTime = delayRange * 0.001;
        //delays.resize(NUM_CHANNELS);
        for(int i=0; i<NUM_CHANNELS; i++) {
            delays.push_back(new Single_Delay());
            const float low = delayTime * static_cast<float>(i) / NUM_CHANNELS;
            const float high = delayTime * static_cast<float>(i+1) / NUM_CHANNELS;
            //delaySamples[i] = randomInRange(low, high);
            float d = randomInRange(low, high);
            if(d<0.001) d = 0.001;
            delays[i]->prepareToPlay(fs, d, 1.f);
            flipPolarity[i] = rand() % 2;
        }
    }
    
    data getAll() {
        data d;
        for(int i=0; i<NUM_CHANNELS; i++) {
            d.channels[i] = delays[i]->get();
        }
        return d;
    }
    
    void delayAll(data r) {
        for(int i=0; i<NUM_CHANNELS; i++) {
            delays[i]->delay(r.channels[i]);
        }
    }
    
    data process(data input) {
        delayAll(input);
        data output = getAll();
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
    
    void prepareToPlay(float diffusionMS, int fs) {
        //steps.resize(NUM_STEPS);
        for(int i=0; i<NUM_STEPS; i++) {
            steps.push_back(new DiffusionStep());
            steps[i]->delayRange = diffusionMS;
            steps[i]->configure(fs);
            diffusionMS *= 0.5;
        }
    }
    
    data process(data input) {
        data o = input;
        for(int i=0; i<NUM_STEPS; i++) {
            o = steps[i]->process(o);
        }
        return o;
    }
};

}

#endif
