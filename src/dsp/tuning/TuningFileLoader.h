#pragma once

#include "TuningTable.h"

#include <string>

/**
 * Loads AnaMark .tun files into a TuningTable.
 * Pure C++ (no JUCE). Implementation lives in TuningFileLoader.cpp.
 *
 * Exact (preferred when [Exact Tuning] is present):
 *   BaseFreq + all 128 note cents. Fail closed with a specific error if the
 *   section is incomplete or contains non-numeric values. No fallback to
 *   Functional when Exact is present but invalid.
 *
 * Functional (used only when Exact section is absent):
 *   InitEqual seeds 12-TET; Note T = "#=B %C" overrides in file order.
 *   Malformed Note / InitEqual lines are errors (not silently skipped).
 */
class TuningFileLoader
{
public:
    struct Result
    {
        bool ok = false;
        std::string error;
        TuningTable table;
    };

    /** Load from disk. Only the .tun extension is accepted. */
    static Result loadFromPath (const std::string& path);

    /** Parse AnaMark TUN file content (Exact preferred, then Functional). */
    static Result parseTun (const std::string& content);
};
