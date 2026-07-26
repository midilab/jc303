#include "TuningFileLoader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>
#include <vector>

namespace
{
    TuningFileLoader::Result fail (const std::string& msg)
    {
        TuningFileLoader::Result r;
        r.ok = false;
        r.error = msg;
        return r;
    }

    std::string extensionOf (const std::string& path)
    {
        auto pos = path.find_last_of ('.');
        if (pos == std::string::npos)
            return {};
        std::string ext = path.substr (pos + 1);
        std::transform (ext.begin(), ext.end(), ext.begin(),
                        [] (unsigned char c) { return (char) std::tolower (c); });
        return ext;
    }

    std::string stemOf (const std::string& path)
    {
        auto slash = path.find_last_of ("/\\");
        std::string file = (slash == std::string::npos) ? path : path.substr (slash + 1);
        auto dot = file.find_last_of ('.');
        if (dot != std::string::npos)
            file = file.substr (0, dot);
        return file;
    }

    std::string trim (const std::string& s)
    {
        size_t b = 0;
        while (b < s.size() && std::isspace ((unsigned char) s[b])) ++b;
        size_t e = s.size();
        while (e > b && std::isspace ((unsigned char) s[e - 1])) --e;
        return s.substr (b, e - b);
    }

    std::vector<std::string> linesOf (const std::string& content)
    {
        std::vector<std::string> lines;
        std::stringstream ss (content);
        std::string line;
        while (std::getline (ss, line))
        {
            if (! line.empty() && line.back() == '\r')
                line.pop_back();
            lines.push_back (line);
        }
        return lines;
    }

    bool parseDouble (const std::string& s, double& out)
    {
        try
        {
            size_t idx = 0;
            const auto body = trim (s);
            if (body.empty())
                return false;
            const double v = std::stod (body, &idx);
            if (idx != body.size())
                return false;
            out = v;
            return std::isfinite (v);
        }
        catch (...)
        {
            return false;
        }
    }

    bool parseInt (const std::string& s, int& out)
    {
        try
        {
            size_t idx = 0;
            const auto body = trim (s);
            if (body.empty())
                return false;
            const int v = std::stoi (body, &idx);
            if (idx != body.size())
                return false;
            out = v;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    /** Strip optional surrounding quotes from a value. */
    std::string unquote (std::string val)
    {
        val = trim (val);
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
            val = val.substr (1, val.size() - 2);
        return val;
    }

    /** Read display name from [Info]: prefer Name, fall back to Filename. */
    void maybeReadInfoKey (const std::string& key, const std::string& val,
                           std::string& infoName, bool& hasExplicitName)
    {
        const auto v = unquote (val);
        if (v.empty())
            return;

        if (key == "Name")
        {
            infoName = v;
            hasExplicitName = true;
        }
        else if (key == "Filename" && ! hasExplicitName)
        {
            infoName = v;
        }
    }

    /**
     * Functional line: Note N = "#=BASE %CENTS" (quotes optional).
     * Returns true on match; fills target note, base note, and cents.
     */
    bool parseFunctionalNoteLine (const std::string& line,
                                  int& targetNote, int& baseNote, double& cents)
    {
        if (line.size() < 8 || line.rfind ("Note", 0) != 0)
            return false;

        auto eq = line.find ('=');
        if (eq == std::string::npos)
            return false;

        auto left = trim (line.substr (0, eq));
        auto right = trim (line.substr (eq + 1));
        if (left.rfind ("Note", 0) != 0)
            return false;

        if (! parseInt (trim (left.substr (4)), targetNote))
            return false;

        right = unquote (right);
        // #=69 %-1200.0
        auto hash = right.find ("#=");
        auto pct = right.find ('%');
        if (hash == std::string::npos || pct == std::string::npos || pct < hash)
            return false;

        auto baseStr = trim (right.substr (hash + 2, pct - (hash + 2)));
        if (! parseInt (baseStr, baseNote))
            return false;
        if (baseNote < 0 || baseNote >= TuningTable::kNumNotes)
            return false;

        auto centsStr = trim (right.substr (pct + 1));
        // strip trailing junk after the number
        size_t end = 0;
        while (end < centsStr.size()
               && (std::isdigit ((unsigned char) centsStr[end])
                   || centsStr[end] == '+' || centsStr[end] == '-'
                   || centsStr[end] == '.' || centsStr[end] == 'e' || centsStr[end] == 'E'))
            ++end;
        centsStr = centsStr.substr (0, end);
        return parseDouble (centsStr, cents);
    }

    bool parseInitEqual (const std::string& line, int& refNote, double& refHz)
    {
        // InitEqual = (69, 528)
        auto keyEnd = line.find ('=');
        if (keyEnd == std::string::npos)
            return false;
        if (trim (line.substr (0, keyEnd)) != "InitEqual")
            return false;

        auto val = trim (line.substr (keyEnd + 1));
        auto lp = val.find ('(');
        auto rp = val.find (')');
        auto comma = val.find (',');
        if (lp == std::string::npos || rp == std::string::npos || comma == std::string::npos
            || comma < lp || rp < comma)
            return false;

        return parseInt (trim (val.substr (lp + 1, comma - lp - 1)), refNote)
            && parseDouble (trim (val.substr (comma + 1, rp - comma - 1)), refHz);
    }

    std::optional<TuningFileLoader::Result> parseTunExact (const std::string& content)
    {
        const auto lines = linesOf (content);
        bool inExact = false;
        bool inInfo = false;
        double baseFreq = 0.0;
        bool hasBase = false;
        std::array<double, TuningTable::kNumNotes> cents {};
        bool hasCents[TuningTable::kNumNotes] = {};
        std::string infoName;
        bool hasExplicitName = false;

        for (const auto& raw : lines)
        {
            auto line = trim (raw);
            if (line.empty() || line[0] == ';')
                continue;

            if (line.front() == '[')
            {
                inExact = (line.find ("[Exact Tuning]") != std::string::npos);
                inInfo = (line.find ("[Info]") != std::string::npos);
                continue;
            }

            if (inInfo)
            {
                auto eq = line.find ('=');
                if (eq != std::string::npos)
                    maybeReadInfoKey (trim (line.substr (0, eq)),
                                      trim (line.substr (eq + 1)),
                                      infoName, hasExplicitName);
                continue;
            }

            if (! inExact)
                continue;

            auto eq = line.find ('=');
            if (eq == std::string::npos)
                continue;

            auto key = trim (line.substr (0, eq));
            auto val = trim (line.substr (eq + 1));

            if (key == "BaseFreq")
            {
                if (! parseDouble (val, baseFreq) || ! (baseFreq > 0.0))
                    return std::nullopt;
                hasBase = true;
            }
            else if (key.rfind ("Note", 0) == 0)
            {
                int index = 0;
                if (! parseInt (trim (key.substr (4)), index))
                    continue;
                if (index < 0 || index >= TuningTable::kNumNotes)
                    continue;

                double c = 0.0;
                if (! parseDouble (val, c))
                    return std::nullopt; // malformed numeric → reject section
                cents[(size_t) index] = c;
                hasCents[index] = true;
            }
        }

        if (! hasBase)
            return std::nullopt;

        int found = 0;
        for (bool h : hasCents)
            if (h) ++found;

        // Fail closed: Exact section must define every MIDI note
        if (found != TuningTable::kNumNotes)
            return std::nullopt;

        std::array<double, TuningTable::kNumNotes> freqs {};
        for (int n = 0; n < TuningTable::kNumNotes; ++n)
            freqs[(size_t) n] = baseFreq * std::pow (2.0, cents[(size_t) n] / 1200.0);

        if (! TuningTable::validateFrequencies (freqs))
            return std::nullopt;

        TuningFileLoader::Result r;
        r.ok = true;
        r.table.setCustom (freqs, infoName.empty() ? "TUN" : infoName);
        return r;
    }

    /**
     * AnaMark Functional Tuning:
     *  1. InitEqual = (refNote, refHz) seeds a full 12-TET grid.
     *  2. Note T = "#=B %C" sets freq[T] = freq[B] * 2^(C/1200), applied in file order
     *     so later lines can chain off earlier overrides.
     *
     * InitEqual alone yields a complete map; Note lines are optional overrides.
     */
    std::optional<TuningFileLoader::Result> parseTunFunctional (const std::string& content)
    {
        const auto lines = linesOf (content);
        bool inFunc = false;
        bool inInfo = false;
        int refNote = 69;
        double refHz = 440.0;
        bool hasInit = false;
        std::string infoName;
        bool hasExplicitName = false;

        // Collect functional note assignments in file order (base may chain).
        struct NoteAssign { int target; int base; double cents; };
        std::vector<NoteAssign> assigns;
        assigns.reserve (128);

        for (const auto& raw : lines)
        {
            auto line = trim (raw);
            if (line.empty() || line[0] == ';')
                continue;

            if (line.front() == '[')
            {
                inFunc = (line.find ("[Functional Tuning]") != std::string::npos);
                inInfo = (line.find ("[Info]") != std::string::npos);
                continue;
            }

            if (inInfo)
            {
                auto eq = line.find ('=');
                if (eq != std::string::npos)
                    maybeReadInfoKey (trim (line.substr (0, eq)),
                                      trim (line.substr (eq + 1)),
                                      infoName, hasExplicitName);
                continue;
            }

            if (! inFunc)
                continue;

            int tmpNote = 0;
            double tmpHz = 0.0;
            if (parseInitEqual (line, tmpNote, tmpHz))
            {
                if (tmpNote < 0 || tmpNote >= TuningTable::kNumNotes || ! (tmpHz > 0.0))
                    return std::nullopt;
                refNote = tmpNote;
                refHz = tmpHz;
                hasInit = true;
                continue;
            }

            int target = 0, base = 0;
            double cents = 0.0;
            if (parseFunctionalNoteLine (line, target, base, cents))
            {
                if (target < 0 || target >= TuningTable::kNumNotes)
                    continue;
                assigns.push_back ({ target, base, cents });
            }
        }

        if (! hasInit)
            return std::nullopt;

        // Seed full 12-TET from InitEqual, then apply Note overrides in order.
        std::array<double, TuningTable::kNumNotes> freqs {};
        for (int n = 0; n < TuningTable::kNumNotes; ++n)
            freqs[(size_t) n] = refHz * std::pow (2.0, (n - refNote) / 12.0);

        for (const auto& a : assigns)
        {
            const double baseHz = freqs[(size_t) a.base];
            const double f = baseHz * std::pow (2.0, a.cents / 1200.0);
            if (! std::isfinite (f) || ! (f > 0.0))
                return std::nullopt;
            freqs[(size_t) a.target] = f;
        }

        if (! TuningTable::validateFrequencies (freqs))
            return std::nullopt;

        TuningFileLoader::Result r;
        r.ok = true;
        r.table.setCustom (freqs, infoName.empty() ? "TUN" : infoName);
        return r;
    }
} // namespace

TuningFileLoader::Result TuningFileLoader::loadFromPath (const std::string& path)
{
    if (extensionOf (path) != "tun")
        return fail ("Unsupported tuning format (only .tun is accepted)");

    std::ifstream in (path, std::ios::binary);
    if (! in)
        return fail ("Could not open file: " + path);

    std::string content ((std::istreambuf_iterator<char> (in)),
                         std::istreambuf_iterator<char>());

    Result result = parseTun (content);
    if (! result.ok)
        return result;

    // Stamp source path; keep Info Name when present, else file stem
    const auto stem = stemOf (path);
    std::string displayName = result.table.getName();
    if (displayName.empty() || displayName == "Custom" || displayName == "TUN")
        displayName = stem.empty() ? "Custom" : stem;

    result.table.setMetadata (displayName, path);
    return result;
}

TuningFileLoader::Result TuningFileLoader::parseTun (const std::string& content)
{
    if (auto exact = parseTunExact (content))
        return *exact;

    if (auto functional = parseTunFunctional (content))
        return *functional;

    return fail ("TUN file has no usable [Exact Tuning] or [Functional Tuning] "
                 "section (Exact needs BaseFreq + all 128 notes; "
                 "Functional needs InitEqual)");
}
