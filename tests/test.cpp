// test.cpp: checks the core with plain asserts. Run: make test
// Author: Ari Klopfer / A_Development. MIT
#include "check.h"
#include "test_env.h"
#include "test_formant.h"
#include "test_notelock.h"
#include "test_onset.h"
#include "test_trigbank.h"
#include "test_yin.h"
#include "FaceTap.h"

int main()
{
    testYin();
    testNoteLock();
    testEnv();
    testOnset();
    testTrigBank();
    testFormant();
    std::printf ("%s: %d failure(s)\n", failures ? "FAILED" : "passed", failures);
    return failures ? 1 : 0;
}
