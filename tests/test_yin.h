// test_yin.h: pitch of pure tones within 5 cents at both candidate thresholds; silence and noise unvoiced.
#pragma once
#include "Yin.h"
#include "check.h"
#include "signals.h"

inline void testYin()
{
    struct Case { float hz, fMin, fMax; };
    const Case cases[] = { {55.f, 50.f, 500.f}, {80.f, 70.f, 1100.f}, {220.f, 70.f, 1100.f},
                           {440.f, 70.f, 1100.f}, {900.f, 70.f, 1100.f}, {1046.5f, 70.f, 1100.f} };
    const float thresholds[] = {0.10f, 0.15f};
    char what[96];
    for (float th : thresholds)
        for (const Case& c : cases)
        {
            Yin yin (c.fMin, c.fMax, SR);
            yin.threshold = th;
            const std::vector<float> x = sine (c.hz, yin.length());
            const Yin::Result r = yin.process (x.data());
            std::snprintf (what, sizeof what, "yin %.1f Hz at %.2f within 5 cents", c.hz, th);
            check (r.f0 > 0.f && std::fabs (cents (r.f0, c.hz)) < 5.f, what, r.f0 > 0.f ? cents (r.f0, c.hz) : 999.0);
            std::snprintf (what, sizeof what, "yin %.1f Hz at %.2f confidence > 0.8", c.hz, th);
            check (r.confidence > 0.8f, what, r.confidence);
        }
    // a voice: harmonics shaped by formants; skip the first 4096 samples while the resonators settle
    const float voiced[] = {120.f, 220.f};
    for (float f0 : voiced)
    {
        Yin v (70.f, 1100.f, SR);
        const std::vector<float> x = vowel (f0, 730.f, 1090.f, 4096 + v.length());
        const Yin::Result r = v.process (x.data() + 4096);
        std::snprintf (what, sizeof what, "yin vowel at %.0f Hz within 5 cents", f0);
        check (r.f0 > 0.f && std::fabs (cents (r.f0, f0)) < 5.f, what, r.f0 > 0.f ? cents (r.f0, f0) : 999.0);
    }
    // above the range: unvoiced, not a subharmonic
    struct Over { float hz, fMin, fMax; };
    const Over over[] = { {1250.f, 70.f, 1100.f}, {2400.f, 150.f, 2000.f} };
    for (const Over& o : over)
    {
        Yin v (o.fMin, o.fMax, SR);
        const std::vector<float> x = sine (o.hz, v.length());
        const Yin::Result r = v.process (x.data());
        std::snprintf (what, sizeof what, "yin %.0f Hz above %.0f Hz -> unvoiced", o.hz, o.fMax);
        check (r.f0 == 0.f, what, r.f0);
    }
    Yin yin (70.f, 1100.f, SR);
    const std::vector<float> silent (yin.length(), 0.f);
    check (yin.process (silent.data()).f0 == 0.f, "yin silence unvoiced");
    Noise rng (7);
    std::vector<float> noise (yin.length());
    for (float& v : noise) v = 0.5f * rng.next();
    const Yin::Result rn = yin.process (noise.data());
    check (rn.f0 == 0.f, "yin white noise unvoiced", rn.f0);
}
