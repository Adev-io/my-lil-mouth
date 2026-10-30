// Env.h: envelope follower, rectify then an asymmetric one-pole. No Rack dependency.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <cmath>

struct Env
{
    void set (float attackMs, float releaseMs, float sampleRate)
    {
        att = 1.f - std::exp (-1000.f / (attackMs * sampleRate));
        rel = 1.f - std::exp (-1000.f / (releaseMs * sampleRate));
    }

    float process (float x)
    {
        const float a = std::fabs (x);
        level += (a > level ? att : rel) * (a - level);
        return level;
    }

    float level = 0.f;

private:
    float att = 0.004f, rel = 0.0004f;
};