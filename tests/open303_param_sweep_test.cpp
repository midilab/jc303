// Parameter-sweep regression test for the realtime path.
//
// The audio thread re-applies the whole cheap-parameter battery
// (JC303::updateOpen303Parameters + setDevilMod) at the top of every block while a knob is
// dragged. This test sweeps every one of those setters every block for thousands of blocks and
// asserts the render stays finite. It also feeds NaN into the setters and into the wavetable
// directly: those used to floor(floorInt(NaN))->INT_MIN and index the table out of bounds,
// crashing the audio thread (rosic_MipMappedWaveTable.h getValueLinear). The guards make them
// a glitch at worst, never a crash.

#include "rosic_Open303.h"

#include <cmath>
#include <cstdio>
#include <limits>

using namespace rosic;

static int failures = 0;

#define CHECK(cond, ...)                                                       \
    do { if (! (cond)) { std::printf ("FAIL %s:%d: ", __FILE__, __LINE__);     \
                          std::printf (__VA_ARGS__); std::printf ("\n");       \
                          ++failures; } } while (0)

// deterministic PRNG in [0, 1)
static unsigned long rng = 0x12345678;
static double frand()  { rng = rng * 6364136223846793005ull + 1442695040888963407ull;
                         return (double)(rng >> 44) * (1.0 / 0x100000); }
static double frand (double lo, double hi) { return lo + frand() * (hi - lo); }

// the exact knob -> engineer mappings from JC303.cpp (unitAttributes / setParameter)
static double decayMsRun (double v)   { return v * 2970.0 + 30.0; }
static double attackMs (double v)     { return 0.3 * std::pow (3000.0 / 0.3, v); }
static double slideMs (double v)      { return v * 358.0 + 2.0; }
static double lfoRateHz (double v)    { return 0.1 * std::pow (200.0, v); }
static double feedbackHpfHz (double v){ return 350.0 * std::pow (100.0 / 350.0, v); }
static double accentSweepMs (double v){ return 15.0 * std::pow (2000.0 / 15.0, v); }
static double bipolar (double v)      { const double a = std::fabs (v);
                                        return a < 0.05 ? 0.0 : std::copysign ((a - 0.05) / 0.95, v); }
static double tuningCents (double v)  { return 700.0 * bipolar (v); }

static void applyBattery (rosic::Open303& o)
{
    o.setWaveform (frand());
    o.setTuning (440.0 * std::pow (2.0, tuningCents (frand(-1,1)) / 1200.0));
    o.setCutoff (314.0 * std::pow (2394.0 / 314.0, frand()));
    o.setResonance (frand() * 100.0);
    o.setEnvMod (frand() * 100.0);
    o.setVolume (frand() * -60.0);
    o.setReverseGate (frand());
    o.setAmpDecay (decayMsRun (frand()));
    o.setAccentDecay (decayMsRun (frand()));
    o.setAccentSweep (accentSweepMs (frand()));
    o.setFeedbackHighpass (feedbackHpfHz (frand()));
    o.setNormalAttack (attackMs (frand()));
    o.setSlideTime (slideMs (frand()));
    o.setTanhShaperDrive (25.0 + frand() * 55.0);
    o.setPulseSag (rosic::Open303::sagForSquareDrive (frand()));
    o.setFilterType ((dfl::FilterType)(int)(frand() * dfl::NUM_FILTER_TYPES));
    o.setSquareModel ((SquareModel)(int)(frand() * NUM_SQUARE_MODELS));
    o.setFilterDrive (frand() * 16.0);
    o.setFilterMode (frand());
    o.setPassbandCompensation (frand());
    o.setFilterFmDepth (frand());
    o.setResonanceTrim (0.85 + frand() * 0.30);
    o.setFilterHiFi (frand() > 0.5);
    o.setFilterTension (bipolar (frand(-1,1)));
    o.setFilterBias (frand());
    o.setFilterRail (frand());
    o.setSubOscGain (frand());
    o.setSubOscBlend (frand());
    o.setOscModel ((OscModel)(int)(frand() * NUM_OSC_MODELS));
    o.setJunoWaveform ((int)(frand() * 5));
    o.setPhaseshaperMode ((int)(frand() * 3));
    o.setSuperDetune (frand());
    o.setSuperMix (frand());
    o.setPhaseshaperDrift (frand());
    o.setPhaseshaperSplit (frand());
    o.setLfoWaveform ((int)(frand() * 6));
    o.setLfoRate (lfoRateHz (frand()));
    o.setLfoDepth (frand());
    o.setLfoDestination ((int)(frand() * 14));
    o.setLfoPhase (frand());
    o.setLfoKeySync (frand() > 0.5);
    o.setLfoContour (frand(-1,1));
    o.setLfoOn (true);
    o.setModSlot (1, (int)(frand() * 8), (int)(frand() * 14), frand(-1,1));
    o.setModSlot (2, (int)(frand() * 8), (int)(frand() * 14), frand(-1,1));
    o.setModSlot (3, (int)(frand() * 8), (int)(frand() * 14), frand(-1,1));
    o.setCircuitPulseShape (frand() > 0.5);
}

// a knob dragged while the sequencer plays: every block re-applies the battery, fires the
// noteOn/noteOff protocol from renderMidi (reversed/gated notes included) and renders.
static void testSweepStaysFinite()
{
    rosic::Open303 o;
    o.setSampleRate (48000.0);

    int note = 36;
    bool reversed = false;
    int bad = 0;
    const int totalBlocks = 500;
    for (int b = 0; b < totalBlocks; ++b)
    {
        applyBattery (o);
        o.setReverseTempo (frand (60.0, 180.0));
        if (b % 40 == 0)
        {
            reversed = (b / 40) % 2 == 1;
            o.noteOn (note, 100, 0.0, false, false, reversed, 16384.0);
            note = 36 + (note - 35) % 20;
        }
        if (b % 40 == 24)
            o.noteOn (note, 0, 0.0);

        double block[rosic::Open303::maxBlockSize];
        for (int s = 0; s < 512; s += rosic::Open303::maxBlockSize)
        {
            const int n = 512 - s < rosic::Open303::maxBlockSize ? 512 - s : rosic::Open303::maxBlockSize;
            o.renderBlock (block, n);
            for (int i = 0; i < n; ++i)
                if (! std::isfinite (block[i]))
                {
                    ++bad;
                    break;
                }
        }
        if (bad > 0)
            break;
    }
    CHECK (bad == 0, "%d non-finite sample(s) in a %d-block full-parameter sweep", bad, totalBlocks);
}

// the previous crash: a NaN pixel width fed shapedPulse -> floorInt(NaN) -> INT_MIN table read.
// The wavetable guard must clamp those indices so the lookups stay in bounds and stay finite.
static void testWavetableIndexGuard()
{
    MipMappedWaveTable wt;
    wt.setWaveform (MipMappedWaveTable::SAW303);

    const double fromMin   = wt.getValueLinear (std::numeric_limits<int>::min(), 0.5, 4);
    const double fromHuge  = wt.getValueLinear (999999, 0.5, 4);
    CHECK (std::isfinite (fromMin),  "getValueLinear(INT_MIN) returned %g", fromMin);
    CHECK (std::isfinite (fromHuge), "getValueLinear(999999) returned %g", fromHuge);

    // the old table-index clamp also left tableIndex == numTables out of bounds; it must clamp now
    const double badTable = wt.getValueLinear (100, 0.5, 12);
    CHECK (std::isfinite (badTable), "getValueLinear(.., tableIndex == numTables) returned %g", badTable);

    // a NaN phase must not take down the process (returns NaN audio, never a crash)
    (void) wt.getValueLinear (std::numeric_limits<double>::quiet_NaN(), 4);
}

// feed NaN through every setter, then render: before the guards this crashed the process
// (the test binary dying fails CI); now it must at worst produce non-finite audio.
static void testNanSettersDoNotCrash()
{
    rosic::Open303 o;
    o.setSampleRate (48000.0);
    const double nan = std::numeric_limits<double>::quiet_NaN();

    o.setWaveform (nan); o.setTuning (nan); o.setCutoff (nan); o.setResonance (nan);
    o.setEnvMod (nan); o.setDecay (nan); o.setAccent (nan); o.setVolume (nan);
    o.setReverseGate (nan); o.setSlideTime (nan); o.setNormalAttack (nan);
    o.setFeedbackHighpass (nan); o.setAmpDecay (nan); o.setAccentDecay (nan);
    o.setAccentSweep (nan); o.setFilterDrive (nan); o.setFilterMode (nan);
    o.setPassbandCompensation (nan); o.setFilterFmDepth (nan); o.setFilterBias (nan);
    o.setFilterRail (nan); o.setLfoRate (nan); o.setLfoDepth (nan); o.setLfoContour (nan);
    o.setSubOscGain (nan); o.setSuperDetune (nan);
    o.noteOn (45, 100, 0.0);

    double block[rosic::Open303::maxBlockSize];
    for (int i = 0; i < 64; ++i)
        o.renderBlock (block, rosic::Open303::maxBlockSize);
    // reaching here at all is the regression check
}

int main()
{
    testSweepStaysFinite();
    testWavetableIndexGuard();
    testNanSettersDoNotCrash();

    if (failures == 0)
        std::printf ("open303_param_sweep_test: all passed\n");
    return failures == 0 ? 0 : 1;
}