// signals.h: synthetic test signals at 48 kHz. Same seed, same signal.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

static const float SR = 48000.f;
static const float PI_F = 3.14159265f;

struct Noise   // white noise -1..1, a plain LCG so every platform makes the same numbers
{
    uint32_t s;
    explicit Noise (uint32_t seed) : s (seed) {}
    float next()
    {
        s = s * 1664525u + 1013904223u;
        return (float) (s >> 8) / 8388608.f - 1.f;
    }
};

inline std::vector<float> sine (float hz, int n, float amp = 0.5f)
{
    std::vector<float> x (n);
    for (int i = 0; i < n; ++i) x[i] = amp * std::sin (2.f * PI_F * hz * (float) i / SR);
    return x;
}

// Two-pole resonator at f Hz with bandwidth bw Hz, unity gain at DC-ish scale (good enough for tests)
inline void resonate (std::vector<float>& x, float f, float bw)
{
    const float r = std::exp (-PI_F * bw / SR), c = 2.f * r * std::cos (2.f * PI_F * f / SR), d = -r * r;
    float y1 = 0.f, y2 = 0.f;
    for (float& v : x)
    {
        const float y = (1.f - r) * v + c * y1 + d * y2;
        y2 = y1;
        y1 = y;
        v = y;
    }
}

// A sung vowel: an impulse train at f0, tilted -6 dB/octave above 100 Hz like a real voice (glottis -12, lips +6),
// through resonators at the formants (bandwidth 80, 100, 120 Hz)
inline std::vector<float> vowel (float f0, float f1, float f2, int n, float f3 = 3300.f)
{
    std::vector<float> x (n, 0.f);
    const float period = SR / f0;
    for (float t = 0.f; t < (float) n; t += period) x[(int) t] = 1.f;
    const float tilt = 1.f - std::exp (-2.f * PI_F * 100.f / SR);
    float lp = 0.f;
    for (float& v : x) v = lp += tilt * (v - lp);
    resonate (x, f1, 80.f);
    resonate (x, f2, 100.f);
    resonate (x, f3, 120.f);   // F3, so the envelope has a ceiling like a real voice
    float peak = 0.f;
    for (float v : x) peak = std::fmax (peak, std::fabs (v));
    for (float& v : x) v *= 0.5f / peak;
    return x;
}

// A beatbox kick: a sine falling from f to f/2 with a 40 ms decay
inline std::vector<float> thump (float f, int n)
{
    std::vector<float> x (n);
    float ph = 0.f;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / SR;
        ph += 2.f * PI_F * f * (0.5f + 0.5f * std::exp (-t / 0.02f)) / SR;
        x[i] = 0.8f * std::exp (-t / 0.04f) * std::sin (ph);
    }
    return x;
}

// A beatbox hat: white noise, first-difference high-passed, 15 ms decay
inline std::vector<float> hiss (Noise& rng, int n)
{
    std::vector<float> x (n);
    float prev = 0.f;
    for (int i = 0; i < n; ++i)
    {
        const float w = rng.next();
        x[i] = 0.5f * std::exp (-(float) i / SR / 0.015f) * (w - prev);
        prev = w;
    }
    return x;
}

inline float cents (float got, float want) { return 1200.f * std::log2 (got / want); }
