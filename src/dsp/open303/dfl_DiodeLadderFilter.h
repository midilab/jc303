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
   * The 1st pole sits one octave above (TB-303 style ~18 dB/oct), with the real circuit's feedback
   * coupling network and resonance-pot law, calibrated to a SPICE model of the TB-303 VCF (see calculateCoefficients).
   * The BP / HP responses mix the same ladder's stage taps.
   */

  class DiodeLadderFilter
  {

  public:

    /** Filter response mode. The ladder's per-stage lowpass taps are mixed
        (Oberheim style) to synthesise band- and high-pass responses from the
        same 4-pole core - identical coefficients to rosic::TeeBeeFilter. */
    enum ResponseMode
    {
      RESPONSE_LP = 0,  // lowpass - the ladder output
      RESPONSE_BP,      // bandpass
      RESPONSE_HP       // highpass
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
        the drive (none at or below 0 dB), so the clean region is unaffected. */
    void setSaturationBias(double newBias);

    /** Sets the morph amount (0..1) for the BP and HP responses: 0 = pure bandpass / highpass,
        1 = pure lowpass, crossfading linearly. In the LP response it is the bass comp amount (see getSample). */
    void setPassbandCompensation(double newCompensation) { passbandCompensation = newCompensation; }

    /** Sets the Filter Feedback corner (Hz): the real circuit's coupling network fits a one-pole highpass of
        ~115 Hz, and the knob scales the network's dominant pole by newCutoff / 115 (see NET_POLE). Bleeds
        resonance off as the cutoff falls into the bass. */
    void setFeedbackHighpass(double newCutoff) { feedbackHpFreq = newCutoff; calculateCoefficients(); }

    /** Sets the unit trim: a loop-gain scale, TRIM_MIN..TRIM_MAX (1 = the nominal unit).
        Component tolerances make real units differ mostly by such a scale (spice/mc_native.py). Above 1 the
        oscillation cap rises with it, so a hot unit self-oscillates at high resonance as some real units do. */
    void setLoopGainTrim(double newTrim)
    {
      loopGainTrim = std::clamp(newTrim, TRIM_MIN, TRIM_MAX);
      calculateCoefficients();
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

    /** The resonance pot's rotation, 0..1 (the knob with Open303's skew undone). */
    double getResonancePot() const { return -std::log(1.0 - std::clamp(resonance, 0.0, 1.0) * RES_SKEW_NORM) / 3.0; }

    /** The dominant coupling pole (Hz), C27/C25 into Q21, as scaled by the Filter Feedback knob. It sits
        in the output path as well as the feedback loop (see TB303OutputNetwork). */
    double getCouplingPoleHz() const { return NET_POLE[0] / (2.0 * PI) * feedbackHpFreq / 115.0; }

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

    // Highpass DC-null factor (see RESPONSE_HP in getSample). The textbook binomial
    // highpass mix un-4lp1+6lp2-4lp3+lp4 assumes each tap is a unity-gain cascade
    // power (lp_i = un*G^i); this Pirkle diode ladder's a2..a4 = 0.5 stage scaling
    // breaks that, so the low end fails to cancel and leaks a large (~+11 dB) boost.
    // Scaling the lowpass-reconstruction term by this factor nulls the DC exactly.
    // It is fixed by the a-coefficients (every stage has unity gain at DC), so it is a constant.
    static constexpr double HP_LP_SUBTRACT = 0.2;  // = 1/5, DC-null for the HP mix

    // Static BP / HP output gains (on top of OUTPUT_GAIN for BP) so they sit near LP loudness on a saw at a
    // typical resonant 303 setting (resonance 50..70, cutoff 800..1500 Hz: BP -0.5 dB, HP -0.6 dB, see morph_level).
    // No correction by cutoff: the BP/HP-vs-LP gap at low resonance and high cutoff is the filter's character (a saw
    // has most of its energy at the low harmonics that BP/HP remove).
    static constexpr double HP_OUTPUT_GAIN = 0.7063;
    static constexpr double BP_OUTPUT_GAIN = 2.8251;

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
    double passbandCompensation;  // LP: bass comp amount; BP/HP: morph amount (0 = pure BP/HP, 1 = pure LP)
    double resonance;             // resonance parameter (0-1, pre-skewed by Open303)
    double sampleRate;
    int    responseMode;          // LP/BP/HP output mixing (@see ResponseMode)

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
    // Cutoff: the cutoff parameter is the circuit's ladder-core 180-degree frequency, 1.18921 x its
    // corner wc (Stinchcombe's core polynomial s^4 + 6.727 s^3 + 14.142 s^2 + 9.514 s + 1), the frequency the
    // resonant peak approaches at high cutoff. The Pirkle ladder's corner is 0.7006 x its cutoff coefficient
    // (fitted to the core polynomial, 0.4 dB / 3 degrees up to 8x cutoff), so tuning = 1 / (1.18921 * 0.7006).
    static constexpr double CUTOFF_TUNING = 1.2003;
    // Output gain: static, chosen so the resonance-0 level equals the TeeBee's above the bass. The output
    // network (TB303OutputNetwork, in Open303) cuts more bass than Open303's chain, so low notes are quieter (C2 about
    // -4 dB). Like the real circuit the ladder's passband drops as resonance rises; the network's wiper tap makes up
    // about half of it.
    static constexpr double OUTPUT_GAIN = 2.1056;
    // Oscillation cap, as a fraction of the interpolated threshold KNC. 0.98 rather than 0.99: between the
    // 1 and 2 kHz nodes at Filter Feedback 1.0 the threshold sags below the interpolation (K / threshold reached
    // 0.9956 at 0.99 on a dense 20 x 20 check); it binds only near that knob extreme.
    static constexpr double K_MARGIN = 0.98;
    // Bass comp knob range (amount at knob 1.0). Full-synth level vs the TeeBee at resonance 60..100
    // (mean of cutoff 400 / 1000 / 2500, env mod 60, C2), with the output network's resonance-pot wiper tap already
    // making up about half the loss: amount 0 is 1.4-2.7 dB quieter, 0.1 (knob 0.5) matches within 1.1 dB, 0.2 is
    // 1.7-3.6 dB louder.
    static constexpr double BASS_COMP_MAX = 0.2;
    // Unit trim range: about the 5th..95th percentile of the per-unit loop-gain scale in the component
    // tolerance Monte Carlo (x0.85..x1.16 at 500 Hz, x0.90..x1.12 above). At x1.15 a unit at full pot sits up to
    // 1.15 x the oscillation threshold and sustains a sine 5-11 dB below a resonance-0 saw.
    static constexpr double TRIM_MIN = 0.85;
    static constexpr double TRIM_MAX = 1.15;
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
    // Bilinear transform calculations (cutoff tuned to the circuit's ladder core, see CUTOFF_TUNING)
    double tunedCutoff = cutoff * CUTOFF_TUNING;
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

    // Feedback factor. Drive is applied through driveLoopGain (drive-invariance: K is divided by the drive stage's
    // small-signal gain, so the loop gain does not depend on drive).
    // The resonance knob is the real pot's rotation (linear 50k, loaded), so undo Open303's skew.
    // The loop gain K(pot, cutoff) is solved offline so the resonant boost equals the SPICE model of the real VCF
    // (tools/diode-fidelity/gen_native_tables.py: spice/native_targets.py + native_solver, 0.8 dB rms over the curves);
    // KNC is the measured self-oscillation threshold (growth test, native_solver) at the Filter Feedback nodes
    // 57.5 / 115 / 268 Hz (knob 1.0 / default / 0; Open303 maps the knob). The knob stops at Q21 beta about 500:
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
      { 0, 2.952, 6.701, 8.531, 11.899, 13.695, 15.977 },
      { 0, 2.947, 6.254, 9.021, 12.297, 14.386, 16.718 },
      { 0, 3.180, 6.236, 9.359, 12.750, 14.664, 17.123 },
      { 0, 3.463, 6.484, 9.255, 12.806, 14.905, 17.307 },
      { 0, 3.525, 6.412, 9.479, 12.993, 15.171, 17.646 },
      { 0, 3.652, 6.333, 9.466, 13.112, 15.249, 17.778 },
      { 0, 3.647, 6.360, 9.461, 13.196, 15.302, 17.871 },
      { 0, 3.537, 6.332, 9.498, 13.121, 15.330, 17.855 },
      { 0, 3.328, 6.321, 9.437, 13.103, 15.309, 17.900 },
      { 0, 3.027, 6.266, 9.399, 13.085, 15.333, 17.987 },
      { 0, 2.643, 6.208, 9.394, 13.113, 15.434, 18.060 } };
    static const double KNC[3][11] = {
      { 24.030, 21.548, 20.204, 19.763, 18.896, 18.612, 18.397, 18.305, 18.324, 18.449, 18.705 },
      { 28.858, 24.252, 21.824, 21.041, 19.515, 19.023, 18.640, 18.455, 18.423, 18.524, 18.765 },
      { 43.817, 32.250, 26.466, 24.656, 21.223, 20.136, 19.296, 18.859, 18.688, 18.719, 18.920 } };
    const double pot = getResonancePot();
    double lc = std::log(std::clamp(cutoff, CN[0], CN[10]));
    int ci = 0; while (ci < 9 && std::log(CN[ci + 1]) < lc) ++ci;
    const double cf = (lc - std::log(CN[ci])) / (std::log(CN[ci + 1]) - std::log(CN[ci]));
    int ai = 0; while (ai < 5 && AN[ai + 1] < pot) ++ai;
    const double af = std::clamp((pot - AN[ai]) / (AN[ai + 1] - AN[ai]), 0.0, 1.0);
    auto at = [&](int c) { return KNT[c][ai] + af * (KNT[c][ai + 1] - KNT[c][ai]); };
    double effK = loopGainTrim * (at(ci) + cf * (at(ci + 1) - at(ci)));
    static const double FN[3] = { 57.5, 115.0, 268.0 };
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
        { { 0.00, 0.36, 0.51, 2.08, 4.07, 6.33 },
          { 0.00, 0.45, 1.01, 2.22, 2.83, 3.92 },
          { 0.00, 0.61, 1.85, 4.09, 5.25, 5.79 },
          { 0.00, 1.22, 3.61, 6.70, 7.72, 7.17 } } };
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

    // 1st stage (one octave above, TB-303 style slope)
    double xin = un * gamma1 + S2 + epsilon1 * S1;
    double v = (a1 * xin - z1) * alpha2FM;
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
    // synthesise band- and high-pass responses from the same core.
    //   LP:  lp4
    //   BP:  lp2 - 2*lp3 + lp4      (0.25 scale)
    //   HP:  un - 0.2*(4*lp1 - 6*lp2 + 4*lp3 - lp4)
    // where `un` is the (shaped) signal entering the ladder - the y0 term. The HP
    // uses the DC-null factor HP_LP_SUBTRACT (see there): the plain binomial mix
    // leaks the low end in this diode topology, so the lowpass-reconstruction term
    // is scaled to cancel DC.
    // In BP / HP the knob (passbandCompensation) crossfades to LP: 0 = pure bandpass / highpass, 1 = pure lowpass
    // (the HP midpoint is a notch). The LP end has the LP level without bass comp, so each end of a morph has the
    // level of its pure mode. Each term is scaled by the drive level table of its response (driveTrim).
    const double lpGain = OUTPUT_GAIN * driveTrim[0];
    double out;
    switch (responseMode)
    {
      case RESPONSE_BP:
      {
        double bp = OUTPUT_GAIN * BP_OUTPUT_GAIN * 0.25 * (lp2 - 2.0 * lp3 + lp4);
        double t  = std::clamp(passbandCompensation, 0.0, 1.0);
        out = (1.0 - t) * driveTrim[1] * bp + t * lpGain * lp4;
        break;
      }
      case RESPONSE_HP:
      {
        double hp = HP_OUTPUT_GAIN * (un - HP_LP_SUBTRACT * (4.0 * lp1 - 6.0 * lp2 + 4.0 * lp3 - lp4));
        double t  = std::clamp(passbandCompensation, 0.0, 1.0);
        out = (1.0 - t) * driveTrim[1] * hp + t * lpGain * lp4;
        break;
      }
      case RESPONSE_LP:
      default:
      {
        // Bass comp: below cutoff the loop's passband gain is about 1 / (1 + K), so (1 + K)^amount restores that
        // fraction (in dB) of the resonance bass loss; 0 = the real circuit, and nothing changes at resonance 0.
        const double bassComp = BASS_COMP_MAX * std::clamp(passbandCompensation, 0.0, 1.0);
        out = lpGain * (bassComp > 0.0 ? std::exp(bassComp * logLoopGain) : 1.0) * lp4;
        break;
      }
    }
    return out;
  }

} // end namespace dfl

#endif // dfl_DiodeLadderFilter_h
