// Oscillator model tests: header-only DSP, no JUCE.
// cmake -S tests -B build-tests && cmake --build build-tests --target oscillator_test && ctest --test-dir build-tests

#include "dfl_AlphaJunoSuperOscillator.h"
#include "dfl_PhaseshaperOscillator.h"

#include <cmath>
#include <complex>
#include <cstdio>
#include <functional>
#include <vector>

static int failures = 0;

#define CHECK(cond, ...)                                   \
    do {                                                   \
        if (! (cond)) {                                    \
            ++failures;                                    \
            std::printf ("FAIL %s:%d: ", __FILE__, __LINE__); \
            std::printf (__VA_ARGS__);                     \
            std::printf ("\n");                            \
        }                                                  \
    } while (0)

static const double kPi = 3.14159265358979323846;

// magnitude of harmonic k of a signal with exactly `period` samples per cycle
static double harmonic (const std::vector<double>& x, double period, int k)
{
    std::complex<double> acc = 0.0;
    for (size_t n = 0; n < x.size(); ++n)
        acc += x[n] * std::polar (1.0, -2.0 * kPi * k * (double) n / period);
    return std::abs (acc) / (double) x.size();
}

// With f0 = sr * P / N (P odd, N a power of two) the rendered signal repeats exactly every N samples,
// so true harmonics sit on the bins j*P and every other bin below Nyquist holds aliased energy only.
static double aliasRatioDb (const std::function<double()>& next, int N, int P)
{
    std::vector<double> x (N);
    for (int n = 0; n < 4 * N; ++n) next();   // settle
    for (int n = 0; n < N; ++n) x[n] = next();

    double harm = 0.0, alias = 0.0;
    for (int b = 1; b < N / 2; ++b)
    {
        std::complex<double> acc = 0.0;
        for (int n = 0; n < N; ++n)
            acc += x[n] * std::polar (1.0, -2.0 * kPi * b * n / N);
        const double p = std::norm (acc);
        if (b % P == 0) harm += p; else alias += p;
    }
    return 10.0 * std::log10 (alias / harm);
}

static dfl::PhaseshaperOscillator makePhaseshaper (double sr, double f, int mode, double shape)
{
    dfl::PhaseshaperOscillator osc;
    osc.setSampleRate (sr);
    osc.setFrequency (f);
    osc.calculateIncrement();
    osc.setMode (mode);
    osc.setShape (shape);
    osc.setDrift (0.0);
    osc.resetPhase();
    return osc;
}

static void testTriModAtZeroShapeIsTriangle()
{
    auto osc = makePhaseshaper (48000.0, 480.0, dfl::PhaseshaperOscillator::TRI_MOD, 0.0);
    double maxErr = 0.0;
    for (int n = 0; n < 1000; ++n)
    {
        const double p = std::fmod (n / 100.0, 1.0);
        const double tri = 2.0 * std::fabs (2.0 * p - 1.0) - 1.0;
        maxErr = std::max (maxErr, std::fabs (osc.getSample() - tri));
    }
    CHECK (maxErr < 1e-9, "shape 0 should be a plain triangle, max error %g", maxErr);
}

static double shapeForTriModAmount (double a)
{
    return (a - 0.5) / (dfl::PhaseshaperOscillator::TRI_MOD_MAX_AMOUNT - 0.5);
}

static void testTriModOddHarmonicsWithDominantThird()
{
    // Kleimola et al. 2010, Sec. 3.5: odd harmonics only, the 3rd the most prominent for aTM in [0.7, 1)
    // (at exactly 1 the 3rd and 5th tie)
    for (double a : { 0.7, 0.82, 0.95 })
    {
        const double shape = shapeForTriModAmount (a);
        auto osc = makePhaseshaper (96000.0, 100.0, dfl::PhaseshaperOscillator::TRI_MOD, shape);
        std::vector<double> x (9600);
        for (auto& s : x) s = osc.getSample();
        const double h1 = harmonic (x, 960.0, 1), h2 = harmonic (x, 960.0, 2), h3 = harmonic (x, 960.0, 3);
        const double h4 = harmonic (x, 960.0, 4), h5 = harmonic (x, 960.0, 5);
        CHECK (h2 < 1e-3 * h3 && h4 < 1e-3 * h3, "aTM %.2f: even harmonics h2=%g h4=%g h3=%g", a, h2, h4, h3);
        CHECK (h3 > h1 && h3 > h5, "aTM %.2f: 3rd not dominant (h1=%g h3=%g h5=%g)", a, h1, h3, h5);
    }
}

static void testTriModPeakSweepsLikeSync()
{
    // past aTM = 1 the strongest harmonic climbs with the amount, near 4 * aTM, as a sync sweep does
    // (half-integer amounts: at whole ones harmonics 4a - 1 and 4a + 1 tie)
    const int expected[] = { 9, 17, 21 };
    int i = 0;
    for (double a : { 2.5, 4.5, 5.5 })
    {
        const double shape = shapeForTriModAmount (a);
        auto osc = makePhaseshaper (96000.0, 100.0, dfl::PhaseshaperOscillator::TRI_MOD, shape);
        osc.setAntialias (false);
        std::vector<double> x (960);
        for (auto& s : x) s = osc.getSample();
        int peak = 1;
        for (int k = 2; k <= 60; ++k)
            if (harmonic (x, 960.0, k) > harmonic (x, 960.0, peak)) peak = k;
        CHECK (peak == expected[i], "shape %.3f: peak at h%d, expected h%d", shape, peak, expected[i]);
        ++i;
    }
}

static void testSupersawMatchesEquation28()
{
    // y = gb(sin(modm(x, m1) + modm(x, m2))), x = a1 * phase (Eq. 28), naive
    auto osc = makePhaseshaper (48000.0, 480.0, dfl::PhaseshaperOscillator::SUPERSAW, 0.5);
    osc.setAntialias (false);
    const double a1 = dfl::PhaseshaperOscillator::SUPERSAW_A1;
    const double m2 = dfl::PhaseshaperOscillator::SUPERSAW_M2;
    const double m1 = dfl::PhaseshaperOscillator::rippleForShape (0.5);
    auto modm = [] (double v, double m) { return v - m * std::floor (v / m); };
    double maxErr = 0.0;
    for (int n = 0; n < 1000; ++n)
    {
        const double x = a1 * std::fmod (n / 100.0, 1.0);
        const double y = 2.0 * std::sin (modm (x, m1) + modm (x, m2)) - 1.0;
        maxErr = std::max (maxErr, std::fabs (osc.getSample() - y));
    }
    CHECK (maxErr < 1e-9, "supersaw differs from Eq. 28, max error %g", maxErr);
}

static void testSupersawIsPeriodicAndBounded()
{
    auto osc = makePhaseshaper (48000.0, 480.0, dfl::PhaseshaperOscillator::SUPERSAW, 0.8);
    std::vector<double> x (1000);
    for (auto& s : x) s = osc.getSample();
    double peak = 0.0, periodErr = 0.0;
    for (size_t n = 0; n < x.size(); ++n)
    {
        peak = std::max (peak, std::fabs (x[n]));
        if (n >= 100) periodErr = std::max (periodErr, std::fabs (x[n] - x[n - 100]));
    }
    CHECK (peak < 1.2, "supersaw peak %g", peak);
    CHECK (periodErr < 1e-9, "supersaw not periodic at f0, error %g", periodErr);
}

static void testDriftModulatesTheShape()
{
    auto still = makePhaseshaper (48000.0, 480.0, dfl::PhaseshaperOscillator::SUPERSAW, 0.5);
    auto moving = makePhaseshaper (48000.0, 480.0, dfl::PhaseshaperOscillator::SUPERSAW, 0.5);
    moving.setDrift (1.0);
    double diff = 0.0;
    for (int n = 0; n < 48000; ++n)
        diff = std::max (diff, std::fabs (still.getSample() - moving.getSample()));
    CHECK (diff > 0.05, "drift had no audible effect (max diff %g)", diff);
}

static void testDriftLeavesTriModAlone()
{
    // sweeping aTM crosses the fundamental's null near 0.7, so drift is a ripple-saw effect only
    auto still = makePhaseshaper (48000.0, 480.0, dfl::PhaseshaperOscillator::TRI_MOD, 0.4);
    auto drifting = makePhaseshaper (48000.0, 480.0, dfl::PhaseshaperOscillator::TRI_MOD, 0.4);
    drifting.setDrift (1.0);
    double diff = 0.0;
    for (int n = 0; n < 48000; ++n)
        diff = std::max (diff, std::fabs (still.getSample() - drifting.getSample()));
    CHECK (diff == 0.0, "drift changed the triangle modulation (max diff %g)", diff);
}

// rate of the drift's shape motion: zero crossings per second of the drift signal
static double driftCrossingRate (double f)
{
    const double sr = 48000.0;
    auto drifting = makePhaseshaper (sr, f, dfl::PhaseshaperOscillator::SUPERSAW, 0.5);
    drifting.setDrift (1.0);
    const int period = (int) std::lround (sr / f);
    const int seconds = 8;
    std::vector<double> dev;   // the drift signal, once per period
    for (int n = 0, k = 0; n < seconds * (int) sr; ++n)
    {
        drifting.getSample();
        if (++k == period) { dev.push_back (drifting.getDriftValue()); k = 0; }
    }
    int crossings = 0;
    for (size_t i = 1; i < dev.size(); ++i)
        if ((dev[i - 1] < 0.0) != (dev[i] < 0.0)) ++crossings;
    return crossings / (double) seconds;
}

static void testDriftRateTracksPitch()
{
    // a detuned bank beats faster on higher notes
    const double low = driftCrossingRate (110.0), high = driftCrossingRate (440.0);
    std::printf ("drift motion: %.2f crossings/s at 110 Hz, %.2f at 440 Hz\n", low, high);
    CHECK (low > 0.5, "drift at 110 Hz is too slow (%.2f crossings/s)", low);
    CHECK (high > 2.5 * low, "drift rate does not track pitch (%.2f vs %.2f)", low, high);
}

static void testPhaseshaperAntialiasing()
{
    const int N = 4096, P = 97;   // f0 = 1136.7 Hz at 48 kHz
    const double sr = 48000.0, f = sr * P / N;
    for (int mode : { (int) dfl::PhaseshaperOscillator::TRI_MOD, (int) dfl::PhaseshaperOscillator::SUPERSAW })
    {
        const double shape = (mode == dfl::PhaseshaperOscillator::TRI_MOD) ? shapeForTriModAmount (0.85) : 0.7;
        auto naive = makePhaseshaper (sr, f, mode, shape);
        naive.setAntialias (false);
        auto blep = makePhaseshaper (sr, f, mode, shape);
        const double naiveDb = aliasRatioDb ([&] { return naive.getSample(); }, N, P);
        const double blepDb = aliasRatioDb ([&] { return blep.getSample(); }, N, P);
        std::printf ("phaseshaper mode %d alias: naive %.1f dB, polyBLEP %.1f dB\n", mode, naiveDb, blepDb);
        CHECK (blepDb < naiveDb - 10.0, "mode %d: polyBLEP only %.1f dB below naive", mode, naiveDb - blepDb);
    }
}

static void testTriModAntialiasingAtFullAmount()
{
    // aTM = 6 wraps at every half-integer level from 0.5 to 5.5, twelve per slope
    const int N = 4096, P = 23;   // f0 = 269.5 Hz at 48 kHz
    const double sr = 48000.0, f = sr * P / N;
    auto naive = makePhaseshaper (sr, f, dfl::PhaseshaperOscillator::TRI_MOD, 1.0);
    naive.setAntialias (false);
    auto blep = makePhaseshaper (sr, f, dfl::PhaseshaperOscillator::TRI_MOD, 1.0);
    const double naiveDb = aliasRatioDb ([&] { return naive.getSample(); }, N, P);
    const double blepDb = aliasRatioDb ([&] { return blep.getSample(); }, N, P);
    std::printf ("tri mod aTM 6 alias: naive %.1f dB, polyBLEP %.1f dB\n", naiveDb, blepDb);
    CHECK (blepDb < naiveDb - 4.0, "aTM 6: polyBLEP only %.1f dB below naive", naiveDb - blepDb);
}

static void testJunoSawIsBandlimited()
{
    const int N = 4096, P = 97;
    const double sr = 48000.0, f = sr * P / N;
    dfl::AlphaJunoSuperOscillator osc;
    osc.setSampleRate (sr);
    osc.setWaveform (dfl::AlphaJunoOscillator::SAW03);
    osc.setPulseWidth (40.0);
    osc.setFrequency (f);
    osc.calculateIncrement();
    osc.reset();
    const double db = aliasRatioDb ([&] { return osc.getSample(); }, N, P);
    std::printf ("juno SAW03 alias: %.1f dB\n", db);
    CHECK (db < -25.0, "Juno SAW03 alias ratio %.1f dB", db);
}

int main()
{
    testTriModAtZeroShapeIsTriangle();
    testTriModOddHarmonicsWithDominantThird();
    testTriModPeakSweepsLikeSync();
    testSupersawMatchesEquation28();
    testSupersawIsPeriodicAndBounded();
    testDriftModulatesTheShape();
    testDriftLeavesTriModAlone();
    testDriftRateTracksPitch();
    testPhaseshaperAntialiasing();
    testTriModAntialiasingAtFullAmount();
    testJunoSawIsBandlimited();

    if (failures == 0)
        std::printf ("oscillator_test: all passed\n");
    return failures == 0 ? 0 : 1;
}
