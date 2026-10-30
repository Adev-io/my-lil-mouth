// Yin.h: monophonic pitch by YIN (de Cheveigne and Kawahara, 2002). No Rack dependency.
// The buffer is 2 x tauMax + 1 samples, tauMax = sampleRate / fMin, so the pitch floor sets the latency.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

struct Yin
{
    struct Result { float f0, confidence; };   // f0 = 0: unvoiced (no dip under the threshold)

    float threshold = 0.15f;   // provisional, unsourced: test_yin runs 0.10 and 0.15

    Yin (float fMin = 70.f, float fMax = 1100.f, float sampleRate = 48000.f) { set (fMin, fMax, sampleRate); }

    void set (float fMin, float fMax, float sampleRate)
    {
        sr = sampleRate;
        tauMax = (int) std::ceil (sampleRate / fMin);
        tauMin = std::max (2, (int) std::floor (sampleRate / fMax));
        d.assign (tauMax + 2, 1.f);
    }

    int length() const { return 2 * tauMax + 1; }

    // x: length() samples, oldest first.
    // ponytail: O(tauMax^2) per call (about 0.5 M multiply-adds at a 70 Hz floor); FFT autocorrelation if CPU matters
    Result process (const float* x)
    {
        const int w = tauMax;
        float run = 0.f;
        for (int tau = 1; tau <= tauMax + 1; ++tau)
        {
            float s = 0.f;
            for (int j = 0; j < w; ++j)
            {
                const float e = x[j] - x[j + tau];
                s += e * e;
            }
            run += s;
            d[tau] = run > 0.f ? s * tau / run : 1.f;   // cumulative mean normalised difference
        }
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (d[tau] >= threshold) continue;
            while (tau < tauMax && d[tau + 1] < d[tau]) ++tau;   // first dip under the threshold: walk to its bottom
            const float a = d[tau - 1], b = d[tau], c = d[tau + 1], den = a - 2.f * b + c;
            const float shift = den > 0.f ? 0.5f * (a - c) / den : 0.f;   // parabolic interpolation
            Result r;
            r.f0 = sr / ((float) tau + shift);
            r.confidence = 1.f - b;
            return r;
        }
        Result none;
        none.f0 = 0.f;
        none.confidence = 0.f;
        return none;
    }

private:
    float sr = 48000.f;
    int tauMin = 2, tauMax = 686;
    std::vector<float> d;
};
