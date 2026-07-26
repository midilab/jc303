#pragma once

#include "TuningTable.h"

#include <string>

/**
 * Loads AnaMark .tun files into a TuningTable.
 * Pure C++ (no JUCE). Implementation lives in TuningFileLoader.cpp.
 *
 * Exact: BaseFreq + all 128 note cents (fail closed if incomplete).
 * Functional: InitEqual seeds 12-TET; Note T = "#=B %C" sets
 * freq[T] = freq[B] * 2^(C/1200) in file order (chaining allowed).
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
