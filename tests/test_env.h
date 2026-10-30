// test_env.h: attack 5 ms and release 50 ms time constants, within 20 %.
#pragma once
#include "Env.h"
#include "check.h"
#include "signals.h"

inline void testEnv()
{
    Env e;
    e.set (5.f, 50.f, SR);
    int n = 0;
    while (e.process (1.f) < 0.632f) ++n;
    check (n / SR * 1000.f >= 4.f && n / SR * 1000.f <= 6.f, "env attack 63 % at 5 ms", n / SR * 1000.f);
    for (int i = 0; i < 4800; ++i) e.process (1.f);
    n = 0;
    while (e.process (0.f) > 0.368f) ++n;
    check (n / SR * 1000.f >= 40.f && n / SR * 1000.f <= 60.f, "env release 37 % at 50 ms", n / SR * 1000.f);
}