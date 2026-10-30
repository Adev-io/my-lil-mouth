// test_trigbank.h: train a kick-like thump and a hat-like hiss, classify unseen hits, reject a tone, save and load.
#pragma once
#include "Fft.h"
#include "Features.h"
#include "TrigBank.h"
#include "check.h"
#include "signals.h"

// Features of the first 512 samples of a hit, Hann-windowed, as the module takes them 2 hops after an onset.
inline Features hitFeatures (const std::vector<float>& x)
{
    std::vector<std::complex<double>> s (512);
    for (int i = 0; i < 512; ++i) s[i] = x[i] * (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * i / 511.0));
    voice::fft (s);
    Features f;
    f.compute (s, SR);
    return f;
}

inline void testTrigBank()
{
    TrigBank bank;
    check (bank.classify (hitFeatures (thump (60.f, 512)).vec) == -1, "trigbank untrained -> -1");

    Noise rng (11);
    bank.beginTraining (0);
    for (int i = 0; i < 5; ++i) bank.train (0, hitFeatures (thump (55.f + 4.f * i, 512)).vec);
    bank.endTraining (0);
    bank.beginTraining (1);
    for (int i = 0; i < 5; ++i) bank.train (1, hitFeatures (hiss (rng, 512)).vec);
    bank.endTraining (1);
    check (bank.slots[0].trained() && bank.slots[1].trained(), "trigbank two slots trained");

    int kicks = 0, hats = 0;
    for (int i = 0; i < 10; ++i)
    {
        kicks += bank.classify (hitFeatures (thump (52.f + 2.5f * i, 512)).vec) == 0;
        hats += bank.classify (hitFeatures (hiss (rng, 512)).vec) == 1;
    }
    check (kicks >= 9, "trigbank unseen thumps -> slot 0 (>= 9 of 10)", kicks);
    check (hats >= 9, "trigbank unseen hiss -> slot 1 (>= 9 of 10)", hats);
    check (bank.classify (hitFeatures (sine (1000.f, 512)).vec) == -1, "trigbank 1 kHz tone rejected");

    // two examples are not enough: the old sound stays
    TrigBank::Slot before = bank.slots[0];
    bank.beginTraining (0);
    bank.train (0, hitFeatures (hiss (rng, 512)).vec);
    bank.train (0, hitFeatures (hiss (rng, 512)).vec);
    bank.endTraining (0);
    check (bank.slots[0].n == before.n && bank.slots[0].c[0] == before.c[0], "trigbank 2 examples keep the old slot");

    // a 13th example is ignored
    bank.beginTraining (2);
    for (int i = 0; i < 13; ++i) bank.train (2, hitFeatures (thump (100.f, 512)).vec);
    check (bank.examples() == 12, "trigbank 13th example ignored", bank.examples());
    bank.endTraining (2);
    check (bank.slots[2].n == 12, "trigbank slot keeps 12", bank.slots[2].n);

    TrigBank copy;
    check (copy.load (bank.save()), "trigbank load accepts save");
    bool same = true;
    for (int s = 0; s < TrigBank::SLOTS; ++s)
    {
        same = same && copy.slots[s].n == bank.slots[s].n && copy.slots[s].spread == bank.slots[s].spread;
        for (int i = 0; i < TrigBank::DIM; ++i) same = same && copy.slots[s].c[i] == bank.slots[s].c[i];
    }
    check (same, "trigbank save/load round trip equal");
    check (! copy.load ("v1 3 0.1 nan") && copy.slots[0].n == bank.slots[0].n, "trigbank malformed text leaves the bank");
    check (! copy.load ("v2"), "trigbank wrong version refused");
    bank.clear (0);
    check (! bank.slots[0].trained(), "trigbank clear");
}
