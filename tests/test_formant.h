// test_formant.h: F1 and F2 of synthetic sung vowels within 15 %; a high voice reports nothing.
#pragma once
#include "Formant.h"
#include "check.h"
#include "signals.h"

inline void testFormant()
{
    Formant fm;
    fm.set (SR);
    struct Case { const char* name; float f0, f1, f2, f3; };
    const Case cases[] = { {"ah 120 Hz", 120.f, 730.f, 1090.f, 3300.f}, {"ee 120 Hz", 120.f, 270.f, 2290.f, 3300.f},
                           {"ee 220 Hz (female /i/)", 220.f, 310.f, 2790.f, 3300.f},
                           {"F3 2400 under the F2 band top", 120.f, 500.f, 1400.f, 2400.f} };
    char what[96];
    for (const Case& c : cases)
    {
        // 4096 samples so the resonators settle; analyse the last 1024 like the module
        const std::vector<float> x = vowel (c.f0, c.f1, c.f2, 4096, c.f3);
        const Formant::Result r = fm.process (x.data() + 3072, 1024, c.f0);
        std::snprintf (what, sizeof what, "formant %s F1 %.0f within 15 %%", c.name, c.f1);
        check (r.ok && std::fabs (r.f1 / c.f1 - 1.f) <= 0.15f, what, r.f1);
        std::snprintf (what, sizeof what, "formant %s F2 %.0f within 15 %%", c.name, c.f2);
        check (r.ok && std::fabs (r.f2 / c.f2 - 1.f) <= 0.15f, what, r.f2);
    }
    // ah from 150 to 400 Hz: where harmonics are sparse it may hold (no result), but a result is never off by 15 %
    int reported = 0, wrong = 0;
    for (float f0 = 150.f; f0 <= 400.f; f0 += 25.f)
    {
        const std::vector<float> x = vowel (f0, 730.f, 1090.f, 4096);
        const Formant::Result r = fm.process (x.data() + 3072, 1024, f0);
        if (! r.ok) continue;
        ++reported;
        wrong += std::fabs (r.f1 / 730.f - 1.f) > 0.15f || std::fabs (r.f2 / 1090.f - 1.f) > 0.15f;
    }
    check (wrong == 0, "formant ah 150-400 Hz: no result off by more than 15 %", wrong);
    check (reported >= 8, "formant ah 150-400 Hz: reports at 8 or more of 11 pitches", reported);
    const std::vector<float> high = vowel (500.f, 730.f, 1090.f, 4096);
    check (! fm.process (high.data() + 3072, 1024, 500.f).ok, "formant 500 Hz voice -> hold (no result)");
    const std::vector<float> silent (1024, 0.f);
    check (! fm.process (silent.data(), 1024, 0.f).ok, "formant silence -> no result");
}
