// Onset.h: percussive onsets from the energy of each hop. No Rack dependency.
// Onset = this hop's energy is over the floor and RATIO times the mean of the last 3 hops, and the last onset is at
// least the debounce ago. ponytail: energy only; add spectral flux if hits over a sustained sung note are missed.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <cmath>

struct Onset
{
    // sensitivity 0..1: the rise needed falls from 6x (7.8 dB) to 2x (3 dB); default 0.5 = 4x
    void set (float sensitivity, float debounceMs, float hopSeconds)
    {
        ratio = 6.f - 4.f * sensitivity;
        debounceHops = (int) std::ceil (debounceMs / 1000.f / hopSeconds);
    }

    // hopEnergy: mean square of the hop's samples; floor: the same measure for the calibrated noise floor + THRESH
    bool process (float hopEnergy, float floor)
    {
        const float mean = (hist[0] + hist[1] + hist[2]) / 3.f;
        hist[2] = hist[1];
        hist[1] = hist[0];
        hist[0] = hopEnergy;
        if (since < debounceHops) ++since;
        if (since < debounceHops || hopEnergy <= floor || hopEnergy <= ratio * mean) return false;
        since = 0;
        return true;
    }

private:
    float ratio = 4.f, hist[3] = {};
    int debounceHops = 6, since = 1 << 20;
};
