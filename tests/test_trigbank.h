// test_trigbank.h: train a kick-like thump and a hat-like hiss, classify unseen hits, reject tones, save and load.
#pragma once
#include "Fft.h"
#include "Features.h"
#include "TrigBank.h"
#include "check.h"
#include "signals.h"

// Features of 512 samples, Hann-windowed, as the module takes them one hop after an onset: the window starts
// up to one hop (256 samples) before the hit, so `lead` samples of silence come first.
inline Features hitFeatures (const std::vector<float>& x, int lead = 0)
{
    std::vector<std::complex<double>> s (512);
    for (int i = 0; i < 512; ++i)
        s[i] = (i >= lead ? x[i - lead] : 0.f) * (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * i / 511.0));
    voice::fft (s);
    Features f;
    f.compute (s, SR);
    return f;
}

inline bool sameBank (const TrigBank& a, const TrigBank& b)
{
    for (int s = 0; s < TrigBank::SLOTS; ++s)
    {
        if (a.slots[s].n != b.slots[s].n || a.slots[s].spread != b.slots[s].spread) return false;
        for (int i = 0; i < TrigBank::DIM; ++i)
            if (a.slots[s].c[i] != b.slots[s].c[i]) return false;
    }
    return true;
}

inline void testTrigBank()
{
    TrigBank bank;
    check (bank.classify (hitFeatures (thump (60.f, 512)).vec) == -1, "trigbank untrained -> -1");

    Noise rng (11);
    bank.beginTraining (0);
    for (int i = 0; i < 5; ++i) bank.train (hitFeatures (thump (55.f + 4.f * i, 512)).vec);
    bank.endTraining();
    bank.beginTraining (1);
    for (int i = 0; i < 5; ++i) bank.train (hitFeatures (hiss (rng, 512)).vec);
    bank.endTraining();
    check (bank.slots[0].trained() && bank.slots[1].trained(), "trigbank two slots trained");

    // unseen hits, with the window starting 0, 128 or 256 samples before the hit (where the module's lands)
    const int leads[] = {0, 128, 256};
    char what[96];
    for (int lead : leads)
    {
        int kicks = 0, hats = 0;
        for (int i = 0; i < 10; ++i)
        {
            kicks += bank.classify (hitFeatures (thump (52.f + 2.5f * i, 512), lead).vec) == 0;
            hats += bank.classify (hitFeatures (hiss (rng, 512), lead).vec) == 1;
        }
        std::snprintf (what, sizeof what, "trigbank unseen thumps, lead %d -> slot 0 (>= 9 of 10)", lead);
        check (kicks >= 9, what, kicks);
        std::snprintf (what, sizeof what, "trigbank unseen hiss, lead %d -> slot 1 (>= 9 of 10)", lead);
        check (hats >= 9, what, hats);
    }
    check (bank.classify (hitFeatures (sine (1000.f, 512)).vec) == -1, "trigbank 1 kHz tone rejected");

    // two examples are not enough: the old sound stays
    TrigBank::Slot before = bank.slots[0];
    bank.beginTraining (0);
    bank.train (hitFeatures (hiss (rng, 512)).vec);
    bank.train (hitFeatures (hiss (rng, 512)).vec);
    bank.endTraining();
    check (bank.slots[0].n == before.n && bank.slots[0].c[0] == before.c[0], "trigbank 2 examples keep the old slot");

    // a 13th example is ignored
    bank.beginTraining (2);
    for (int i = 0; i < 13; ++i) bank.train (hitFeatures (thump (100.f, 512)).vec);
    check (bank.examples() == 12, "trigbank 13th example ignored", bank.examples());
    bank.endTraining();
    check (bank.slots[2].n == 12 && bank.training() == -1, "trigbank slot keeps 12, training over");

    // train and endTraining without beginTraining do nothing
    TrigBank idle = bank;
    idle.train (hitFeatures (hiss (rng, 512)).vec);
    idle.endTraining();
    check (sameBank (idle, bank), "trigbank train without beginTraining ignored");

    TrigBank copy;
    check (copy.load (bank.save()), "trigbank load accepts save");
    check (sameBank (copy, bank), "trigbank save/load round trip equal");
    const std::string good = bank.save();
    const char* bad[] = { "v2", "v1 3 0.1 nan", "v1 3.9 0.1", "v1 3 -0.5" };
    for (const char* b : bad)
    {
        TrigBank t = bank;
        std::snprintf (what, sizeof what, "trigbank refuses \"%s\" and keeps the bank", b);
        check (! t.load (b) && sameBank (t, bank), what);
    }
    TrigBank junk = bank;
    check (! junk.load (good + " 7") && sameBank (junk, bank), "trigbank refuses trailing junk");
    TrigBank cut = bank;
    check (! cut.load (good.substr (0, good.size() / 2)) && sameBank (cut, bank), "trigbank refuses truncated text");
    bank.clear (0);
    check (! bank.slots[0].trained(), "trigbank clear");
}
