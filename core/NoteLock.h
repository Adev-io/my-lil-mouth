// NoteLock.h: sung f0 to a held semitone with hysteresis, and the glided NOTE voltage. No Rack dependency.
// The note moves only when the voice sits more than LOCK cents from it for 2 hops in a row (like Dubler's
// "pitch stickiness"; the values are our own). LOCK runs 50 to 100: 50 = plain rounding, 100 = only on reaching the
// next semitone (under 50 would do nothing more, the nearest semitone has to change too). Each voiced start after
// silence snaps at once; while unvoiced the output holds, even mid-glide.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <cmath>

struct NoteLock
{
    struct Out { int semitone; bool changed; };

    void set (float lockCents, float glideMs)
    {
        lock = lockCents;
        glideSec = glideMs / 1000.f;
    }

    // Once per hop. f0 in Hz; voiced = the module's gate decision.
    Out process (float f0, bool voiced)
    {
        Out o;
        o.semitone = held;
        o.changed = false;
        if (! voiced || ! (f0 > 0.f))
        {
            sounding = false;
            away = 0;
            return o;
        }
        const float midi = 69.f + 12.f * std::log2 (f0 / 440.f);
        const int nearest = (int) std::lround (midi);
        if (! sounding)   // voiced start: snap, no hold, no glide
        {
            sounding = true;
            away = 0;
            held = nearest;
            out = target();
            rate = 0.f;
        }
        else if (nearest != held && std::fabs (midi - (float) held) * 100.f > lock)
        {
            if (++away < 2) return o;
            away = 0;
            held = nearest;
            rate = glideSec > 0.f ? std::fabs (target() - out) / glideSec : 0.f;
            if (rate == 0.f) out = target();
        }
        else
        {
            away = 0;
            return o;
        }
        o.semitone = held;
        o.changed = true;
        return o;
    }

    float target() const { return (float) (held - 60) / 12.f; }   // V/Oct, C4 = 0 V

    // Per sample: the NOTE output, moving linearly to the target over GLIDE.
    float step (float sampleTime)
    {
        if (! sounding) return out;   // unvoiced: hold, even mid-glide
        const float t = target();
        if (rate <= 0.f || glideSec <= 0.f) { rate = 0.f; return out = t; }   // GLIDE 0 also ends a glide under way
        const float dv = rate * sampleTime;
        if (std::fabs (t - out) <= dv) { out = t; rate = 0.f; }
        else out += out < t ? dv : -dv;
        return out;
    }

    int note() const { return held; }

private:
    float lock = 40.f, glideSec = 0.f, out = 0.f, rate = 0.f;
    int held = 60, away = 0;
    bool sounding = false;
};
