// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Loewenfels
// Public interface modeled on rosic::TeeBeeFilter (Open303, MIT, Copyright (c) 2009 Robin Schmidt).

#ifndef dfl_DiodeLadderFilter_h
#define dfl_DiodeLadderFilter_h

// standard-library includes:
#include <stdlib.h>
#include <cmath>
#include <cstring>    // math_approx.hpp's bit_cast uses std::memcpy
#include <algorithm>  // math_approx.hpp uses std::max/std::min unqualified
#include <utility>    // math_approx.hpp uses std::make_pair

// rosic includes:
#include "rosic_RealFunctions.h"

// chowdsp includes:
#include <math_approx/math_approx.hpp>

namespace dfl
{

  /**
   * 4-pole diode ladder filter based on Will Pirkle's analysis.
   * http://www.willpirkle.com/Downloads/AN-6DiodeLadderFilter.pdf
   *
   * This is an alternative to TeeBeeFilter with a different character.
   * The diode ladder has asymmetric clipping characteristics and a
   * different resonance behavior compared to the transistor ladder.
   */

  class DiodeLadderFilter
  {

  public:

    /** Filter response mode. The ladder's per-stage lowpass taps are mixed
        (Oberheim style) to synthesise band- and high-pass responses from the
        same 4-pole core - identical coefficients to rosic::TeeBeeFilter. */
    enum ResponseMode
    {
      RESPONSE_LP = 0,  // 4-pole lowpass (24 dB/oct) - the plain diode output
      RESPONSE_BP,      // bandpass (12/12 dB/oct)
      RESPONSE_HP       // 4-pole highpass (24 dB/oct)
    };

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. */
    DiodeLadderFilter();

    /** Destructor. */
    ~DiodeLadderFilter();

    //---------------------------------------------------------------------------------------------
    // parameter settings:

    /** Sets the sample-rate for this filter. */
    void setSampleRate(double newSampleRate);

    /** Sets the cutoff frequency for this filter. */
    INLINE void setCutoff(double newCutoff, bool updateCoefficients = true);

    /** Sets the resonance (0-1 range, pre-skewed by Open303). */
    INLINE void setResonance(double newResonance, bool updateCoefficients = true);

    /** Sets the input drive in decibels. */
    void setInputDrive(double newDrive);

    /** Sets the shaper asymmetry, 0..1 (0 = symmetric tanh, 1 = bias SAT_BIAS_MAX). It ramps in with
        the drive (none at or below 0 dB), so the clean / TeeBee-parity region is unaffected. */
    void setSaturationBias(double newBias);

    /** Sets the morph amount (0..1) for the BP and HP responses: 0 = pure bandpass / highpass,
        1 = pure lowpass, crossfading linearly. Ignored in LP mode. (Historically the bass-comp
        knob; the LP/BP output compensation is now a fixed constant, see FIXED_PASSBAND_COMP.) */
    void setPassbandCompensation(double newCompensation) { passbandCompensation = newCompensation; }

    /** Sets the corner frequency (Hz) of the highpass in the feedback path (octave mode
        only). The real circuit's coupling-capacitor network fits a one-pole highpass of
        ~110-135 Hz (Stinchcombe's model; spread by component tolerance). Bleeds
        resonance off as the cutoff falls into the bass. */
    void setFeedbackHighpass(double newCutoff) { feedbackHpFreq = newCutoff; updateFeedbackHp(); calculateCoefficients(); }

    /** Sets whether the first pole is one octave above (TB-303 style ~18dB/oct slope). */
    INLINE void setOctaveMode(bool enabled)
    {
      if (enabled != octaveMode)
      {
        octaveMode = enabled;
        feedbackHpState = 0.0;    // the loop highpass only runs in octave mode
        calculateCoefficients();  // octave mode caps the resonance (see K below)
      }
    }

    /** Sets the response mode (LP/BP/HP). This only re-mixes the ladder's stage
        outputs, so it needs no coefficient recalculation. @see ResponseMode */
    INLINE void setResponseMode(int newMode) { responseMode = newMode; }

    /** Sets the Filter FM depth (0 to 1). Devilfish-style audio-rate cutoff modulation.
     *  Uses AC-coupled input signal to modulate filter cutoff frequency. */
    void setFilterFmDepth(double depth) { filterFmDepth = std::max(0.0, std::min(1.0, depth)); }

    /** Returns the Filter FM depth. */
    double getFilterFmDepth() const { return filterFmDepth; }

    //---------------------------------------------------------------------------------------------
    // inquiry:

    /** Returns the cutoff frequency of this filter. */
    double getCutoff() const { return cutoff; }

    /** Returns the resonance parameter of this filter (0-1 range, pre-skewed). */
    double getResonance() const { return resonance; }

    /** Returns the drive parameter in decibels. */
    double getDrive() const { return drive; }

    /** Returns the passband gain compensation amount (0.0 = none, 1.0 = full compensation). */
    double getPassbandCompensation() const { return passbandCompensation; }

    /** Returns the current response mode (LP/BP/HP). @see ResponseMode */
    int getResponseMode() const { return responseMode; }

    //---------------------------------------------------------------------------------------------
    // audio processing:

    /** Calculates one output sample at a time. */
    INLINE double getSample(double in);

    //---------------------------------------------------------------------------------------------
    // others:

    /** Causes the filter to re-calculate the coefficients. */
    INLINE void calculateCoefficients();

    /** Implements the waveshaping nonlinearity. */
    INLINE double shape(double x);

    /** Resets the internal state variables. */
    void reset();

    //=============================================================================================

  protected:

    // State variables for the 4 one-pole filter stages
    double z1, z2, z3, z4;

    // Coefficients for the diode ladder topology
    double alpha;           // g / (1 + g) - the one-pole alpha
    double alpha2;          // one octave above, for 1st stage
    double G1, G2, G3, G4;  // "Big G" coefficients
    double beta1, beta2, beta3, beta4;   // feedback beta coefficients
    double delta1, delta2, delta3;       // delta coefficients
    double gamma1, gamma2, gamma3;       // gamma coefficients (not to confuse with GAMMA)
    double epsilon1, epsilon2, epsilon3; // epsilon coefficients
    double SG1, SG2, SG3, SG4;          // sigma gain coefficients
    double GAMMA;           // product of all G's
    double K;               // resonance/feedback factor
    double Kcomp;           // drive-independent feedback (K * driveLoopGain in octave mode) for the output compensation
    double plainTrimLP;     // plain Diode amplitude trim (linear gain), see calculateCoefficients
    double plainTrimBPHP;   // BP / HP amplitude trim (linear gain)

    // Scaling factors for each stage (a0 values from Pirkle)
    static constexpr double a1 = 1.0;
    static constexpr double a2 = 0.5;
    static constexpr double a3 = 0.5;
    static constexpr double a4 = 0.5;

    // Headroom for the input drive stage: scale down before the tanh and back
    // up after, so 0 dB drive stays clean and the curve is reserved for when
    // the signal is actually driven. 0.5 = 6 dB headroom (matches DB303).
    static constexpr double headroomBias = 0.5;

    // Asymmetric shaper bias: the shaper is shape(a + b) - shape(b), scaled so its small-signal slope
    // stays 1 (loop gain, resonance and stability are unchanged). b = bias knob (0..1) * SAT_BIAS_MAX,
    // ramping from 0 at 0 dB drive to full at SAT_BIAS_TOP_DB, so the clean region and the
    // TeeBee-parity default are untouched. Large-signal level and resonance depend on b; the level
    // is part of the drive level tables DT below (fitted at SAT_BIAS_DEFAULT).
    static constexpr double SAT_BIAS_MAX     = 1.0;
    static constexpr double SAT_BIAS_TOP_DB  = 16.0;
    static constexpr double SAT_BIAS_DEFAULT = 0.5;

    // Drive level compensation (dB): output gain as a function of drive and resonance so the drive knob is not
    // a volume control. Full-synth renders (default bias, mean over cutoff / env mod) of the level relative to the
    // -3 dB default; the correction is target - measured, with the target rising 1 dB from 0 to +16 dB drive.
    // Tables per filter [octave LP, plain LP, plain BP/HP]; rows: resonance knob 0/30/70/100 %, columns: drive nodes.
    // Fitted by tools/diode-fidelity/fit_drive_trim.py (see docs/diode-octave-fidelity.md).
    static constexpr int    DT_DRIVE_N = 6;
    static constexpr int    DT_RES_N   = 4;
    // Resonance nodes are in the skewed resonance the filter receives: skew(0), skew(.3), skew(.7), skew(1).
    // The level change the bias causes (about +1.5 dB at low resonance and full drive at 0.5) is part of the drive
    // level tables DT below, which are fitted with the default bias; a different bias shifts the level.

    // Exponents for the drive level-makeup (applied as 1/driveFactor^exp). 0 = no makeup (drive
    // boosts level), 1 = full makeup (small-signal level independent of drive). Below 0 dB the
    // shaper is nearly linear, so the drive would only be a volume knob: cancel it fully (CLEAN).
    // Above 0 dB the shaper compresses and drive should keep adding a little level with the grit:
    // SAT is chosen so the level stays within about +-1.5 dB from 0 to +16 dB (see drive_level).
    static constexpr double DRIVE_MAKEUP_EXP_CLEAN = 1.0;
    static constexpr double DRIVE_MAKEUP_EXP_SAT   = 0.65;

    // Saturation depth: above 0 dB the gain into the shaper grows (1 + SAT_DEPTH_EXTRA) times faster than the
    // knob's dB value, so the top of the knob drives the tanh much harder. 0 = knob dB is the shaper gain.
    // Below 0 dB nothing changes (clean region and the TeeBee-parity default are untouched).
    static constexpr double SAT_DEPTH_EXTRA = 0.5;

    // The plain-mode knob 0..1 maps onto the old knob 0..PLAIN_KNOB_SPAN. PLAIN_SKEW_NORM = 1 - exp(-3)
    // is the normaliser of Open303's resonance skew.
    static constexpr double PLAIN_KNOB_SPAN = 0.8;
    static constexpr double PLAIN_SKEW_NORM = 0.950213;

    // Plain Diode / BP / HP resonance law (see calculateCoefficients). The loop gain, in units of the
    // self-oscillation point (~17), is rho(s) with s the remapped resonance: it reaches PLAIN_RHO_ONSET
    // at the skewed resonance PLAIN_ONSET_S (= skew(75%) of the old knob: self-oscillation starts there,
    // i.e. at 93.75% of the new knob) and PLAIN_RHO_MAX at full resonance (the strength these modes had
    // at the old 4.5 dB default drive, sqrt(dB2amp(4.5)));
    // the remapped knob tops out at skew(0.8) = 0.9569, so PLAIN_RHO_MAX itself is never reached.
    static constexpr double PLAIN_ONSET_S   = 0.94147;
    static constexpr double PLAIN_RHO_ONSET = 1.01;
    static constexpr double PLAIN_RHO_MAX   = 1.295679;

    // Octave-mode feedback law: see calculateCoefficients(). The feedback K is capped at this
    // fraction of the diode's own (cutoff- and feedback-HP-dependent) self-oscillation threshold.
    static constexpr double OCTAVE_K_MARGIN = 0.99;

    // Low-resonance output trim (octave mode): the static baseline gain is ~1.2 dB
    // hotter than the TeeBee at low K, so pull it down, fading to unity as K rises:
    // gain = 1 - DEPTH * exp(-K / DECAY). Matches the TeeBee's peak level.
    static constexpr double OCTAVE_LOWRES_TRIM_DEPTH = 0.13;
    static constexpr double OCTAVE_LOWRES_TRIM_DECAY = 2.5;

    // Fixed passband (bass) compensation applied to the LP and BP outputs: output gain
    // (2 + FIXED_PASSBAND_COMP * Kcomp). 0.1 is the value at which the diode's level tracks
    // the TeeBee's across resonance and cutoff (offset spread 0.28 dB); 0 would give the
    // real circuit's larger level dip with resonance. It used to be a user knob.
    static constexpr double FIXED_PASSBAND_COMP = 0.1;

    // Static octave-mode output trim. The filter-drive default is -6 dB (the setting at which
    // brightness and shriek match the TeeBee); at that setting, with drive-independent bass
    // comp, the diode would otherwise be ~4.6 dB quieter than the TeeBee (full-synth mean
    // over 96 settings, spread 0.3 dB).
    static constexpr double OCTAVE_OUTPUT_TRIM = 1.2101;   // 10^(1.66/20)

    // Drive->resonance coupling (octave only). Below the reference the resonance is fully
    // drive-invariant (the boost is floored at 1.0, so lowering drive never weakens it - it
    // only cleans up the saturation stage). Above the reference, harder drive pushes the
    // effective resonance up toward the self-oscillation cap (OCTAVE_K_MARGIN * threshold).
    // REF is the linear driveFactor at +4.5 dB: dB2amp(4.5) (filterDrive 0..1 maps to
    // -3..+16 dB, see FILTER_DRIVE in JC303.cpp). EXP sets the coupling strength.
    static constexpr double OCTAVE_RES_DRIVE_REF = 1.6788;  // = dB2amp(4.5)
    static constexpr double OCTAVE_RES_DRIVE_EXP = 0.5;     // 0 = no coupling (invariant)

    // Cutoff tuning: the diode ladder's resonant frequency sits below the nominal
    // cutoff (~0.71x 4-pole, ~0.79x octave), so scale the cutoff up before
    // computing coefficients to land the resonant peak on the nominal frequency
    // (matching the TeeBee, whose peak tracks its cutoff 1:1).
    static constexpr double CUTOFF_TUNING        = 1.41;  // 4-pole (~1/0.71)
    static constexpr double CUTOFF_TUNING_OCTAVE = 1.17;  // octave (resonant peak / stopband aligned to TeeBee)
    // Low-cutoff correction: the resonant peak sat 6-11% below the TeeBee's at 300-500 Hz with the
    // single constant above, so the octave tuning is raised by up to +8% at 300 Hz, tapering
    // log-linearly to nothing at 5 kHz (peak-frequency error 0.17 -> 0.03, stopband shape 0.48 -> 0.60).
    static constexpr double OCTAVE_TUNING_LOW_BOOST = 0.08;
    static constexpr double OCTAVE_TUNING_LOW_C0    = 300.0;
    static constexpr double OCTAVE_TUNING_LOW_C1    = 5000.0;

    // Highpass DC-null factor (see RESPONSE_HP in getSample). The textbook binomial
    // highpass mix un-4lp1+6lp2-4lp3+lp4 assumes each tap is a unity-gain cascade
    // power (lp_i = un*G^i); this Pirkle diode ladder's a2..a4 = 0.5 stage scaling
    // breaks that, so the low end fails to cancel and leaks a large (~+11 dB) boost.
    // Scaling the lowpass-reconstruction term by this factor nulls the DC exactly.
    // Measured to be 1/5 and independent of cutoff AND resonance (it is fixed by the
    // a-coefficients), so it is a constant, not a runtime-derived value.
    static constexpr double HP_LP_SUBTRACT = 0.2;  // = 1/5, DC-null for the HP mix

    // Filter parameters
    double cutoff;
    double drive;
    double driveFactor;
    double driveMakeup;           // precomputed 1/driveFactor^makeup exponent (piecewise, see DRIVE_MAKEUP_EXP_*)
    double satBiasKnob;           // bias amount, 0..1
    double driveTrim[3];          // drive level compensation, linear, per table: [0] octave, [1] plain LP, [2] plain BP / HP
    double satBias;               // asymmetric shaper offset in effect (knob * SAT_BIAS_MAX * drive ramp)
    void updateSatBias();
    double satBiasShape;          // shape(satBias)
    double satBiasNorm;           // 1 / slope of the shaper at satBias
    double driveLoopGain;         // driveFactor * driveMakeup: small-signal gain of the drive stage inside the loop
    double passbandCompensation;  // BP/HP morph amount (0 = pure BP/HP, 1 = pure LP)
    double resonance;             // resonance parameter (0-1, pre-skewed by Open303)
    double sampleRate;
    bool   octaveMode;            // true = 1st pole one octave above (TB-303 style)
    int    responseMode;          // LP/BP/HP output mixing (@see ResponseMode)

    // Filter FM (Devilfish mod) - audio-rate cutoff modulation from input
    double filterFmDepth;                          // User parameter: 0 (off) to 1 (full)
    double acCouplingState;                        // One-pole HPF state for AC coupling

    // Feedback-path highpass (octave mode): fb = HP(K * loop sum), a one-pole HP built as
    // x - lp(x) with an Euler lowpass, so fb = fbHpGain * (raw - fbHpState), fbHpGain = 1 - a.
    // Its instantaneous gain is folded into the zero-delay solve (no loop delay).
    double feedbackHpFreq;
    double feedbackHpAlpha;   // a = 1 - exp(-2*pi*fc/fs)
    double feedbackHpGain;    // 1 - a
    double feedbackHpState;
    void updateFeedbackHp()
    {
      feedbackHpAlpha = 1.0 - exp(-2.0 * PI * feedbackHpFreq / sampleRate);
      feedbackHpGain  = 1.0 - feedbackHpAlpha;
    }
    static constexpr double acCouplingFreq = 20.0; // AC coupling corner frequency (Hz)
    static constexpr double filterFmScale  = 0.4;  // Scale factor for FM modulation depth
  };

  //-----------------------------------------------------------------------------------------------
  // inlined functions:

  INLINE void DiodeLadderFilter::setCutoff(double newCutoff, bool updateCoefficients)
  {
    if( newCutoff != cutoff )
    {
      if( newCutoff < 200.0 )
        cutoff = 200.0;
      else if( newCutoff > 20000.0 )
        cutoff = 20000.0;
      else
        cutoff = newCutoff;

      if( updateCoefficients == true )
        calculateCoefficients();
    }
  }

  INLINE void DiodeLadderFilter::setResonance(double newResonance, bool updateCoefficients)
  {
    // newResonance is already skewed (0-1 range) from Open303
    resonance = newResonance;

    if( updateCoefficients == true )
      calculateCoefficients();
  }

  INLINE void DiodeLadderFilter::calculateCoefficients()
  {
    // Bilinear transform calculations (cutoff tuned so the resonant peak lands
    // on the nominal frequency - see CUTOFF_TUNING constants)
    double tuning = CUTOFF_TUNING;
    if (octaveMode)
    {
      double w = 1.0 - std::log(std::max(cutoff, OCTAVE_TUNING_LOW_C0) / OCTAVE_TUNING_LOW_C0)
                       / std::log(OCTAVE_TUNING_LOW_C1 / OCTAVE_TUNING_LOW_C0);
      w = std::min(std::max(w, 0.0), 1.0);
      tuning = CUTOFF_TUNING_OCTAVE * (1.0 + OCTAVE_TUNING_LOW_BOOST * w);
    }
    double tunedCutoff = cutoff * tuning;
    double wd = 2.0 * PI * tunedCutoff;
    double T = 1.0 / sampleRate;
    double wa = (2.0 / T) * tan(wd * T / 2.0);
    double gCoeff = wa * T / 2.0;
    double gp1 = 1.0 + gCoeff;

    // Calculate "Big G" coefficients (feedback path gains)
    G4 = 0.5 * gCoeff / gp1;
    G3 = 0.5 * gCoeff / (gp1 - 0.5 * gCoeff * G4);
    G2 = 0.5 * gCoeff / (gp1 - 0.5 * gCoeff * G3);
    G1 = gCoeff / (gp1 - gCoeff * G2);

    // Product of all G's
    GAMMA = G4 * G3 * G2 * G1;

    // Sigma gain coefficients
    SG1 = G4 * G3 * G2;
    SG2 = G4 * G3;
    SG3 = G4;
    SG4 = 1.0;

    // One-pole alpha coefficient
    alpha = gCoeff / gp1;

    // Alpha for 1st stage (one octave above, like TB-303). The octave-up pole
    // reaches Nyquist at half the cutoff the other poles do, so at very high
    // cutoff (and low sample rates) its prewarp argument can exceed pi/2, where
    // tan() goes negative and the stage becomes unstable. Clamp the argument to a
    // safe fraction of Nyquist - only engages at extreme cutoff, no effect
    // otherwise.
    const double MAX_POLE_ARG = 1.4;   // < pi/2 (1.5708)
    double octArg = std::clamp(2*wd * T / 2.0, 0.0, MAX_POLE_ARG);
    double wa2 = (2.0 / T) * tan(octArg);
    double g2 = wa2 * T / 2.0;
    alpha2 = g2 / (1.0 + g2);

    // Beta coefficients
    beta1 = 1.0 / (gp1 - gCoeff * G2);
    beta2 = 1.0 / (gp1 - 0.5 * gCoeff * G3);
    beta3 = 1.0 / (gp1 - 0.5 * gCoeff * G4);
    beta4 = 1.0 / gp1;

    // Gamma coefficients (local feedback)
    gamma1 = 1.0 + G1 * G2;
    gamma2 = 1.0 + G2 * G3;
    gamma3 = 1.0 + G3 * G4;

    // Delta coefficients
    delta1 = gCoeff;
    delta2 = 0.5 * gCoeff;
    delta3 = 0.5 * gCoeff;

    // Epsilon coefficients
    epsilon1 = G2;
    epsilon2 = G3;
    epsilon3 = G4;

    // Feedback factor. Plain Diode: K = 17 is the diode-ladder self-oscillation point.
    // Octave mode follows the TeeBee instead (see the table block below); drive is applied
    // through driveLoopGain (drive-invariance: K is divided by the drive stage's small-signal
    // gain, so the loop gain does not depend on drive).
    if (octaveMode)
    {
      // TeeBee-parity feedback law (fitted offline, drive-0 basis, cutoff tuning 1.17 with the low-cutoff boost): the
      // feedback K that makes the diode's resonant boost equal the TeeBee's at the same
      // cutoff, resonance and feedback-HP setting. K = K100(cutoff, fbHP) * rho(res, cutoff),
      // capped just below the diode's own self-oscillation threshold Kc(cutoff, fbHP).
      static const double CG[11] = { 300, 500, 800, 1000, 2000, 3000, 5000, 8000, 12000, 16000, 20000 };
      static const double FG[3]  = { 76.7, 122.0, 268.0 };   // diode feedback-HP nodes (Hz)
      static const double K100T[3][11] = {
        { 16.140, 16.750, 17.200, 17.370, 17.790, 17.890, 18.070, 18.360, 18.250, 18.360, 18.690 },
        { 15.280, 16.200, 16.820, 17.070, 17.600, 17.810, 18.000, 18.120, 18.240, 18.550, 18.700 },
        { 14.900, 15.260, 15.970, 16.300, 17.230, 17.630, 17.960, 18.200, 18.260, 18.710, 19.010 } };
      static const double KCT[3][11] = {
        { 24.770, 22.020, 20.520, 20.020, 19.050, 18.730, 18.480, 18.360, 18.360, 18.470, 18.700 },
        { 29.430, 24.660, 22.120, 21.300, 19.680, 19.160, 18.740, 18.530, 18.480, 18.560, 18.780 },
        { 30.000, 30.000, 27.700, 25.680, 21.810, 20.570, 19.600, 19.080, 18.860, 18.850, 19.020 } };
      static const double RHOT[11][5] = {
        { 0.470, 0.740, 0.880, 0.980, 1.000 },
        { 0.444, 0.728, 0.873, 0.960, 1.000 },
        { 0.436, 0.719, 0.870, 0.955, 1.000 },
        { 0.456, 0.726, 0.872, 0.956, 1.000 },
        { 0.456, 0.736, 0.877, 0.959, 1.000 },
        { 0.473, 0.743, 0.888, 0.960, 1.000 },
        { 0.494, 0.767, 0.897, 0.966, 1.000 },
        { 0.526, 0.791, 0.915, 0.971, 1.000 },
        { 0.570, 0.824, 0.929, 0.978, 1.000 },
        { 0.605, 0.846, 0.939, 0.979, 1.000 },
        { 0.644, 0.870, 0.954, 0.992, 1.000 }
      };
      static const double RS[6] = { 0.0, 0.47512, 0.73553, 0.87850, 0.95724, 1.0 };   // skew(res) at the rho nodes

      double lc = std::log(std::min(std::max(cutoff, CG[0]), CG[10]));
      int ci = 0; while (ci < 9 && std::log(CG[ci + 1]) < lc) ++ci;
      double cf = (lc - std::log(CG[ci])) / (std::log(CG[ci + 1]) - std::log(CG[ci]));
      double lf = std::log(std::min(std::max(feedbackHpFreq, FG[0]), FG[2]));
      int fi = (lf < std::log(FG[1])) ? 0 : 1;
      double ff = (lf - std::log(FG[fi])) / (std::log(FG[fi + 1]) - std::log(FG[fi]));
      auto bil = [&](const double t[3][11]) {
        double a = t[fi][ci] + cf * (t[fi][ci + 1] - t[fi][ci]);
        double b = t[fi + 1][ci] + cf * (t[fi + 1][ci + 1] - t[fi + 1][ci]);
        return a + ff * (b - a); };
      double k100 = bil(K100T);
      // Self-oscillation threshold for the cap below. Interpolated in cutoff and feedback-HP corner
      // normally; when drive coupling pushes the resonance up (drive above the reference) it uses the
      // lowest-corner column instead (interpolation error between HP nodes plus the push could
      // otherwise cross the true threshold).
      double kcInterp = bil(KCT);
      double kcSafe = KCT[0][ci] + cf * (KCT[0][ci + 1] - KCT[0][ci]);
      // rho(res) at this cutoff: piecewise-linear in the skewed resonance
      double r[6]; r[0] = 0.0; for (int k = 0; k < 5; ++k) r[k + 1] = RHOT[ci][k] + cf * (RHOT[ci + 1][k] - RHOT[ci][k]);
      int ri = 0; while (ri < 4 && RS[ri + 1] < resonance) ++ri;
      double rf = (std::min(resonance, 1.0) - RS[ri]) / (RS[ri + 1] - RS[ri]);
      double effK = k100 * (r[ri] + rf * (r[ri + 1] - r[ri]));
      // Drive above the calibration reference pushes resonance up (floored at 1.0 below it).
      double coupling = pow(std::max(driveFactor, OCTAVE_RES_DRIVE_REF) / OCTAVE_RES_DRIVE_REF,
                            OCTAVE_RES_DRIVE_EXP);
      effK *= coupling;
      effK = std::min(effK, OCTAVE_K_MARGIN * (coupling > 1.0 ? kcSafe : kcInterp));   // stay just under self-oscillation
      K = effK / driveLoopGain;
      Kcomp = effK;   // the bass-comp and low-res trim must not depend on drive
      plainTrimLP = plainTrimBPHP = 1.0;
    }
    else
    {
      // Plain Diode / BP / HP: the loop gain (K * driveLoopGain) is 17 * rho(resonance)
      // regardless of drive, so the drive knob changes the saturation only. rho is linear in the
      // remapped resonance up to the onset of self-oscillation (rho = 1.01), then rises quadratically
      // to the old default's full strength at the old resonance 100.
      // The knob only spans 0..PLAIN_KNOB_SPAN of the old range (the top of the old range all sounded
      // alike). resonance is skew(knob), so skew(span * knob) = (1 - (1 - c*resonance)^span) / c.
      const double rIn = std::min(resonance, 1.0);
      const double s = (1.0 - std::pow(1.0 - PLAIN_SKEW_NORM * rIn, PLAIN_KNOB_SPAN)) / PLAIN_SKEW_NORM;
      const double sk = PLAIN_RHO_ONSET / PLAIN_ONSET_S;
      const double u = std::max(0.0, (s - PLAIN_ONSET_S) / (1.0 - PLAIN_ONSET_S));
      const double rho = sk * s + (PLAIN_RHO_MAX - sk) * u * u;
      K = 17.0 * rho / driveLoopGain;
      Kcomp = 17.0 * s;
      // Amplitude trim (dB) as a function of rho that matches the level these modes had at the old
      // 4.5 dB default drive at the same loop gain: they are ~4 dB louder in the linear regime and
      // ~4-5 dB quieter when self-oscillating at the new -6 dB default (fitted, mean over cutoffs).
      static const double PR[9]  = { 0.0, 0.4941, 0.8092, 1.0101, 1.1018, 1.1695, 1.2199, 1.2571, 1.2957 };
      static const double TLP[9] = { 1.51, 1.63, 1.72, 0.45, -2.70, -4.50, -5.10, -5.30, -5.50 };   // plain Diode
      static const double TBH[9] = { 1.18, 0.27, 0.05, -1.05, -4.00, -5.35, -5.75, -5.85, -5.95 };  // BP / HP
      int ti = 0; while (ti < 7 && PR[ti + 1] < rho) ++ti;
      const double tf = std::min(std::max((rho - PR[ti]) / (PR[ti + 1] - PR[ti]), 0.0), 1.0);
      plainTrimLP   = pow(10.0, (TLP[ti] + tf * (TLP[ti + 1] - TLP[ti])) / 20.0);
      plainTrimBPHP = pow(10.0, (TBH[ti] + tf * (TBH[ti + 1] - TBH[ti])) / 20.0);
    }

    // Drive level compensation (see DT_* above)
    {
      static const double DN[DT_DRIVE_N] = { -3.0, 0.0, 4.5, 9.0, 12.5, 16.0 };
      static const double RN[DT_RES_N]   = { 0.0, 0.6245, 0.9235, 1.0 };
      static const double DT[3][DT_RES_N][DT_DRIVE_N] = {
        { { 0.00, 0.21, -0.53, -0.22, 0.98, 3.05 },
          { 0.00, 0.07, -1.09, -1.45, -2.67, -4.04 },
          { 0.00, 0.17, -1.02, -2.35, -3.65, -5.02 },
          { 0.00, 0.30, -0.83, -2.16, -3.46, -4.83 } },
        { { 0.00, 0.21, -0.53, -0.21, 1.00, 3.07 },
          { 0.00, 0.12, -1.28, -2.52, -3.31, -3.72 },
          { 0.00, 0.46, -0.18, -1.12, -2.00, -2.71 },
          { 0.00, 1.55, 2.50, 2.30, 1.59, 0.88 } },
        { { 0.00, 0.33, 0.34, 1.65, 3.46, 5.68 },
          { 0.00, 0.68, 2.35, 5.55, 7.84, 9.59 },
          { 0.00, 1.26, 3.94, 8.19, 11.75, 14.08 },
          { 0.00, 2.22, 5.81, 9.98, 13.23, 15.81 } } };
      int di = 0; while (di < DT_DRIVE_N - 2 && DN[di + 1] < drive) ++di;
      const double df = std::clamp((drive - DN[di]) / (DN[di + 1] - DN[di]), 0.0, 1.0);
      const double rr = std::min(std::max(resonance, 0.0), 1.0);
      int ri = 0; while (ri < DT_RES_N - 2 && RN[ri + 1] < rr) ++ri;
      const double rf = (rr - RN[ri]) / (RN[ri + 1] - RN[ri]);
      for (int m = 0; m < 3; ++m)
      {
        const double a = DT[m][ri][di]     + df * (DT[m][ri][di + 1]     - DT[m][ri][di]);
        const double b = DT[m][ri + 1][di] + df * (DT[m][ri + 1][di + 1] - DT[m][ri + 1][di]);
        driveTrim[m] = pow(10.0, (a + rf * (b - a)) / 20.0);
      }
    }

  }

  INLINE double DiodeLadderFilter::shape(double x)
  {
    // Soft clipping - tanh approximation
    return math_approx::tanh<7>(x);
  }

  INLINE double DiodeLadderFilter::getSample(double in)
  {
    // Safety: if the ladder state ever goes non-finite (extreme cutoff/feedback
    // modulation), reset it so the filter self-heals instead of latching to
    // permanent silence via propagating NaN/Inf.
    if (!std::isfinite(z1 + z2 + z3 + z4))
      z1 = z2 = z3 = z4 = 0.0;

    // Input without compensation - compensation applied at output
    double input = in;

    // -------------------------------------------------------------------------
    // Filter FM (Devilfish mod): AC-coupled input modulates cutoff
    // Uses one-pole HPF for AC coupling (~20Hz corner), then scales the one-pole
    // alpha coefficients per sample for audio-rate cutoff modulation.
    // -------------------------------------------------------------------------
    double fmMod = 1.0;
    if (filterFmDepth > 0.0) {
      // One-pole HPF for AC coupling: y[n] = x[n] - x_lp[n]
      // where x_lp is one-pole LPF output
      double acAlpha = 2.0 * PI * acCouplingFreq / sampleRate;
      acCouplingState += acAlpha * (input - acCouplingState);
      double acCoupledInput = input - acCouplingState;  // HPF output = input - LPF output

      // Modulate alpha coefficients based on AC-coupled input
      fmMod = std::max(0.5, std::min(1.5, 1.0 + filterFmDepth * filterFmScale * acCoupledInput));
    }

    double alphaFM  = alpha  * fmMod;
    double alpha2FM = alpha2 * fmMod;

    // Calculate feedback signals (S4 -> S3 -> S2 -> S1)
    double S4 = beta4 * z4;
    double S3 = beta3 * (z3 + S4 * delta3);
    double S2 = beta2 * (z2 + S3 * delta2);
    double S1 = beta1 * (z1 + S2 * delta1);

    // SIGMA - weighted sum of feedback signals
    double SIGMA = SG1 * S1 + SG2 * S2 + SG3 * S3 + SG4 * S4;

    // Form input to the ladder (with feedback)
    double un;
    if (octaveMode)
    {
      // un = input - fbHpGain*(K*(SIGMA + GAMMA*g*un) - s)  =>  solve for un exactly, with g the
      // drive stage's small-signal gain (driveLoopGain) between un and the ladder input.
      double kg = feedbackHpGain * K;
      un = (input - kg * SIGMA + feedbackHpGain * feedbackHpState) / (1.0 + kg * driveLoopGain * GAMMA);
      feedbackHpState += feedbackHpAlpha * (K * (SIGMA + GAMMA * driveLoopGain * un) - feedbackHpState);
    }
    else
      un = (input - K * SIGMA) / (1.0 + K * driveLoopGain * GAMMA);

    // Apply input nonlinearity with headroom scaling: scale down before the
    // tanh and back up after, so 0 dB drive (driveFactor == 1) stays clean
    // and turning up the drive pushes the signal into saturation. Matches the
    // DB303 shaper so the 0..9 dB range feels the same.
    // driveMakeup is the level compensation (see setInputDrive): without it the
    // pre-tanh driveFactor gain doubles as a volume boost, so the drive knob
    // acted as a loudness control. It keeps drive roughly level-neutral while the
    // tanh still saturates peaks - so drive changes timbre/resonance-compression,
    // not overall level.
    un = (shape(driveFactor * un * headroomBias + satBias) - satBiasShape) * satBiasNorm * driveMakeup / headroomBias;

    // 1st stage (optionally one octave above for TB-303 style slope)
    double xin = un * gamma1 + S2 + epsilon1 * S1;
    double v = (a1 * xin - z1) * (octaveMode ? alpha2FM : alphaFM);
    double lp1 = v + z1;
    z1 = lp1 + v;

    // 2nd stage
    xin = lp1 * gamma2 + S3 + epsilon2 * S2;
    v = (a2 * xin - z2) * alphaFM;
    double lp2 = v + z2;
    z2 = lp2 + v;

    // 3rd stage
    xin = lp2 * gamma3 + S4 + epsilon3 * S3;
    v = (a3 * xin - z3) * alphaFM;
    double lp3 = v + z3;
    z3 = lp3 + v;

    // 4th stage
    v = (a4 * lp3 - z4) * alphaFM;
    double lp4 = v + z4;
    z4 = lp4 + v;

    // Oberheim multi-mode output: mix the ladder's per-stage lowpass taps to
    // synthesise band- and high-pass responses from the same 4-pole core.
    //   LP (24 dB/oct):  lp4
    //   BP (12/12):      lp2 - 2*lp3 + lp4      (0.25 scale)
    //   HP (24 dB/oct):  un - 0.2*(4*lp1 - 6*lp2 + 4*lp3 - lp4)
    // where `un` is the (shaped) signal entering the ladder - the y0 term. The HP
    // uses the DC-null factor HP_LP_SUBTRACT (see there): the plain binomial mix
    // leaks the low end in this diode topology, so the lowpass-reconstruction term
    // is scaled to cancel DC, giving a real highpass whose passband gain matches LP
    // (measured within ~2%). Works in plain and octave mode; the steeper octave 1st
    // pole just shifts the corner, as it does for LP.
    double out;
    switch (responseMode)
    {
      case RESPONSE_BP:
      {
        // BP mode: the morph knob crossfades BP -> LP (0 = pure bandpass, 1 = pure lowpass),
        // mirroring the HP morph below.
        double bp = 0.25 * (lp2 - 2.0 * lp3 + lp4);
        double t  = std::clamp(passbandCompensation, 0.0, 1.0);
        out = (1.0 - t) * bp + t * lp4;
        break;
      }
      case RESPONSE_HP:
      {
        // HP mode: the morph knob (passbandCompensation) crossfades HP -> LP: 0 = pure highpass,
        // 1 = pure lowpass, linearly. The midpoint is a notch (highs from HP + lows from LP). Both endpoints
        // share ~the same passband gain, so the sweep stays even in level.
        double hp = un - HP_LP_SUBTRACT * (4.0 * lp1 - 6.0 * lp2 + 4.0 * lp3 - lp4);
        double t  = std::clamp(passbandCompensation, 0.0, 1.0);
        out = (1.0 - t) * hp + t * lp4;
        break;
      }
      case RESPONSE_LP:
      default:
        out = lp4;
        break;
    }

    // Apply passband (bass) gain compensation at output (keeps saturation
    // independent of it). This restores the LP bass droop that resonance causes.
    // In HP mode the same knob is repurposed as the HP->LP morph (above), so the
    // bass-comp boost is not applied there (would double-use the control + run hot).
    double comp = (responseMode == RESPONSE_HP) ? 1.0
                                                : (2.0 + FIXED_PASSBAND_COMP * Kcomp);
    // Octave mode: keep peak level on par with the TeeBee at low resonance (see
    // OCTAVE_LOWRES_TRIM_*). The diode may stay a little hotter in peaks than the
    // TeeBee only in plain Diode mode, never in octave mode.
    if (!octaveMode)
      comp *= (responseMode == RESPONSE_LP) ? plainTrimLP : plainTrimBPHP;
    if (octaveMode && responseMode != RESPONSE_HP)
      comp *= OCTAVE_OUTPUT_TRIM * (1.0 - OCTAVE_LOWRES_TRIM_DEPTH * exp(-Kcomp / OCTAVE_LOWRES_TRIM_DECAY));
    comp *= driveTrim[octaveMode ? 0 : (responseMode == RESPONSE_LP ? 1 : 2)];
    return out * comp;
  }

} // end namespace dfl

#endif // dfl_DiodeLadderFilter_h
