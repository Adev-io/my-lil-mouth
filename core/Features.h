// Features.h: timbre of one FFT frame: 8 octave band shares, spectral centroid, energy. No Rack dependency.
// vec[] is what the trigger bank compares: the 8 shares (sum 1) and the brightness (0..1), all on one scale.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

struct Features
{
    enum { BANDS = 8, DIM = 9 };
    float bands[BANDS] = {};   // shares of the 50 Hz .. 12.8 kHz energy in octaves 50-100, 100-200 ... 6400-12800
    float centroid = 0.f;      // Hz
    float energy = 0.f;        // sum of bin powers
    float vec[DIM] = {};

    // brightness: log centroid, 200 Hz -> 0, 8 kHz -> 1, clamped
    static float bright (float centroidHz)
    {
        if (! (centroidHz > 0.f)) return 0.f;
        return std::min (1.f, std::max (0.f, std::log2 (centroidHz / 200.f) / std::log2 (40.f)));
    }

    // spec: the full complex FFT of n points (n a power of two)
    void compute (const std::vector<std::complex<double>>& spec, float sampleRate)
    {
        const size_t n = spec.size();
        double band[BANDS] = {}, total = 0.0, weighted = 0.0, inBands = 0.0;
        for (size_t k = 1; k < n / 2; ++k)
        {
            const double p = std::norm (spec[k]), f = (double) k * sampleRate / (double) n;
            total += p;
            weighted += p * f;
            if (f < 50.0 || f >= 12800.0) continue;
            band[std::min ((int) BANDS - 1, (int) std::floor (std::log2 (f / 50.0)))] += p;
            inBands += p;
        }
        energy = (float) total;
        centroid = total > 0.0 ? (float) (weighted / total) : 0.f;
        for (int b = 0; b < BANDS; ++b) vec[b] = bands[b] = inBands > 0.0 ? (float) (band[b] / inBands) : 0.f;
        vec[BANDS] = bright (centroid);
    }
};
