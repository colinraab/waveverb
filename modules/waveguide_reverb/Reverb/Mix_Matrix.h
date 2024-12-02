#ifndef COLIN_MIX_MATRIX_H
#define COLIN_MIX_MATRIX_H
#include <cmath>
#include <random>

/*
  ==============================================================================

    Mix_Matrix.h
    Created: 2 Nov 2024 10:52:25am
    Author:  Colin Raab

  ==============================================================================
*/

namespace Colin
{

static constexpr int NUM_CHANNELS = 16;

struct data {
    float channels[NUM_CHANNELS] = {0.f};
    
    void fill(float input) {
        for(size_t i=0; i<NUM_CHANNELS; i++) {
            channels[i] = input;
        }
    }

    void scale(float scalar) {
        for(size_t i=0; i<NUM_CHANNELS; i++) {
            channels[i] = channels[i]*scalar;
        }
    }
};

class Mix_Matrix {
protected:
    float coeffs[NUM_CHANNELS] = {0};
    std::random_device rd;

public:
    Mix_Matrix() {
        coeffs[0] = 1;
        coeffs[1] = 0;
        for (size_t i = 1; i < (NUM_CHANNELS/2); ++i) {
            double phase = M_PI * i / NUM_CHANNELS;
            coeffs[2*i] = static_cast<float>(std::cos(phase));
            coeffs[2*i + 1] = static_cast<float>(std::sin(phase));
        }
    }
    ~Mix_Matrix() = default;
    
    data Householder(data r) {
        data o;
        float factor = -2.f / NUM_CHANNELS;
        float sum = 0.f;
        for(int i=0; i<NUM_CHANNELS; i++)
            sum += r.channels[i];
        sum *= factor;
        for(size_t i=0; i<NUM_CHANNELS; i++) {
            o.channels[i] = r.channels[i] + sum;
        }
        return o;
    }
    
    data Hadamard(data r) {
        float a = 0;
        float b = 0;
        //data* point = &r;
        recursiveUnscaled(&r, NUM_CHANNELS);
        data o = r;
        float factor = std::sqrt(1.f/NUM_CHANNELS);
        for(size_t i=0; i<NUM_CHANNELS; i++) {
            o.channels[i] *= factor;
        }
        return o;
    }

    void recursiveUnscaled(data* r, int size) {
        if (size <= 1) return;
        int hSize = size/2;

        // Two (unscaled) Hadamards of half the size
        recursiveUnscaled(r, hSize);
        recursiveUnscaled(r + hSize, hSize);

        // Combine the two halves using sum/difference
        for (int i = 0; i < hSize; ++i) {
            double a = r->channels[i];
            double b = r->channels[i + hSize];
            r->channels[i] = (a + b);
            r->channels[i + hSize] = (a - b);
        }
    }

    data intermix(data f, data m, float fCoeff, float mCoeff) {
        data o;
        for(size_t i=0; i<NUM_CHANNELS; i++) {
            o.channels[i] = fCoeff * f.channels[i] + mCoeff * m.channels[i];
        }
        return o;
        /*
        data o;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for(int i=0; i<NUM_CHANNELS; i++) {
            if(dist(gen) > blend) {
                o.channels[i] = f.channels[i];
            }
            else {
                o.channels[i] = m.channels[i];
            }
        }
        return o;
        */
    }

    data stereoToMulti(float l, float r) {
        data o;
        o.channels[0] = l;
        o.channels[1] = r;
        for(size_t i=2; i<NUM_CHANNELS; i+=2) {
            o.channels[i] = l * coeffs[i] + r * coeffs[i+1];
            o.channels[i+1] = r * coeffs[i] - l * coeffs[i+1];
        }
        return o;
    }
    
    void multiToStereo(data input, float &l, float &r) {
        l = input.channels[0];
        r = input.channels[1];
        for(size_t i=2; i<NUM_CHANNELS; i+=2) {
            l += input.channels[i] * coeffs[i] - input.channels[i+1] * coeffs[i+1];
            r += input.channels[i+1] * coeffs[i] + input.channels[i] * coeffs[i+1];
        }
        //l *= std::sqrt(2.0/NUM_CHANNELS);
        //r *= std::sqrt(2.0/NUM_CHANNELS);
    }

    data combine(const data &one, const data &two) {
        data o;
        for(size_t i=0; i<NUM_CHANNELS; i++) {
            o.channels[i] = one.channels[i] + two.channels[i];
        }
        return o;
    }
    
    void cheapEnergyCrossfade(float x, float &toCoeff, float &fromCoeff) {
        float x2 = 1.f - x;
        float A = x * x2;
        float B = A * (1.f + 1.4186f * A);
        float C = (B + x);
        float D = (B + x2);
        toCoeff = C * C;
        fromCoeff = D * D;
    }
};

}


#endif
