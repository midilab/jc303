#pragma once

#include <array>
#include <cctype>
#include <cmath>
#include <sstream>
#include <string>

/**
 * Fixed 128-note frequency map (MIDI note → Hz) plus display metadata.
 *
 * DSP consumers should install getFrequencies() into a real-time-safe pitch
 * buffer (see Open303); do not read this object from the audio thread if
 * another thread may mutate it.
 */
class TuningTable
{
public:
    static constexpr int kNumNotes = 128;

    TuningTable()
    {
        setEqualTemperament (440.0);
    }

    /** Fill an array with 12-TET frequencies for the given A4 reference. */
    static void fillEqualTemperament (std::array<double, kNumNotes>& freqs, double a4Hz)
    {
        if (! (a4Hz > 0.0) || ! std::isfinite (a4Hz))
            a4Hz = 440.0;

        for (int n = 0; n < kNumNotes; ++n)
            freqs[(size_t) n] = a4Hz * std::pow (2.0, (n - 69) / 12.0);
    }

    /** True when every entry is finite and in (0, 20000] Hz. */
    static bool validateFrequencies (const std::array<double, kNumNotes>& freqs)
    {
        for (int n = 0; n < kNumNotes; ++n)
        {
            const double f = freqs[(size_t) n];
            if (! std::isfinite (f) || ! (f > 0.0) || f > 20000.0)
                return false;
        }
        return true;
    }

    static std::string frequenciesToCsv (const std::array<double, kNumNotes>& freqs)
    {
        std::ostringstream oss;
        oss.precision (12);
        for (int n = 0; n < kNumNotes; ++n)
        {
            if (n > 0)
                oss << ',';
            oss << freqs[(size_t) n];
        }
        return oss.str();
    }

    /** Parse exactly 128 comma-separated doubles. No side effects on failure.
        Each token must be a full number (optional surrounding whitespace only). */
    static bool parseFrequenciesCsv (const std::string& csv,
                                     std::array<double, kNumNotes>& out)
    {
        std::array<double, kNumNotes> parsed {};
        std::stringstream ss (csv);
        std::string token;
        int count = 0;

        while (std::getline (ss, token, ','))
        {
            if (count >= kNumNotes)
                return false;

            // trim whitespace
            size_t b = 0;
            while (b < token.size() && std::isspace ((unsigned char) token[b])) ++b;
            size_t e = token.size();
            while (e > b && std::isspace ((unsigned char) token[e - 1])) --e;
            if (b >= e)
                return false;

            try
            {
                size_t idx = 0;
                const std::string body = token.substr (b, e - b);
                const double v = std::stod (body, &idx);
                if (idx != body.size() || ! std::isfinite (v))
                    return false;
                parsed[(size_t) count] = v;
            }
            catch (...)
            {
                return false;
            }
            ++count;
        }

        if (count != kNumNotes)
            return false;

        if (! validateFrequencies (parsed))
            return false;

        out = parsed;
        return true;
    }

    /** Update display metadata without re-validating frequencies. */
    void setMetadata (const std::string& displayName, const std::string& path = {})
    {
        if (! displayName.empty())
            name = displayName;
        sourcePath = path;
    }

    void setEqualTemperament (double a4Hz)
    {
        fillEqualTemperament (frequencies, a4Hz);
        customActive = false;
        name = "12-TET";
        sourcePath.clear();
    }

    /**
     * Install a full custom map. Returns false if validation fails (table unchanged).
     */
    bool setCustom (const std::array<double, kNumNotes>& freqs,
                    const std::string& displayName,
                    const std::string& path = {})
    {
        if (! validateFrequencies (freqs))
            return false;

        frequencies = freqs;
        customActive = true;
        name = displayName.empty() ? "Custom" : displayName;
        sourcePath = path;
        return true;
    }

    double noteToHz (int noteNumber) const
    {
        if (noteNumber < 0) noteNumber = 0;
        if (noteNumber > 127) noteNumber = 127;
        return frequencies[(size_t) noteNumber];
    }

    bool isCustom() const { return customActive; }
    const std::string& getName() const { return name; }
    const std::string& getSourcePath() const { return sourcePath; }
    const std::array<double, kNumNotes>& getFrequencies() const { return frequencies; }

    std::string frequenciesToString() const
    {
        return frequenciesToCsv (frequencies);
    }

    /**
     * Restore frequencies from CSV, keeping / applying name and path in one shot.
     * Returns false if parse or validation fails (table unchanged).
     */
    bool loadFromCsv (const std::string& csv,
                      const std::string& displayName,
                      const std::string& path = {})
    {
        std::array<double, kNumNotes> parsed {};
        if (! parseFrequenciesCsv (csv, parsed))
            return false;
        return setCustom (parsed, displayName, path);
    }

private:
    std::array<double, kNumNotes> frequencies {};
    bool customActive = false;
    std::string name = "12-TET";
    std::string sourcePath;
};
