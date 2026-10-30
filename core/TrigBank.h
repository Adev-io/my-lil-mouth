// TrigBank.h: four trained percussive sounds, nearest centroid with a reject radius. No Rack dependency.
// A slot learns from 3 to 12 examples (Dubler's documented cap is 12). classify() returns the slot or -1.
// The reject radius is a heuristic (2 x the slot's training spread, floor 0.3, because a few noisy examples
// underestimate their spread), tuned on the rig.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include "Features.h"
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

struct TrigBank
{
    enum { SLOTS = 4, DIM = Features::DIM, MIN_EX = 3, MAX_EX = 12 };

    struct Slot
    {
        float c[DIM] = {};
        float spread = 0.f;
        int n = 0;   // examples learned; under MIN_EX = untrained
        bool trained() const { return n >= MIN_EX; }
    };

    Slot slots[SLOTS];

    // Starts training slot s; train() and endTraining() act on it until endTraining().
    void beginTraining (int s)
    {
        slot = s;
        backup = slots[s];
        count = 0;
    }

    int training() const { return slot; }   // -1 when no slot trains

    // One example while the slot's TRAIN button is held. Examples past MAX_EX are ignored.
    void train (const float* v)
    {
        if (slot < 0 || count >= MAX_EX) return;
        for (int i = 0; i < DIM; ++i) ex[count][i] = v[i];
        ++count;
    }

    int examples() const { return count; }

    // Keeps the new sound if it has MIN_EX examples, else puts the old one back.
    void endTraining()
    {
        if (slot < 0) return;
        const int s = slot;
        slot = -1;
        if (count < MIN_EX)
        {
            slots[s] = backup;
            return;
        }
        Slot& sl = slots[s];
        for (int i = 0; i < DIM; ++i)
        {
            float m = 0.f;
            for (int e = 0; e < count; ++e) m += ex[e][i];
            sl.c[i] = m / (float) count;
        }
        float sp = 0.f;
        for (int e = 0; e < count; ++e) sp += dist (ex[e], sl.c);
        sl.spread = sp / (float) count;
        sl.n = count;
    }

    void clear (int s) { slots[s] = Slot(); }

    int classify (const float* v) const
    {
        int best = -1;
        float bestD = 0.f;
        for (int s = 0; s < SLOTS; ++s)
        {
            if (! slots[s].trained()) continue;
            const float d = dist (v, slots[s].c);
            if (best < 0 || d < bestD) { best = s; bestD = d; }
        }
        if (best < 0) return -1;
        const float radius = std::fmax (0.3f, 2.f * slots[best].spread);
        return bestD <= radius ? best : -1;
    }

    // Text for the patch JSON: "v1", then per slot n, spread and the DIM centroid values. C locale, whatever the host's.
    std::string save() const
    {
        std::ostringstream out;
        out.imbue (std::locale::classic());
        out << "v1" << std::setprecision (9);
        for (int s = 0; s < SLOTS; ++s)
        {
            out << ' ' << slots[s].n << ' ' << slots[s].spread;
            for (int i = 0; i < DIM; ++i) out << ' ' << slots[s].c[i];
        }
        return out.str();
    }

    // Strict: whole tokens only, n 0..12, spread >= 0, finite values, nothing after the last one.
    // Leaves the bank untouched and returns false on anything else.
    bool load (const std::string& text)
    {
        std::istringstream in (text);
        in.imbue (std::locale::classic());
        std::string tag;
        if (! (in >> tag) || tag != "v1") return false;
        Slot next[SLOTS];
        for (int s = 0; s < SLOTS; ++s)
        {
            if (! token (in, next[s].n) || next[s].n < 0 || next[s].n > MAX_EX) return false;
            if (! token (in, next[s].spread) || ! std::isfinite (next[s].spread) || next[s].spread < 0.f) return false;
            for (int i = 0; i < DIM; ++i)
                if (! token (in, next[s].c[i]) || ! std::isfinite (next[s].c[i])) return false;
        }
        if (in >> tag) return false;
        for (int s = 0; s < SLOTS; ++s) slots[s] = next[s];
        return true;
    }

private:
    Slot backup;
    float ex[MAX_EX][DIM] = {};
    int count = 0, slot = -1;

    // One whitespace-separated token that must parse completely as a T.
    template <typename T>
    static bool token (std::istream& in, T& value)
    {
        std::string t;
        if (! (in >> t)) return false;
        std::istringstream one (t);
        one.imbue (std::locale::classic());
        return (one >> value) && one.peek() == std::char_traits<char>::eof();
    }

    static float dist (const float* a, const float* b)
    {
        float s = 0.f;
        for (int i = 0; i < DIM; ++i) s += (a[i] - b[i]) * (a[i] - b[i]);
        return std::sqrt (s);
    }
};
