// test_formant.h: F1 and F2 of synthetic sung vowels within 15 %; a high voice reports nothing.
#pragma once
#include "Formant.h"
#include "check.h"
#include "signals.h"

inline void testFormant()
{
    Formant fm;
    fm.set (SR);
    struct Case { const char* name; float f0, f1, f2; };
    const Case cases[] = { {"ah 120 Hz", 120.f, 730.f, 1090.f}, {"ee 120 Hz", 120.f, 270.f, 2290.f},
                           {"ee 220 Hz (female /i/)", 220.f, 310.f, 2790.f} };
    char what[96];
    for (const Case& c : cases)
    {
        // 4096 samples so the resonators settle; analyse the last 1024 like the module
        const std::vector<float> x = vowel (c.f0, c.f1, c.f2, 4096);
        const Formant::Result r = fm.process (x.data() + 3072, 1024, c.f0);
        std::snprintf (what, sizeof what, "formant %s F1 %.0f within 15 %%", c.name, c.f1);
        check (r.ok && std::fabs (r.f1 / c.f1 - 1.f) <= 0.15f, what, r.f1);
        std::snprintf (what, sizeof what, "formant %s F2 %.0f within 15 %%", c.name, c.f2);
        check (r.ok && std::fabs (r.f2 / c.f2 - 1.f) <= 0.15f, what, r.f2);
    }
    const std::vector<float> high = vowel (700.f, 730.f, 1090.f, 4096);
    check (! fm.process (high.data() + 3072, 1024, 700.f).ok, "formant 700 Hz voice -> hold (no result)");
    const std::vector<float> silent (1024, 0.f);
    check (! fm.process (silent.data(), 1024, 0.f).ok, "formant silence -> no result");
}
