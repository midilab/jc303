// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Lowenfels
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
   * Four equal stage poles: the TB-303's half-value bottom cap (C18) sees only one diode pair, so its pole matches the
   * others, and the stages' coupling spreads the poles (about 18 dB/oct from 2 to 4 x cutoff). With the real circuit's
   * feedback coupling network and resonance-pot law, calibrated to a SPICE model of the TB-303 VCF (see calculateCoefficients).
   * The mode morph (LP -> BP -> HP) mixes the ladder's stage taps (see getSample).
   */

  class DiodeLadderFilter
  {

  public:

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
        the drive (none at or below 0 dB), so the clean region is unaffected. */
    void setSaturationBias(double newBias);

    /** Sets the bass comp amount (0..1): restores part of the passband loss that resonance causes (see getSample). */
    void setPassbandCompensation(double newCompensation) { passbandCompensation = newCompensation; }

    /** Sets the response mode, 0..1: 0 = lowpass, 0.5 = bandpass, 1 = highpass, morphing without notches in between
        (see getSample). Cheap, so it may change per sample. */
    void setMode(double newMode) { mode = std::clamp(newMode, 0.0, 1.0); }

    /** Sets the Filter Feedback corner (Hz): the real circuit's coupling network fits a one-pole highpass of
        ~115 Hz at Stinchcombe's Q21 beta (about 200), and the knob scales the network's dominant pole by
        newCutoff / 115 (see NET_POLE). Bleeds resonance off as the cutoff falls into the bass. */
    void setFeedbackHighpass(double newCutoff) { feedbackHpFreq = newCutoff; calculateCoefficients(); }

    /** The default Filter Feedback corner: Q21 beta 300, as in the SPICE netlist the feedback law is fitted to
        (tools/diode-fidelity/spice/tb303_vcf.py), whose C27/C25 coupling corner is 63.2 Hz (92.0 Hz at beta 200). */
    static constexpr double FEEDBACK_DEFAULT = 79.0;

    /** Sets the unit trim: a loop-gain scale, TRIM_MIN..TRIM_MAX (1 = the nominal unit).
        Component tolerances make real units differ mostly by such a scale (spice/mc_native.py). Above 1 the
        oscillation cap rises with it, so a hot unit self-oscillates at high resonance as some real units do. */
    void setLoopGainTrim(double newTrim)
    {
      loopGainTrim = std::clamp(newTrim, TRIM_MIN, TRIM_MAX);
      calculateCoefficients();
    }

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

    /** The resonance pot's rotation, 0..1 (the knob with Open303's skew undone). */
    double getResonancePot() const { return -std::log(1.0 - std::clamp(resonance, 0.0, 1.0) * RES_SKEW_NORM) / 3.0; }

    /** The dominant coupling pole (Hz), C27/C25 into Q21, as scaled by the Filter Feedback knob. It sits
        in the output path as well as the feedback loop (see TB303OutputNetwork). */
    double getCouplingPoleHz() const { return NET_POLE[0] / (2.0 * PI) * feedbackHpFreq / 115.0; }

    /** Returns the drive parameter in decibels. */
    double getDrive() const { return drive; }

    /** Returns the passband gain compensation amount (0.0 = none, 1.0 = full compensation). */
    double getPassbandCompensation() const { return passbandCompensation; }

    /** Returns the response mode, 0..1 (LP / BP / HP). */
    double getMode() const { return mode; }

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
    double alpha4;          // stage 4's alpha, raised by the Q21 load (see LOAD_KAPPA)
    double G1, G2, G3, G4;  // "Big G" coefficients
    double beta1, beta2, beta3, beta4;   // feedback beta coefficients
    double delta1, delta2, delta3;       // delta coefficients
    double gamma1, gamma2, gamma3;       // gamma coefficients (not to confuse with GAMMA)
    double epsilon1, epsilon2, epsilon3; // epsilon coefficients
    double SG1, SG2, SG3, SG4;          // sigma gain coefficients
    double GAMMA;           // product of all G's
    double K;               // resonance/feedback factor

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
    // ramping from 0 at 0 dB drive to full at SAT_BIAS_TOP_DB, so the clean region is untouched.
    // Large-signal level and resonance depend on b; the level is part of the drive level tables DT below
    // (fitted at SAT_BIAS_DEFAULT).
    static constexpr double SAT_BIAS_MAX     = 1.0;
    static constexpr double SAT_BIAS_TOP_DB  = 16.0;
    static constexpr double SAT_BIAS_DEFAULT = 0.5;

    // Drive level compensation (dB): output gain as a function of drive and resonance so the drive knob is not
    // a volume control. Full-synth renders (default bias, mean over cutoff / env mod) of the level relative to the
    // -3 dB default; the correction is target - measured, with the target rising 1 dB from 0 to +16 dB drive.
    // Tables per response [LP, BP / HP]; rows: resonance knob 0/30/70/100 %, columns: drive nodes.
    // Fitted by tools/diode-fidelity/fit_drive_trim.py (see docs/diode-octave-fidelity.md).
    static constexpr int    DT_DRIVE_N = 6;
    static constexpr int    DT_RES_N   = 4;

    // Exponents for the drive level-makeup (applied as 1/driveFactor^exp). 0 = no makeup (drive
    // boosts level), 1 = full makeup (small-signal level independent of drive). Below 0 dB the
    // shaper is nearly linear, so the drive would only be a volume knob: cancel it fully (CLEAN).
    // Above 0 dB the shaper compresses and drive should keep adding a little level with the grit:
    // SAT is chosen so the level stays within about +-1.5 dB from 0 to +16 dB (see drive_level).
    static constexpr double DRIVE_MAKEUP_EXP_CLEAN = 1.0;
    static constexpr double DRIVE_MAKEUP_EXP_SAT   = 0.65;

    // Saturation depth: above 0 dB the gain into the shaper grows (1 + SAT_DEPTH_EXTRA) times faster than the
    // knob's dB value, so the top of the knob drives the tanh much harder. 0 = knob dB is the shaper gain.
    // Below 0 dB nothing changes (the clean region is untouched).
    static constexpr double SAT_DEPTH_EXTRA = 0.5;

    // RES_SKEW_NORM = 1 - exp(-3) is the normaliser of Open303's resonance skew.
    static constexpr double RES_SKEW_NORM = 0.950213;

    // Tap mixes giving p^k times the lowpass, p = s / wg (see getSample, updateTapMix). Without the Q21 load the
    // ladder's taps relative to lp4 are lp3 -> 2 + 2p, lp2 -> 3 + 8p + 4p^2, lp1 -> 4 + 20p + 24p^2 + 8p^3,
    // un -> 1 + 16p + 40p^2 + 32p^3 + 8p^4 (stage k > 1 takes 0.5 (y[k-1] + y[k+1]), stage 1 takes un + y2); the load
    // changes the top stage, so each tap is P(p) lp4 + c w (w = load lowpass, p w = rho (lp4 - w)). The basis is
    // triangular, so weights on un, lp1..lp4 give p^0..p^4 exactly, less tapW * w; the global feedback and the coupling
    // network only change the shared denominator.
    double tapMix[5][5] = {};
    double tapW[5] = {};
    void updateTapMix(double rho);

    // Static BP / HP output gains (relative to LP) so the pure modes sit near LP loudness on a saw at a typical
    // resonant 303 setting (resonance 50..70, cutoff 800..1500 Hz, see mode_level). No correction by cutoff: the
    // BP/HP-vs-LP gap at low resonance and high cutoff is the filter's character (a saw has most of its energy at the
    // low harmonics that BP/HP remove).
    static constexpr double BP_GAIN = 8.283;   // +18.4 dB
    static constexpr double HP_GAIN = 8.262;   // +18.3 dB (the p^4 mix has passband 1/8)

    // Filter parameters
    double cutoff;
    double drive;
    double driveFactor;
    double driveMakeup;           // precomputed 1/driveFactor^makeup exponent (piecewise, see DRIVE_MAKEUP_EXP_*)
    double satBiasKnob;           // bias amount, 0..1
    double driveTrim[2];          // drive level compensation, linear, per table: [0] LP, [1] BP / HP
    double satBias;               // asymmetric shaper offset in effect (knob * SAT_BIAS_MAX * drive ramp)
    void updateSatBias();
    double satBiasShape;          // shape(satBias)
    double satBiasNorm;           // 1 / slope of the shaper at satBias
    double driveLoopGain;         // driveFactor * driveMakeup: small-signal gain of the drive stage inside the loop
    double passbandCompensation;  // bass comp amount, 0..1
    double resonance;             // resonance parameter (0-1, pre-skewed by Open303)
    double sampleRate;
    double mode = 0.0;            // response mode, 0 = LP, 0.5 = BP, 1 = HP

    // Filter FM (Devilfish mod) - audio-rate cutoff modulation from input
    double filterFmDepth;                          // User parameter: 0 (off) to 1 (full)
    double acCouplingState;                        // One-pole HPF state for AC coupling

    // Filter Feedback corner (Hz), scales the coupling network's dominant pole (see NET_POLE)
    double feedbackHpFreq;
    // The real feedback coupling network (Stinchcombe's full TB-303 analysis, poles and zeroes in
    // rad/s) F(s) = s^4 (s+46.5)(s+4.40) / ((s+578.1)(s+97.5)(s+38.5)(s+20.0)(s+7.41)(s+4.45)), unity at high
    // frequency, run as a cascade of TPT one-poles inside the zero-delay loop solve: four highpasses and the
    // (s+46.5)/(s+38.5) shelf. (s+4.40)/(s+4.45) is unity within 0.1 dB and is left out. The Filter Feedback knob
    // scales the dominant 578.1 rad/s (92 Hz) pole by feedbackHpFreq / 115.
    static constexpr int    NET_N = 5;
    static constexpr double NET_POLE[NET_N] = { 578.1, 97.5, 20.0, 7.41, 38.5 };
    static constexpr double NET_SHELF_ZERO  = 46.5;   // zero of the last section
    // Cutoff: the cutoff parameter is the circuit's ladder-core 180-degree frequency, 2^(1/4) x its
    // corner wc (Stinchcombe's core polynomial s^4 + 6.727 s^3 + 14.142 s^2 + 9.514 s + 1), the frequency the
    // resonant peak approaches at high cutoff. The equal-stage ladder's D0 (see TAP_MIX) is exactly that polynomial
    // with the stage rate at 2^(3/4) wc, so tuning = 2^(3/4) / 2^(1/4).
    static constexpr double CUTOFF_TUNING = 1.41421356237;
    // Oscillation cap, as a fraction of the interpolated threshold KNC. 0.98 rather than 0.99: between the
    // 1 and 2 kHz nodes at Filter Feedback 1.0 the threshold sags below the interpolation (K / threshold reached
    // 0.9956 at 0.99 on a dense 20 x 20 check); it binds only near that knob extreme.
    static constexpr double K_MARGIN = 0.98;
    // Bass comp knob range (amount at knob 1.0): the output gain (1 + K)^amount restores that fraction (in dB) of the
    // ladder's resonance passband loss. 0 (the default) is the real circuit.
    static constexpr double BASS_COMP_MAX = 0.12;
    // Unit trim range: about the 5th..95th percentile of the per-unit loop-gain scale in the component
    // tolerance Monte Carlo (x0.85..x1.16 at 500 Hz, x0.90..x1.12 above). At x1.15 a unit at full pot sits up to
    // 1.15 x the oscillation threshold and sustains a sine 5-11 dB below a resonance-0 saw.
    static constexpr double TRIM_MIN = 0.85;
    static constexpr double TRIM_MAX = 1.15;
    // Q21 loading: C27 / C25 into Q21's bases is a series RC on the ladder top. In the stage equations it adds
    // kappa p / (1 + p / rho) y4 to the top node (p = s / wg, rho = coupling pole / wg, kappa = C27 / (2 C26) with
    // the top rung's diode resistance 1 / (wg C26)), i.e. an extra conductance kappa rho above the coupling pole that
    // a one-pole lowpass of y4 at the pole cancels below it. SPICE: the loss is up to 1.9 dB at 300 Hz cutoff, and
    // this matches it within 0.16 dB (20-640 Hz, 300 Hz-5 kHz cutoff) with no fitted parameter.
    static constexpr double LOAD_KAPPA = 0.1e-6 / (2.0 * 33e-9);
    double loadKR    = 0.0;   // kappa rho
    double loadSGain = 0.0;   // the load lowpass state's share of S4 (stage 4 and the lowpass are solved together)
    double loadA     = 0.0;   // load lowpass: TPT one-pole coefficient at the coupling pole
    double loadState = 0.0;
    double loadW     = 0.0;   // load lowpass output
    double netA[NET_N] = {};      // feedback network: TPT one-pole coefficients g / (1 + g)
    double netState[NET_N] = {};
    double shelfRatio = 1.0;      // NET_SHELF_ZERO / shelf pole
    double logLoopGain = 0.0;   // log(1 + K): the bass comp gain is exp(amount * logLoopGain)
    double loopGainTrim = 1.0;    // unit trim, see setLoopGainTrim

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
    // Bilinear transform, prewarped at the cutoff (the resonant peak) rather than the stage rate, so the peak
    // frequency does not depend on the sample rate (stage rate tuned to the circuit's ladder core, see CUTOFF_TUNING)
    double gCoeff = CUTOFF_TUNING * tan(PI * cutoff / sampleRate);
    double gp1 = 1.0 + gCoeff;

    // Q21 loading on stage 4 (see LOAD_KAPPA): rate g (1 + kappa rho), input (a4 lp3 + kappa rho w) / (1 + kappa rho),
    // w = (gl lp4 + loadState) / (1 + gl) solved with it. rho = gl / g is the coupling pole over the stage rate in the
    // discrete domain, so the tap relations (updateTapMix) hold exactly.
    const double gl = std::tan(PI * std::min(getCouplingPoleHz(), 0.45 * sampleRate) / sampleRate);
    const double rho = gl / gCoeff;
    loadKR = LOAD_KAPPA * rho;
    const double g4 = gCoeff * (1.0 + loadKR);
    alpha4 = g4 / (1.0 + g4);
    loadA = gl / (1.0 + gl);
    const double den4 = 1.0 + g4 - gCoeff * loadKR * loadA;
    loadSGain = gCoeff * loadKR / (1.0 + gl);
    updateTapMix(rho);

    // Calculate "Big G" coefficients (feedback path gains)
    G4 = 0.5 * gCoeff / den4;
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

    // Beta coefficients
    beta1 = 1.0 / (gp1 - gCoeff * G2);
    beta2 = 1.0 / (gp1 - 0.5 * gCoeff * G3);
    beta3 = 1.0 / (gp1 - 0.5 * gCoeff * G4);
    beta4 = 1.0 / den4;

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

    // Feedback factor. Drive is applied through driveLoopGain (drive-invariance: K is divided by the drive stage's
    // small-signal gain, so the loop gain does not depend on drive).
    // The resonance knob is the real pot's rotation (linear 50k, loaded), so undo Open303's skew.
    // The loop gain K(pot, cutoff) is solved offline so the resonant boost equals the SPICE model of the real VCF
    // (tools/diode-fidelity/gen_native_tables.py: spice/native_targets.py + native_solver, 0.26 dB rms over the curves);
    // KNC is the measured self-oscillation threshold (growth test, native_solver) at the Filter Feedback nodes
    // 57.5 / 79 / 268 Hz (knob 1.0 / default / 0; Open303 maps the knob). The knob stops at Q21 beta about 500:
    // beyond it the coupling network's ~7 Hz mode motorboats at high resonance (SPICE: beta 800 at full pot).
    for (int i = 0; i < NET_N; ++i)
    {
      const double p = NET_POLE[i] * (i == 0 ? feedbackHpFreq / 115.0 : 1.0);
      const double gn = std::tan(std::min(0.5 * p / sampleRate, 1.4));
      netA[i] = gn / (1.0 + gn);
    }
    shelfRatio = NET_SHELF_ZERO / NET_POLE[NET_N - 1];
    static const double CN[11] = { 300, 500, 800, 1000, 2000, 3000, 5000, 8000, 12000, 16000, 20000 };
    static const double AN[7]  = { 0.0, 0.2, 0.4, 0.6, 0.8, 0.9, 1.0 };
    static const double KNT[11][7] = {
      { 0, 3.438, 7.573, 8.984, 12.574, 14.438, 16.671 },
      { 0, 3.198, 6.025, 8.883, 12.397, 14.466, 16.893 },
      { 0, 3.169, 5.886, 9.122, 12.504, 14.543, 16.908 },
      { 0, 3.344, 6.126, 9.005, 12.583, 14.583, 16.931 },
      { 0, 3.145, 6.048, 9.074, 12.583, 14.614, 16.994 },
      { 0, 3.185, 6.072, 9.054, 12.560, 14.597, 16.988 },
      { 0, 3.091, 5.985, 9.040, 12.541, 14.600, 16.932 },
      { 0, 3.021, 5.925, 9.017, 12.466, 14.527, 16.885 },
      { 0, 2.909, 5.961, 8.955, 12.375, 14.419, 16.843 },
      { 0, 2.762, 5.866, 8.855, 12.287, 14.358, 16.702 },
      { 0, 2.588, 5.774, 8.761, 12.198, 14.250, 16.561 } };
    static const double KNC[3][11] = {
      { 27.171, 22.849, 20.567, 19.830, 18.391, 17.921, 17.549, 17.341, 17.225, 17.166, 17.130 },
      { 30.769, 24.833, 21.745, 20.754, 18.835, 18.214, 17.722, 17.448, 17.295, 17.219, 17.172 },
      { 59.998, 45.040, 33.247, 29.640, 22.947, 20.876, 19.280, 18.405, 17.922, 17.681, 17.535 } };
    const double pot = getResonancePot();
    double lc = std::log(std::clamp(cutoff, CN[0], CN[10]));
    int ci = 0; while (ci < 9 && std::log(CN[ci + 1]) < lc) ++ci;
    const double cf = (lc - std::log(CN[ci])) / (std::log(CN[ci + 1]) - std::log(CN[ci]));
    int ai = 0; while (ai < 5 && AN[ai + 1] < pot) ++ai;
    const double af = std::clamp((pot - AN[ai]) / (AN[ai + 1] - AN[ai]), 0.0, 1.0);
    auto at = [&](int c) { return KNT[c][ai] + af * (KNT[c][ai + 1] - KNT[c][ai]); };
    double effK = loopGainTrim * (at(ci) + cf * (at(ci + 1) - at(ci)));
    static const double FN[3] = { 57.5, FEEDBACK_DEFAULT, 268.0 };
    const double lf = std::log(std::clamp(feedbackHpFreq, FN[0], FN[2]));
    const int fi = (lf < std::log(FN[1])) ? 0 : 1;
    const double ff = (lf - std::log(FN[fi])) / (std::log(FN[fi + 1]) - std::log(FN[fi]));
    auto kcAt = [&](int h) { return KNC[h][ci] + cf * (KNC[h][ci + 1] - KNC[h][ci]); };
    // the cap rises with a trim above 1 (a hot unit may self-oscillate); below 1 the trim only lowers K
    effK = std::min(effK, K_MARGIN * std::max(loopGainTrim, 1.0) * (kcAt(fi) + ff * (kcAt(fi + 1) - kcAt(fi))));
    K = effK / driveLoopGain;
    logLoopGain = std::log1p(effK);

    // Drive level compensation (see DT_* above)
    {
      static const double DN[DT_DRIVE_N] = { -3.0, 0.0, 4.5, 9.0, 12.5, 16.0 };
      static const double RN[DT_RES_N]   = { 0.0, 0.6245, 0.9235, 1.0 };
      static const double DT[2][DT_RES_N][DT_DRIVE_N] = {
        { { 0.00, 0.23, -0.37, 0.16, 1.49, 3.57 },
          { 0.00, 0.08, -1.54, -2.97, -4.00, -4.56 },
          { 0.00, 0.12, -1.32, -2.72, -3.91, -5.08 },
          { 0.00, 0.50, -0.04, -1.00, -2.13, -3.36 } },
        { { 0.00, 0.44, 0.98, 2.91, 5.19, 7.81 },
          { 0.00, 0.69, 2.49, 5.14, 6.58, 8.29 },
          { 0.00, 0.80, 2.94, 6.17, 8.15, 9.59 },
          { 0.00, 1.27, 4.03, 7.47, 8.61, 8.00 } } };
      int di = 0; while (di < DT_DRIVE_N - 2 && DN[di + 1] < drive) ++di;
      const double df = std::clamp((drive - DN[di]) / (DN[di + 1] - DN[di]), 0.0, 1.0);
      const double rr = std::min(std::max(resonance, 0.0), 1.0);
      int ri = 0; while (ri < DT_RES_N - 2 && RN[ri + 1] < rr) ++ri;
      const double rf = (rr - RN[ri]) / (RN[ri + 1] - RN[ri]);
      for (int m = 0; m < 2; ++m)
      {
        const double a = DT[m][ri][di]     + df * (DT[m][ri][di + 1]     - DT[m][ri][di]);
        const double b = DT[m][ri + 1][di] + df * (DT[m][ri + 1][di + 1] - DT[m][ri + 1][di]);
        driveTrim[m] = pow(10.0, (a + rf * (b - a)) / 20.0);
      }
    }
  }

  INLINE void DiodeLadderFilter::updateTapMix(double rho)
  {
    // taps j = un, lp1, lp2, lp3, lp4 as P[j](p) lp4 + c[j] w; P[j][d] is the p^d coefficient, degree 4 - j
    const double lam = loadKR;
    double P[5][5] = {}, c[5] = {};
    P[4][0] = 1.0;
    P[3][0] = 2.0 + 2.0 * lam; P[3][1] = 2.0; c[3] = -2.0 * lam;   // lp3 = 2 (p + 1 + lam) lp4 - 2 lam w
    // tap j = m (p + 1) tap(j + 1) - tap(j + 2), with (p + 1) w = rho lp4 + (1 - rho) w
    auto next = [&](int j, double m)
    {
      for (int d = 4; d >= 0; --d)
        P[j][d] = m * (P[j + 1][d] + (d > 0 ? P[j + 1][d - 1] : 0.0)) - P[j + 2][d];
      P[j][0] += m * c[j + 1] * rho;
      c[j] = m * c[j + 1] * (1.0 - rho) - c[j + 2];
    };
    next(2, 2.0);   // lp2 = 2 (p + 1) lp3 - lp4
    next(1, 2.0);   // lp1 = 2 (p + 1) lp2 - lp3
    next(0, 1.0);   // un  = (p + 1) lp1 - lp2
    for (int k = 0; k < 5; ++k)
    {
      tapW[k] = 0.0;
      for (int j = 0; j < 5; ++j)
      {
        const int d = 4 - j;
        double acc = (d == k) ? 1.0 : 0.0;
        for (int i = 0; i < j; ++i)
          acc -= tapMix[k][i] * P[i][d];
        tapMix[k][j] = acc / P[j][d];
        tapW[k] += tapMix[k][j] * c[j];
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
    if (!std::isfinite(z1 + z2 + z3 + z4 + loadState))
      z1 = z2 = z3 = z4 = loadState = loadW = 0.0;

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

    // Calculate feedback signals (S4 -> S3 -> S2 -> S1)
    double S4 = beta4 * (z4 + loadSGain * loadState);
    double S3 = beta3 * (z3 + S4 * delta3);
    double S2 = beta2 * (z2 + S3 * delta2);
    double S1 = beta1 * (z1 + S2 * delta1);

    // SIGMA - weighted sum of feedback signals
    double SIGMA = SG1 * S1 + SG2 * S2 + SG3 * S3 + SG4 * S4;

    // Form input to the ladder (with feedback). The coupling network is linear, so the cascade's instantaneous
    // response is y = netG * x + netS, with x = SIGMA + GAMMA*g*un the ladder output estimate: solve for un exactly,
    // then advance the cascade.
    double netG = 1.0, netS = 0.0;
    for (int i = 0; i < NET_N; ++i)
    {
      const double a = netA[i], c = 1.0 - a;
      const double gx = (i < NET_N - 1) ? c : c + shelfRatio * a;   // highpass, or (s+z)/(s+p) shelf
      const double gs = (i < NET_N - 1) ? -c : (shelfRatio - 1.0) * c;
      netG *= gx;
      netS = gx * netS + gs * netState[i];
    }
    double un = (input - K * (netG * SIGMA + netS)) / (1.0 + K * netG * GAMMA * driveLoopGain);
    double x = SIGMA + GAMMA * driveLoopGain * un;
    for (int i = 0; i < NET_N; ++i)
    {
      const double v = (x - netState[i]) * netA[i];
      const double lp = v + netState[i];
      netState[i] = lp + v;
      x = (i < NET_N - 1) ? x - lp : x - lp + shelfRatio * lp;
    }

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

    // 1st stage
    double xin = un * gamma1 + S2 + epsilon1 * S1;
    double v = (a1 * xin - z1) * alphaFM;
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
    // (with the load lowpass at this sample's lp4; exact without Filter FM)
    loadW = loadA * (G4 * lp3 + S4) + (1.0 - loadA) * loadState;
    v = ((a4 * lp3 + loadKR * loadW) / (1.0 + loadKR) - z4) * alpha4 * fmMod;
    double lp4 = v + z4;
    z4 = lp4 + v;

    // Q21 load lowpass
    loadW = loadA * lp4 + (1.0 - loadA) * loadState;
    loadState = 2.0 * loadW - loadState;

    // Multimode output: the mode morphs LP -> BP -> HP along numerators with only real zeros, so no notch appears
    // on the way (a linear crossfade of 1 and p^2 has zeros on the frequency axis). With t the position within a
    // half and c0, c1, c2 the square roots of the LP / BP / HP output gains:
    //   LP -> BP:  ((1 - t) c0 + t c1 p)^2     = (1-t)^2 c0^2 * 1   + 2 t (1-t) c0 c1 * p   + t^2 c1^2 * p^2
    //   BP -> HP:  p^2 ((1 - t) c1 + t c2 p)^2 = (1-t)^2 c1^2 * p^2 + 2 t (1-t) c1 c2 * p^3 + t^2 c2^2 * p^4
    // Each p^k is a tap mix (tapMix, tapW). Bass comp: below cutoff the loop's passband gain is about 1 / (1 + K), so
    // (1 + K)^amount restores that fraction (in dB) of the resonance bass loss in the LP term; 0 = the real circuit.
    const double taps[5] = { un, lp1, lp2, lp3, lp4 };
    auto pk = [&](int k) { const double* w = tapMix[k]; return w[0] * taps[0] + w[1] * taps[1] + w[2] * taps[2] + w[3] * taps[3] + w[4] * taps[4] - tapW[k] * loadW; };
    const double bassComp = BASS_COMP_MAX * std::clamp(passbandCompensation, 0.0, 1.0);
    const double gLP = driveTrim[0] * (bassComp > 0.0 ? std::exp(bassComp * logLoopGain) : 1.0);
    double out;
    if (mode <= 0.0)
      out = gLP * lp4;
    else
    {
      const double gBP = BP_GAIN * driveTrim[1];
      const double lo = mode < 0.5 ? std::sqrt(gLP) : std::sqrt(gBP);
      const double hi = mode < 0.5 ? std::sqrt(gBP) : std::sqrt(HP_GAIN * driveTrim[1]);
      const int k = mode < 0.5 ? 0 : 2;
      const double t = mode < 0.5 ? 2.0 * mode : 2.0 * mode - 1.0;
      const double a = (1.0 - t) * lo, b = t * hi;
      // the weights sum to (1-t)^2 + t^2 (0.5 at the midpoint); normalising keeps the in-between level up
      out = (a * a * pk(k) + 2.0 * a * b * pk(k + 1) + b * b * pk(k + 2)) / ((1.0 - t) * (1.0 - t) + t * t);
    }
    return out;
  }

} // end namespace dfl

#endif // dfl_DiodeLadderFilter_h
