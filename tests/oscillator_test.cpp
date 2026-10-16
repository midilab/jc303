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

static void testTriModOddHarmonicsWithDominantThird()
{
    // Kleimola et al. 2010, Sec. 3.5: odd harmonics only, the 3rd the most prominent for aTM in [0.7, 1]
    // (aTM 0.7, 0.82, 0.95; at exactly 1 the 3rd and 5th tie)
    for (double shape : { 0.4, 0.64, 0.9 })
    {
        auto osc = makePhaseshaper (96000.0, 100.0, dfl::PhaseshaperOscillator::TRI_MOD, shape);
        std::vector<double> x (9600);
        for (auto& s : x) s = osc.getSample();
        const double h1 = harmonic (x, 960.0, 1), h2 = harmonic (x, 960.0, 2), h3 = harmonic (x, 960.0, 3);
        const double h4 = harmonic (x, 960.0, 4), h5 = harmonic (x, 960.0, 5);
        CHECK (h2 < 1e-3 * h3 && h4 < 1e-3 * h3, "shape %.2f: even harmonics h2=%g h4=%g h3=%g", shape, h2, h4, h3);
        CHECK (h3 > h1 && h3 > h5, "shape %.2f: 3rd not dominant (h1=%g h3=%g h5=%g)", shape, h1, h3, h5);
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

static void testPhaseshaperAntialiasing()
{
    const int N = 4096, P = 97;   // f0 = 1136.7 Hz at 48 kHz
    const double sr = 48000.0, f = sr * P / N;
    for (int mode : { (int) dfl::PhaseshaperOscillator::TRI_MOD, (int) dfl::PhaseshaperOscillator::SUPERSAW })
    {
        auto naive = makePhaseshaper (sr, f, mode, 0.7);
        naive.setAntialias (false);
        auto blep = makePhaseshaper (sr, f, mode, 0.7);
        const double naiveDb = aliasRatioDb ([&] { return naive.getSample(); }, N, P);
        const double blepDb = aliasRatioDb ([&] { return blep.getSample(); }, N, P);
        std::printf ("phaseshaper mode %d alias: naive %.1f dB, polyBLEP %.1f dB\n", mode, naiveDb, blepDb);
        CHECK (blepDb < naiveDb - 10.0, "mode %d: polyBLEP only %.1f dB below naive", mode, naiveDb - blepDb);
    }
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
    testSupersawMatchesEquation28();
    testSupersawIsPeriodicAndBounded();
    testDriftModulatesTheShape();
    testPhaseshaperAntialiasing();
    testJunoSawIsBandlimited();

    if (failures == 0)
        std::printf ("oscillator_test: all passed\n");
    return failures == 0 ? 0 : 1;
}
