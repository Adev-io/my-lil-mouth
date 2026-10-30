// Formant.h: F1 and F2 of a sung vowel by LPC. No Rack dependency.
// Decimate to about 12 kHz behind a 5 kHz low-pass, pre-emphasis 0.97, Hann, autocorrelation, Levinson-Durbin
// order 10 (Praat: twice the 5 formants under a 5.5 kHz ceiling), then pick peaks of the LPC envelope on a
// 512-point grid over 0..6 kHz with parabolic refinement. No polynomial root finder.
// Plain LPC locks onto harmonics when f0 is high (F1 off by more than 15 % from about 450 Hz), so it reports nothing
// above maxF0. F2 is the first envelope peak above F1, so a strong F3 under 3 kHz cannot take its place.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

struct Formant
{
    enum { ORDER = 10, GRID = 512 };
    struct Result { float f1, f2; bool ok; };
    float maxF0 = 400.f;   // test_formant passes up to 400 Hz; tuned on the rig

    void set (float sampleRate)
    {
        sr = sampleRate;
        dec = std::max (1, (int) std::lround (sampleRate / 12000.f));
        y.reserve (1024);   // the decimated window is 256 at any rate: no allocation on the audio thread
        rate = sampleRate / (float) dec;
        // 4th-order Butterworth low-pass at 5 kHz: two RBJ biquads, Q 0.5412 and 1.3066
        const float qs[2] = {0.5412f, 1.3066f};
        for (int s = 0; s < 2; ++s)
        {
            const float w = 2.f * 3.14159265f * 5000.f / sampleRate, cw = std::cos (w), al = std::sin (w) / (2.f * qs[s]);
            const float a0 = 1.f + al;
            lp[s].b0 = (1.f - cw) / 2.f / a0;
            lp[s].b1 = (1.f - cw) / a0;
            lp[s].b2 = lp[s].b0;
            lp[s].a1 = -2.f * cw / a0;
            lp[s].a2 = (1.f - al) / a0;
        }
        for (int k = 0; k < GRID; ++k)
            for (int i = 1; i <= ORDER; ++i)
            {
                const float w = 3.14159265f * (float) k / GRID * 12000.f / rate * (float) i;   // grid spans 0..6 kHz
                cosT[k][i] = std::cos (w);
                sinT[k][i] = std::sin (w);
            }
    }

    // x: n samples at the module rate, oldest first. f0: the pitch tracker's f0 (0 = unvoiced, still analysed).
    Result process (const float* x, int n, float f0)
    {
        Result r;
        r.f1 = r.f2 = 0.f;
        r.ok = false;
        if (f0 > maxF0) return r;
        // low-pass, decimate (the filters start from rest every window: stateless)
        y.clear();
        Biquad a = lp[0], b = lp[1];
        for (int i = 0; i < n; ++i)
        {
            const float v = b.run (a.run (x[i]));
            if (i % dec == dec - 1) y.push_back (v);
        }
        const int m = (int) y.size();
        if (m <= ORDER + 1) return r;
        // pre-emphasis then Hann
        for (int i = m - 1; i > 0; --i) y[i] -= 0.97f * y[i - 1];
        for (int i = 0; i < m; ++i) y[i] *= 0.5f - 0.5f * std::cos (2.f * 3.14159265f * (float) i / (float) (m - 1));
        // autocorrelation, Levinson-Durbin
        double ac[ORDER + 1];
        for (int l = 0; l <= ORDER; ++l)
        {
            double s = 0.0;
            for (int i = l; i < m; ++i) s += (double) y[i] * y[i - l];
            ac[l] = s;
        }
        if (! (ac[0] > 1e-12)) return r;
        ac[0] *= 1.0 + 1e-9;   // tiny white-noise floor keeps the recursion stable on pure tones
        double a_[ORDER + 1] = {1.0}, tmp[ORDER + 1], err = ac[0];
        for (int i = 1; i <= ORDER; ++i)
        {
            double k = -ac[i];
            for (int j = 1; j < i; ++j) k -= a_[j] * ac[i - j];
            k /= err;
            for (int j = 0; j <= i; ++j) tmp[j] = a_[j];
            for (int j = 1; j < i; ++j) a_[j] = tmp[j] + k * tmp[i - j];
            a_[i] = k;
            err *= 1.0 - k * k;
            if (! (err > 0.0)) return r;
        }
        // envelope in dB on the grid
        for (int k = 0; k < GRID; ++k)
        {
            double re = 1.0, im = 0.0;
            for (int i = 1; i <= ORDER; ++i)
            {
                re += a_[i] * cosT[k][i];
                im -= a_[i] * sinT[k][i];
            }
            env[k] = (float) -std::log10 (re * re + im * im + 1e-30);
        }
        // peaks
        const float hzPerBin = 6000.f / GRID;
        float best1 = -1e9f;
        for (int k = 1; k < GRID - 1; ++k)
        {
            if (! (env[k] > env[k - 1] && env[k] >= env[k + 1])) continue;
            const float den = env[k - 1] - 2.f * env[k] + env[k + 1];
            const float f = ((float) k + (den < 0.f ? 0.5f * (env[k - 1] - env[k + 1]) / den : 0.f)) * hzPerBin;
            if (f >= 250.f && f <= 900.f && env[k] > best1) { best1 = env[k]; r.f1 = f; }
        }
        if (r.f1 <= 0.f) return r;
        for (int k = 1; k < GRID - 1; ++k)
        {
            if (! (env[k] > env[k - 1] && env[k] >= env[k + 1])) continue;
            const float den = env[k - 1] - 2.f * env[k] + env[k + 1];
            const float f = ((float) k + (den < 0.f ? 0.5f * (env[k - 1] - env[k + 1]) / den : 0.f)) * hzPerBin;
            if (f > r.f1 && f >= 600.f && f <= 3000.f) { r.f2 = f; break; }   // the first peak above F1
        }
        r.ok = r.f2 > 0.f;
        if (! r.ok) r.f1 = 0.f;
        return r;
    }

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float run (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };
    Biquad lp[2];
    float sr = 48000.f, rate = 12000.f;
    int dec = 4;
    float cosT[GRID][ORDER + 1], sinT[GRID][ORDER + 1], env[GRID];
    std::vector<float> y;
};
