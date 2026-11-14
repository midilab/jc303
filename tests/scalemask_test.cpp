// Standalone: c++ -std=c++17 -I src/sequencer tests/scalemask_test.cpp && ./a.out
#include "ScaleMask.h"
#include <cstdio>
#include <cstring>

static int failures = 0;

static void check (bool ok, const char* what)
{
    if (! ok)
    {
        std::printf ("FAIL: %s\n", what);
        ++failures;
    }
}

static int indexOf (const char* name)
{
    for (int i = 0; i < scalemask::kNumScaleChoices; ++i)
        if (std::strcmp (scalemask::kScaleNames[i], name) == 0)
            return i;
    return -1;
}

int main()
{
    using namespace scalemask;

    // Melodic minor modes (Aciduino harmonizer parity)
    struct { const char* name; uint16_t mask; } modes[] =
    {
        { "Dorian b2",      bits ({ 0, 1, 3, 5, 7, 9, 10 }) },
        { "Lydian Aug",     bits ({ 0, 2, 4, 6, 8, 9, 11 }) },
        { "Lydian Dom",     bits ({ 0, 2, 4, 6, 7, 9, 10 }) },
        { "Mixolydian b6",  bits ({ 0, 2, 4, 5, 7, 8, 10 }) },
        { "Locrian n2",     bits ({ 0, 2, 3, 5, 6, 8, 10 }) },
        { "Altered",        bits ({ 0, 1, 3, 4, 6, 8, 10 }) },
    };

    for (const auto& m : modes)
    {
        const int i = indexOf (m.name);
        check (i > 0, m.name);
        if (i > 0)
            check (kScaleMasks[i] == m.mask, m.name);
    }

    // Existing indices stay put so saved seqScale values keep their meaning
    check (indexOf ("Chromatic") == 0 && indexOf ("Whole Tone") == 13, "existing order");

    // Every non-chromatic scale contains its root
    for (int i = 1; i < kNumScaleChoices; ++i)
        check ((kScaleMasks[i] & 1) != 0, kScaleNames[i]);

    std::printf (failures ? "%d failure(s)\n" : "ok\n", failures);
    return failures != 0;
}
