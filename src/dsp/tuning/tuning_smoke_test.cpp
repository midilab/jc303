// Standalone smoke test for AnaMark .tun parsing + TuningTable (no JUCE).
//
// Build (from this directory):
//   cl /EHsc /std:c++17 tuning_smoke_test.cpp TuningFileLoader.cpp
//   g++ -std=c++17 tuning_smoke_test.cpp TuningFileLoader.cpp -o tuning_smoke_test
//
// Optional arg: path to a real .tun file (extra load check).

#include "TuningFileLoader.h"

#include <cmath>
#include <iostream>
#include <string>

static int failures = 0;

static void expectNear (const char* label, double got, double expected, double tolCents = 0.5)
{
    if (! (got > 0.0) || ! (expected > 0.0))
    {
        std::cerr << "FAIL " << label << ": non-positive frequency\n";
        ++failures;
        return;
    }

    const double cents = 1200.0 * std::log2 (got / expected);
    if (std::abs (cents) > tolCents)
    {
        std::cerr << "FAIL " << label << ": got " << got << " Hz, expected " << expected
                  << " Hz (err " << cents << " c)\n";
        ++failures;
    }
    else
    {
        std::cout << "OK   " << label << ": " << got << " Hz\n";
    }
}

static void expectTrue (const char* label, bool cond)
{
    if (! cond)
    {
        std::cerr << "FAIL " << label << "\n";
        ++failures;
    }
    else
    {
        std::cout << "OK   " << label << "\n";
    }
}

// Minimal complete Exact map: BaseFreq * 2^(cents/1200); all notes 0 cents → unison grid.
static std::string makeExactAllZero (double baseFreq)
{
    std::string s = "[Info]\nName = \"Exact Zero\"\n\n[Exact Tuning]\nBaseFreq = "
                  + std::to_string (baseFreq) + "\n";
    for (int n = 0; n < TuningTable::kNumNotes; ++n)
        s += "Note" + std::to_string (n) + " = 0\n";
    return s;
}

static std::string makeFunctionalInitOnly (int refNote, double refHz)
{
    return std::string ("[Info]\nName = \"Func Init\"\n\n[Functional Tuning]\n")
         + "InitEqual = (" + std::to_string (refNote) + ", " + std::to_string (refHz) + ")\n";
}

static std::string makeFunctionalWithOverride()
{
    // InitEqual A4=440, then raise note 76 (E) by pure fifth from note 69:
    // freq[76] = freq[69] * 2^(701.955.../1200) ≈ 440 * 1.5
    return R"TUN(
[Info]
Name = "Func Fifth"

[Functional Tuning]
InitEqual = (69, 440)
Note 76 = "#=69 %701.9550008654"
)TUN";
}

static std::string makeFunctionalChainedBase()
{
    // Chain: set 60 from 69, then 67 from 60 (not from InitEqual base).
    // 60 = 440 * 2^(-900/1200) = 440 / 2^0.75
    // 67 = freq[60] * 2^(700/1200)
    return R"TUN(
[Info]
Name = "Func Chain"

[Functional Tuning]
InitEqual = (69, 440)
Note 60 = "#=69 %-900"
Note 67 = "#=60 %700"
)TUN";
}

int main (int argc, char** argv)
{
    // --- Exact: complete map with all cents = 0 → every note = BaseFreq ---
    {
        const double base = 8.1757989156;
        auto r = TuningFileLoader::parseTun (makeExactAllZero (base));
        expectTrue ("exact complete ok", r.ok);
        if (r.ok)
        {
            expectTrue ("exact custom", r.table.isCustom());
            expectNear ("exact note 0", r.table.noteToHz (0), base, 0.1);
            // Zero cents on every note → unison grid (not 12-TET)
            expectNear ("exact note 69 unison", r.table.noteToHz (69), base, 0.1);
        }
    }

    // --- Exact: BaseFreq + cents so note 69 is 440 Hz ---
    {
        // BaseFreq at MIDI 0; note 69 cents = 6900 → 440 Hz when base is C-1 @ A440
        const double base = 440.0 / std::pow (2.0, 69.0 / 12.0);
        std::string s = "[Exact Tuning]\nBaseFreq = " + std::to_string (base) + "\n";
        for (int n = 0; n < TuningTable::kNumNotes; ++n)
            s += "Note" + std::to_string (n) + " = " + std::to_string (n * 100.0) + "\n";
        auto r = TuningFileLoader::parseTun (s);
        expectTrue ("exact ET-like ok", r.ok);
        if (r.ok)
            expectNear ("exact note 69 = 440", r.table.noteToHz (69), 440.0, 0.05);
    }

    // --- Exact: incomplete rejected ---
    {
        auto r = TuningFileLoader::parseTun ("[Exact Tuning]\nBaseFreq = 8.1758\nNote0 = 0\n");
        expectTrue ("reject incomplete exact", ! r.ok);
    }

    // --- Functional: InitEqual alone fills full ET grid ---
    {
        auto r = TuningFileLoader::parseTun (makeFunctionalInitOnly (69, 528.0));
        expectTrue ("functional init-only ok", r.ok);
        if (r.ok)
        {
            expectNear ("func init A4", r.table.noteToHz (69), 528.0, 0.01);
            expectNear ("func init ET fifth", r.table.noteToHz (76),
                        528.0 * std::pow (2.0, 7.0 / 12.0), 0.05);
        }
    }

    // --- Functional: #= base relative override ---
    {
        auto r = TuningFileLoader::parseTun (makeFunctionalWithOverride());
        expectTrue ("functional override ok", r.ok);
        if (r.ok)
        {
            expectNear ("func A4 stays 440", r.table.noteToHz (69), 440.0, 0.01);
            expectNear ("func pure fifth on 76", r.table.noteToHz (76), 440.0 * 1.5, 0.5);
        }
    }

    // --- Functional: chained #= base (second note references first override) ---
    {
        auto r = TuningFileLoader::parseTun (makeFunctionalChainedBase());
        expectTrue ("functional chain ok", r.ok);
        if (r.ok)
        {
            const double n60 = 440.0 * std::pow (2.0, -900.0 / 1200.0);
            const double n67 = n60 * std::pow (2.0, 700.0 / 1200.0);
            expectNear ("func chain note 60", r.table.noteToHz (60), n60, 0.2);
            expectNear ("func chain note 67", r.table.noteToHz (67), n67, 0.2);
        }
    }

    // --- Functional: missing InitEqual rejected ---
    {
        auto r = TuningFileLoader::parseTun (
            "[Functional Tuning]\nNote 69 = \"#=69 %0\"\n");
        expectTrue ("reject functional without InitEqual", ! r.ok);
    }

    // --- Extension / format reject ---
    {
        auto r = TuningFileLoader::loadFromPath ("dummy.scl");
        expectTrue ("reject .scl", ! r.ok);
    }

    // --- TuningTable ET + validation ---
    {
        TuningTable et;
        et.setEqualTemperament (440.0);
        expectTrue ("ET not custom", ! et.isCustom());
        expectNear ("ET A4", et.noteToHz (69), 440.0, 0.01);
        expectNear ("ET C4", et.noteToHz (60), 261.625565, 0.05);
    }

    {
        std::array<double, TuningTable::kNumNotes> bad {};
        TuningTable::fillEqualTemperament (bad, 440.0);
        bad[10] = -1.0;
        TuningTable t;
        expectTrue ("reject invalid custom freqs", ! t.setCustom (bad, "bad"));
        expectTrue ("table unchanged after reject", ! t.isCustom());
    }

    // --- CSV round-trip + reject trailing junk ---
    {
        TuningTable src;
        std::array<double, TuningTable::kNumNotes> freqs {};
        TuningTable::fillEqualTemperament (freqs, 432.0);
        expectTrue ("set custom for csv", src.setCustom (freqs, "csv-test"));

        TuningTable restored;
        expectTrue ("csv round-trip",
                    restored.loadFromCsv (src.frequenciesToString(), "csv-test"));
        expectNear ("csv A4", restored.noteToHz (69), 432.0, 0.01);

        std::string junk = src.frequenciesToString();
        // corrupt one token
        auto pos = junk.find (',');
        if (pos != std::string::npos)
            junk.insert (pos, "x");
        TuningTable badCsv;
        expectTrue ("csv reject junk token",
                    ! badCsv.loadFromCsv (junk, "bad"));
    }

    // --- Optional real file ---
    if (argc > 1)
    {
        auto r = TuningFileLoader::loadFromPath (argv[1]);
        expectTrue ("optional file load", r.ok);
        if (r.ok)
        {
            std::cout << "Loaded: " << r.table.getName()
                      << " path=" << r.table.getSourcePath() << "\n";
            expectTrue ("optional custom", r.table.isCustom());
        }
    }

    if (failures)
    {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "All smoke checks passed.\n";
    return 0;
}
