// test_notelock.h: vibrato holds the note, a 60-cent step changes it once, the glide lands in time.
#pragma once
#include "NoteLock.h"
#include "check.h"
#include "signals.h"

inline void testNoteLock()
{
    NoteLock nl;
    nl.set (55.f, 0.f);
    int changes = 0;
    for (int h = 0; h < 200; ++h)   // 30-cent vibrato at 5.5 Hz, one value per 256-sample hop
    {
        const float c = 30.f * std::sin (2.f * PI_F * 5.5f * (float) h * 256.f / SR);
        changes += nl.process (440.f * std::pow (2.f, c / 1200.f), true).changed;
    }
    check (nl.note() == 69 && changes == 1, "notelock vibrato holds A4 (one change = the start)", changes);

    changes = 0;
    for (int h = 0; h < 50; ++h) changes += nl.process (440.f * std::pow (2.f, 60.f / 1200.f), true).changed;
    check (nl.note() == 70 && changes == 1, "notelock 60-cent step changes once", changes);

    NoteLock one;
    one.set (55.f, 0.f);
    one.process (440.f, true);
    const NoteLock::Out o = one.process (466.16f, true);
    check (! o.changed, "notelock one hop away is not enough");
    check (one.process (466.16f, true).changed, "notelock second hop away changes");

    // glide 100 ms: from A4 to B4 (two semitones = 1/6 V) lands within 110 ms
    NoteLock g;
    g.set (55.f, 100.f);
    g.process (440.f, true);
    for (int i = 0; i < 10; ++i) g.step (1.f / SR);
    g.process (493.88f, true);
    g.process (493.88f, true);
    int samples = 0;
    while (std::fabs (g.step (1.f / SR) - g.target()) > 1e-6f && samples < 48000) ++samples;
    check (samples <= (int) (0.110f * SR) && samples >= (int) (0.090f * SR), "notelock glide 100 ms (+-10 %)", samples / SR * 1000.f);

    // unvoiced holds the note; the next voiced start snaps with no hold
    g.process (0.f, false);
    check (std::fabs (g.step (1.f / SR) - (71.f - 60.f) / 12.f) < 1e-6f, "notelock unvoiced holds");
    check (g.process (261.63f, true).changed && g.note() == 60, "notelock voiced start snaps");
    check (std::fabs (g.step (1.f / SR)) < 1e-6f, "notelock voiced start skips the glide");

    // LOCK does something: a voice 45 cents sharp with 30-cent vibrato (15..75 cents) stays A4 at LOCK 100,
    // and flips at LOCK 55 (plain rounding would flip too)
    const float locks[] = {100.f, 55.f};
    int flips[2] = {};
    for (int k = 0; k < 2; ++k)
    {
        NoteLock s;
        s.set (locks[k], 0.f);
        for (int h = 0; h < 200; ++h)
        {
            const float c = 45.f + 30.f * std::sin (2.f * PI_F * 5.5f * (float) h * 256.f / SR);
            flips[k] += s.process (440.f * std::pow (2.f, c / 1200.f), true).changed;
        }
    }
    check (flips[0] == 1, "notelock LOCK 100 holds a sharp vibrato", flips[0]);
    check (flips[1] > 1, "notelock LOCK 55 lets the same vibrato through", flips[1]);

    // the gate falls 50 ms into a 100 ms glide: NOTE stays where it is
    NoteLock h;
    h.set (60.f, 100.f);
    h.process (440.f, true);
    h.process (493.88f, true);
    h.process (493.88f, true);
    for (int i = 0; i < 2400; ++i) h.step (1.f / SR);
    h.process (0.f, false);
    const float mid = h.step (1.f / SR);
    for (int i = 0; i < 4800; ++i) h.step (1.f / SR);
    check (std::fabs (h.step (1.f / SR) - mid) < 1e-6f && std::fabs (mid - h.target()) > 0.01f, "notelock gate off mid-glide holds", mid);

    // GLIDE turned to 0 halfway: the next step lands
    NoteLock z;
    z.set (60.f, 100.f);
    z.process (440.f, true);
    z.process (493.88f, true);
    z.process (493.88f, true);
    for (int i = 0; i < 2400; ++i) z.step (1.f / SR);
    z.set (60.f, 0.f);
    check (std::fabs (z.step (1.f / SR) - z.target()) < 1e-6f, "notelock GLIDE to 0 mid-glide lands");
}
