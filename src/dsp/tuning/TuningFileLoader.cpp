#include "TuningFileLoader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
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

    TuningFileLoader::Result failAt (int lineNo, const std::string& msg)
    {
        return fail ("Line " + std::to_string (lineNo) + ": " + msg);
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

    bool startsWithNoteKey (const std::string& key)
    {
        return key.rfind ("Note", 0) == 0;
    }

    bool startsWithIgnoreCase (const std::string& s, const char* prefix)
    {
        const size_t n = std::char_traits<char>::length (prefix);
        if (s.size() < n)
            return false;
        for (size_t i = 0; i < n; ++i)
        {
            const auto a = (unsigned char) s[i];
            const auto b = (unsigned char) prefix[i];
            if (std::tolower (a) != std::tolower (b))
                return false;
        }
        return true;
    }

    /**
     * Functional line: Note N = "#=BASE %CENTS" (quotes optional).
     * On failure, sets detail with a short reason for the error dialog.
     */
    bool parseFunctionalNoteLine (const std::string& line,
                                  int& targetNote, int& baseNote, double& cents,
                                  std::string& detail)
    {
        auto eq = line.find ('=');
        if (eq == std::string::npos)
        {
            detail = "Note line is missing '='";
            return false;
        }

        auto left = trim (line.substr (0, eq));
        auto right = trim (line.substr (eq + 1));
        if (! startsWithNoteKey (left))
        {
            detail = "expected a Note assignment";
            return false;
        }

        if (! parseInt (trim (left.substr (4)), targetNote))
        {
            detail = "Note index is not a valid integer (\"" + left + "\")";
            return false;
        }
        if (targetNote < 0 || targetNote >= TuningTable::kNumNotes)
        {
            detail = "Note index " + std::to_string (targetNote)
                   + " is out of range (0–127)";
            return false;
        }

        right = unquote (right);
        auto hash = right.find ("#=");
        auto pct = right.find ('%');
        if (hash == std::string::npos || pct == std::string::npos || pct < hash)
        {
            detail = "Note value must look like \"#=<base> %<cents>\" (got \""
                   + right + "\")";
            return false;
        }

        auto baseStr = trim (right.substr (hash + 2, pct - (hash + 2)));
        if (! parseInt (baseStr, baseNote))
        {
            detail = "base note is not a valid integer (\"" + baseStr + "\")";
            return false;
        }
        if (baseNote < 0 || baseNote >= TuningTable::kNumNotes)
        {
            detail = "base note " + std::to_string (baseNote)
                   + " is out of range (0–127)";
            return false;
        }

        auto centsStr = trim (right.substr (pct + 1));
        // Allow a single number token; reject trailing junk / letters.
        if (! parseDouble (centsStr, cents))
        {
            // Truncate long garbage for the dialog
            std::string shown = centsStr;
            if (shown.size() > 40)
                shown = shown.substr (0, 40) + "...";
            detail = "cents value is not a number (\"" + shown + "\")";
            return false;
        }
        return true;
    }

    bool parseInitEqual (const std::string& line, int& refNote, double& refHz,
                         std::string& detail)
    {
        auto keyEnd = line.find ('=');
        if (keyEnd == std::string::npos)
        {
            detail = "InitEqual line is missing '='";
            return false;
        }
        if (trim (line.substr (0, keyEnd)) != "InitEqual")
        {
            detail = "expected InitEqual";
            return false;
        }

        auto val = trim (line.substr (keyEnd + 1));
        auto lp = val.find ('(');
        auto rp = val.find (')');
        auto comma = val.find (',');
        if (lp == std::string::npos || rp == std::string::npos || comma == std::string::npos
            || comma < lp || rp < comma)
        {
            detail = "InitEqual must look like (note, hz) (got \"" + val + "\")";
            return false;
        }

        const auto noteStr = trim (val.substr (lp + 1, comma - lp - 1));
        const auto hzStr = trim (val.substr (comma + 1, rp - comma - 1));
        if (! parseInt (noteStr, refNote))
        {
            detail = "InitEqual note is not a valid integer (\"" + noteStr + "\")";
            return false;
        }
        if (! parseDouble (hzStr, refHz))
        {
            detail = "InitEqual frequency is not a number (\"" + hzStr + "\")";
            return false;
        }
        if (refNote < 0 || refNote >= TuningTable::kNumNotes)
        {
            detail = "InitEqual note " + std::to_string (refNote)
                   + " is out of range (0–127)";
            return false;
        }
        if (! (refHz > 0.0))
        {
            detail = "InitEqual frequency must be > 0 (got " + hzStr + ")";
            return false;
        }
        return true;
    }

    std::string formatMissingNotes (const bool hasCents[TuningTable::kNumNotes], int found)
    {
        std::string msg = "Exact Tuning incomplete: got "
                        + std::to_string (found) + "/"
                        + std::to_string (TuningTable::kNumNotes)
                        + " notes";
        std::string missingList;
        int listed = 0;
        constexpr int kMaxList = 8;
        for (int n = 0; n < TuningTable::kNumNotes; ++n)
        {
            if (hasCents[n])
                continue;
            if (listed > 0)
                missingList += ", ";
            missingList += std::to_string (n);
            ++listed;
            if (listed >= kMaxList)
            {
                missingList += ", …";
                break;
            }
        }
        if (! missingList.empty())
            msg += " (missing e.g. " + missingList + ")";
        return msg;
    }

    /**
     * Parse [Exact Tuning]. Caller guarantees the section header was seen.
     * Always returns a Result (success or specific error) — never falls through.
     */
    TuningFileLoader::Result parseTunExactStrict (const std::string& content)
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
        int exactLineCount = 0;

        for (size_t i = 0; i < lines.size(); ++i)
        {
            const int lineNo = (int) i + 1;
            auto line = trim (lines[i]);
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

            ++exactLineCount;
            auto eq = line.find ('=');
            if (eq == std::string::npos)
                return failAt (lineNo,
                               "Exact Tuning entry is missing '=' (\"" + line + "\")");

            auto key = trim (line.substr (0, eq));
            auto val = trim (line.substr (eq + 1));

            if (key == "BaseFreq")
            {
                if (! parseDouble (val, baseFreq))
                    return failAt (lineNo,
                                   "BaseFreq is not a number (\"" + val + "\")");
                if (! (baseFreq > 0.0))
                    return failAt (lineNo,
                                   "BaseFreq must be > 0 (got " + val + ")");
                hasBase = true;
            }
            else if (startsWithNoteKey (key))
            {
                int index = 0;
                if (! parseInt (trim (key.substr (4)), index))
                    return failAt (lineNo,
                                   "Note index is not a valid integer (\"" + key + "\")");
                if (index < 0 || index >= TuningTable::kNumNotes)
                    return failAt (lineNo,
                                   "Note index " + std::to_string (index)
                                   + " is out of range (0–127)");

                double c = 0.0;
                if (! parseDouble (val, c))
                    return failAt (lineNo,
                                   "Note " + std::to_string (index)
                                   + " value is not a number (\"" + val + "\")");
                cents[(size_t) index] = c;
                hasCents[index] = true;
            }
            // Unknown keys inside Exact are ignored (AnaMark may add more later).
        }

        if (exactLineCount == 0)
            return fail ("[Exact Tuning] section is empty");

        if (! hasBase)
            return fail ("Exact Tuning is missing BaseFreq");

        int found = 0;
        for (bool h : hasCents)
            if (h) ++found;

        if (found != TuningTable::kNumNotes)
            return fail (formatMissingNotes (hasCents, found));

        std::array<double, TuningTable::kNumNotes> freqs {};
        for (int n = 0; n < TuningTable::kNumNotes; ++n)
            freqs[(size_t) n] = baseFreq * std::pow (2.0, cents[(size_t) n] / 1200.0);

        if (! TuningTable::validateFrequencies (freqs))
            return fail ("Exact Tuning produced invalid frequencies "
                         "(must be finite and in (0, 20000] Hz)");

        TuningFileLoader::Result r;
        r.ok = true;
        r.table.setCustom (freqs, infoName.empty() ? "TUN" : infoName);
        return r;
    }

    /**
     * Parse [Functional Tuning]. Caller guarantees the section header was seen
     * and Exact was not present (or not chosen).
     */
    TuningFileLoader::Result parseTunFunctionalStrict (const std::string& content)
    {
        const auto lines = linesOf (content);
        bool inFunc = false;
        bool inInfo = false;
        int refNote = 69;
        double refHz = 440.0;
        bool hasInit = false;
        std::string infoName;
        bool hasExplicitName = false;
        int funcLineCount = 0;

        struct NoteAssign { int target; int base; double cents; int lineNo; };
        std::vector<NoteAssign> assigns;
        assigns.reserve (128);

        for (size_t i = 0; i < lines.size(); ++i)
        {
            const int lineNo = (int) i + 1;
            auto line = trim (lines[i]);
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

            ++funcLineCount;

            // InitEqual (key match even when value is garbage)
            {
                auto eq = line.find ('=');
                if (eq != std::string::npos
                    && trim (line.substr (0, eq)) == "InitEqual")
                {
                    int tmpNote = 0;
                    double tmpHz = 0.0;
                    std::string detail;
                    if (! parseInitEqual (line, tmpNote, tmpHz, detail))
                        return failAt (lineNo, detail);
                    refNote = tmpNote;
                    refHz = tmpHz;
                    hasInit = true;
                    continue;
                }
            }

            // Any line whose left side starts with Note must fully parse
            {
                auto eq = line.find ('=');
                const auto left = (eq != std::string::npos)
                                    ? trim (line.substr (0, eq))
                                    : line;
                if (startsWithNoteKey (left) || startsWithIgnoreCase (line, "Note"))
                {
                    int target = 0, base = 0;
                    double cents = 0.0;
                    std::string detail;
                    if (! parseFunctionalNoteLine (line, target, base, cents, detail))
                        return failAt (lineNo, detail);
                    assigns.push_back ({ target, base, cents, lineNo });
                    continue;
                }
            }

            // Unknown keys inside Functional are ignored.
        }

        if (funcLineCount == 0)
            return fail ("[Functional Tuning] section is empty");

        if (! hasInit)
            return fail ("Functional Tuning is missing InitEqual = (note, hz)");

        // Seed full 12-TET from InitEqual, then apply Note overrides in order.
        std::array<double, TuningTable::kNumNotes> freqs {};
        for (int n = 0; n < TuningTable::kNumNotes; ++n)
            freqs[(size_t) n] = refHz * std::pow (2.0, (n - refNote) / 12.0);

        for (const auto& a : assigns)
        {
            const double baseHz = freqs[(size_t) a.base];
            const double f = baseHz * std::pow (2.0, a.cents / 1200.0);
            if (! std::isfinite (f) || ! (f > 0.0))
                return failAt (a.lineNo,
                               "Note " + std::to_string (a.target)
                               + " produced a non-positive frequency");
            freqs[(size_t) a.target] = f;
        }

        if (! TuningTable::validateFrequencies (freqs))
            return fail ("Functional Tuning produced invalid frequencies "
                         "(must be finite and in (0, 20000] Hz)");

        TuningFileLoader::Result r;
        r.ok = true;
        r.table.setCustom (freqs, infoName.empty() ? "TUN" : infoName);
        return r;
    }

    void scanSections (const std::string& content,
                       bool& hasExact, bool& hasFunctional)
    {
        hasExact = false;
        hasFunctional = false;
        for (const auto& raw : linesOf (content))
        {
            auto line = trim (raw);
            if (line.empty() || line[0] == ';')
                continue;
            if (line.front() != '[')
                continue;
            if (line.find ("[Exact Tuning]") != std::string::npos)
                hasExact = true;
            if (line.find ("[Functional Tuning]") != std::string::npos)
                hasFunctional = true;
        }
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
    bool hasExact = false;
    bool hasFunctional = false;
    scanSections (content, hasExact, hasFunctional);

    // If Exact is present, it is authoritative: success or specific error.
    // Do not fall back to Functional when Exact is broken (avoids silently
    // accepting a file the user deliberately corrupted in the Exact block).
    if (hasExact)
        return parseTunExactStrict (content);

    if (hasFunctional)
        return parseTunFunctionalStrict (content);

    return fail ("TUN file has no [Exact Tuning] or [Functional Tuning] section");
}
