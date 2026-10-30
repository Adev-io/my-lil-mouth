// test_onset.h: bursts on silence, fed hop by hop like the module does.
#pragma once
#include "Onset.h"
#include "check.h"
#include "signals.h"

// Bursts of noise (burstMs long) starting at each time in startsMs; returns the onset times in ms.
inline std::vector<float> onsetsOf (const std::vector<float>& startsMs, float burstMs, float totalMs)
{
    const int hop = 256, n = (int) (totalMs / 1000.f * SR);
    std::vector<float> x (n, 0.f);
    Noise rng (3);
    for (float s : startsMs)
        for (int i = (int) (s / 1000.f * SR); i < (int) ((s + burstMs) / 1000.f * SR) && i < n; ++i) x[i] = 0.5f * rng.next();
    Onset on;
    on.set (0.5f, 30.f, hop / SR);
    std::vector<float> got;
    for (int h = 0; h + hop <= n; h += hop)
    {
        float e = 0.f;
        for (int i = h; i < h + hop; ++i) e += x[i] * x[i];
        if (on.process (e / hop, 1e-6f)) got.push_back ((h + hop) / SR * 1000.f);
    }
    return got;
}

inline void testOnset()
{
    std::vector<float> starts;
    for (int i = 0; i < 8; ++i) starts.push_back (100.f + 200.f * i);
    const std::vector<float> got = onsetsOf (starts, 20.f, 1800.f);
    check (got.size() == 8, "onset eight bursts -> 8 onsets", (double) got.size());
    float worst = 0.f;
    for (size_t i = 0; i < got.size() && i < 8; ++i) worst = std::fmax (worst, got[i] - starts[i]);
    check (got.size() == 8 && worst <= 20.f, "onset each within 20 ms of its burst", worst);
    check (onsetsOf ({100.f, 120.f}, 10.f, 400.f).size() == 1, "onset bursts 20 ms apart -> 1");
    check (onsetsOf ({100.f, 140.f}, 20.f, 400.f).size() == 2, "onset bursts 40 ms apart -> 2");
    check (onsetsOf ({}, 20.f, 400.f).empty(), "onset silence -> none");
}
