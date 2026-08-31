#ifndef dfl_DiodeLadderFilter_h
#define dfl_DiodeLadderFilter_h

// standard-library includes:
#include <stdlib.h>
#include <cmath>
#include <algorithm>   // math_approx.hpp uses std::max/std::min unqualified
#include <utility>     // math_approx.hpp uses std::make_pair

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

    /** Sets the passband gain compensation amount (0.0 = none, 1.0 = full compensation). */
    void setPassbandCompensation(double newCompensation) { passbandCompensation = newCompensation; }

    /** Sets whether the first pole is one octave above (TB-303 style ~18dB/oct slope). */
    INLINE void setOctaveMode(bool enabled)
    {
      if (enabled != octaveMode)
      {
        octaveMode = enabled;
        calculateCoefficients();  // octave mode caps the resonance (see K below)
      }
    }

    /** Sets the response mode (LP/BP/HP). This only re-mixes the ladder's stage
        outputs, so it needs no coefficient recalculation. @see ResponseMode */
    INLINE void setResponseMode(int newMode) { responseMode = newMode; }

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

    // Scaling factors for each stage (a0 values from Pirkle)
    static constexpr double a1 = 1.0;
    static constexpr double a2 = 0.5;
    static constexpr double a3 = 0.5;
    static constexpr double a4 = 0.5;

    // Headroom for the input drive stage: scale down before the tanh and back
    // up after, so 0 dB drive stays clean and the curve is reserved for when
    // the signal is actually driven. 0.5 = 6 dB headroom (matches DB303).
    static constexpr double headroomBias = 0.5;

    // Exponent for the drive level-makeup (applied as 1/driveFactor^exp). 0 = no
    // makeup (drive boosts level), 1 = full makeup (drive cuts level at real
    // levels). 0.5 keeps the level within ~+-2-3 dB across drive and resonance.
    static constexpr double DRIVE_MAKEUP_EXP = 0.5;

    // Octave-mode feedback scale (fraction of the plain-diode self-oscillation
    // point K = 17), calibrated at 1 kHz. 0.85 matches the TeeBee's resonant Q
    // there across the whole knob range. Plain Diode ignores this and reaches K = 17.
    static constexpr double OCTAVE_RESONANCE_CEILING = 0.85;

    // Resonance tracking (octave only). The TeeBee's resonant bandwidth is ~constant,
    // so its Q rises with cutoff; scale the octave feedback by (cutoff/1kHz)^exp to
    // track it. The knee + tracking are applied to the effective resonance (before
    // driveMakeup); drive coupling (see OCTAVE_RES_DRIVE_*) is applied here too.
    // At high cutoff the effective resonance would exceed the octave's self-
    // oscillation onset (~17.8), so a soft knee saturates it smoothly toward K_CEIL:
    // transparent below K_KNEE (full resonance through the musical range), asymptoting
    // to K_CEIL up top so the chirp bites like the TeeBee without self-oscillating.
    static constexpr double OCTAVE_RES_TRACK_EXP = 0.22;
    static constexpr double OCTAVE_RES_TRACK_REF = 1000.0;  // Hz where scaling = 1
    static constexpr double OCTAVE_K_KNEE        = 14.3;    // soft-knee onset (effective)
    static constexpr double OCTAVE_K_CEIL        = 17.5;    // asymptote, < 17.8 self-osc

    // Drive->resonance coupling (octave only). By default resonance is fully drive-
    // invariant; above the filterDrive=0.5 calibration point we let harder drive push
    // the effective resonance further up the soft-knee toward self-oscillation. The
    // boost is FLOORED at 1.0 at/below the reference, so lowering drive below 0.5 never
    // weakens resonance - it only cleans up the saturation stage (which still tracks
    // driveFactor), leaving the lower half of the knob as a pure timbre sweep.
    // REF is the linear driveFactor at knob 0.5: dB2amp(4.5), where filterDrive 0..1
    // maps to 0..9 dB (see FILTER_DRIVE in JC303.cpp). EXP sets the coupling strength.
    static constexpr double OCTAVE_RES_DRIVE_REF = 1.6788;  // = dB2amp(4.5)
    static constexpr double OCTAVE_RES_DRIVE_EXP = 0.5;     // 0 = no coupling (invariant)

    // Cutoff tuning: the diode ladder's resonant frequency sits below the nominal
    // cutoff (~0.71x 4-pole, ~0.79x octave), so scale the cutoff up before
    // computing coefficients to land the resonant peak on the nominal frequency
    // (matching the TeeBee, whose peak tracks its cutoff 1:1).
    static constexpr double CUTOFF_TUNING        = 1.41;  // 4-pole (~1/0.71)
    static constexpr double CUTOFF_TUNING_OCTAVE = 1.33;  // octave (peak aligned to TeeBee)

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
    double driveMakeup;           // precomputed 1/driveFactor^DRIVE_MAKEUP_EXP
    double passbandCompensation;  // 0.0 = no compensation, 1.0 = full (1+K) boost
    double resonance;             // resonance parameter (0-1, pre-skewed by Open303)
    double sampleRate;
    bool   octaveMode;            // true = 1st pole one octave above (TB-303 style)
    int    responseMode;          // LP/BP/HP output mixing (@see ResponseMode)
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
    double tunedCutoff = cutoff * (octaveMode ? CUTOFF_TUNING_OCTAVE : CUTOFF_TUNING);
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

    // Feedback factor; K = 17 is the diode-ladder self-oscillation point. Plain
    // Diode reaches it at full resonance. Octave mode is capped at
    // OCTAVE_RESONANCE_CEILING, scaled by driveMakeup (drive-invariance: drive
    // raises the loop's small-signal gain by driveFactor^DRIVE_MAKEUP_EXP, which
    // driveMakeup cancels), and scaled by resonance tracking so its Q rises with
    // cutoff like the TeeBee.
    if (octaveMode)
    {
      double resTrack = pow(cutoff / OCTAVE_RES_TRACK_REF, OCTAVE_RES_TRACK_EXP);
      // Effective resonance (no driveMakeup yet), soft-knee-limited, then converted to
      // the actual feedback coefficient via driveMakeup. Because physical resonance =
      // K * loop-gain = effK, the soft knee below bounds it at OCTAVE_K_CEIL regardless
      // of drive, so the coupling below can push toward self-osc without running away.
      double effK = 17.0 * resonance * OCTAVE_RESONANCE_CEILING * resTrack;
      // Drive above the calibration reference pushes resonance up the knee; floored at
      // 1.0 below it so lower drive leaves resonance at the 0.5-calibrated value.
      effK *= pow(std::max(driveFactor, OCTAVE_RES_DRIVE_REF) / OCTAVE_RES_DRIVE_REF,
                  OCTAVE_RES_DRIVE_EXP);
      if (effK > OCTAVE_K_KNEE)
      {
        double span = OCTAVE_K_CEIL - OCTAVE_K_KNEE;
        effK = OCTAVE_K_KNEE + span * tanh((effK - OCTAVE_K_KNEE) / span);
      }
      K = effK * driveMakeup;
    }
    else
    {
      K = 17.0 * resonance;
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

    // Calculate feedback signals (S4 -> S3 -> S2 -> S1)
    double S4 = beta4 * z4;
    double S3 = beta3 * (z3 + S4 * delta3);
    double S2 = beta2 * (z2 + S3 * delta2);
    double S1 = beta1 * (z1 + S2 * delta1);

    // SIGMA - weighted sum of feedback signals
    double SIGMA = SG1 * S1 + SG2 * S2 + SG3 * S3 + SG4 * S4;

    // Form input to the ladder (with feedback)
    double un = (input - K * SIGMA) / (1.0 + K * GAMMA);

    // Apply input nonlinearity with headroom scaling: scale down before the
    // tanh and back up after, so 0 dB drive (driveFactor == 1) stays clean
    // and turning up the drive pushes the signal into saturation. Matches the
    // DB303 shaper so the 0..9 dB range feels the same.
    // driveMakeup is the level compensation (see setInputDrive): without it the
    // pre-tanh driveFactor gain doubles as a volume boost, so the drive knob
    // acted as a loudness control. It keeps drive roughly level-neutral while the
    // tanh still saturates peaks - so drive changes timbre/resonance-compression,
    // not overall level.
    un = shape(driveFactor * un * headroomBias) * driveMakeup / headroomBias;

    // 1st stage (optionally one octave above for TB-303 style slope)
    double xin = un * gamma1 + S2 + epsilon1 * S1;
    double v = (a1 * xin - z1) * (octaveMode ? alpha2 : alpha);
    double lp1 = v + z1;
    z1 = lp1 + v;

    // 2nd stage
    xin = lp1 * gamma2 + S3 + epsilon2 * S2;
    v = (a2 * xin - z2) * alpha;
    double lp2 = v + z2;
    z2 = lp2 + v;

    // 3rd stage
    xin = lp2 * gamma3 + S4 + epsilon3 * S3;
    v = (a3 * xin - z3) * alpha;
    double lp3 = v + z3;
    z3 = lp3 + v;

    // 4th stage
    v = (a4 * lp3 - z4) * alpha;
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
        out = 0.25 * (lp2 - 2.0 * lp3 + lp4);
        break;
      case RESPONSE_HP:
      {
        // HP mode repurposes the (bass-comp-inert) passbandCompensation knob as a
        // HP->LP morph: 0 = pure highpass, 1 = pure lowpass, crossfading linearly.
        // The midpoint is a notch (highs from HP + lows from LP). Both endpoints
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
                                                : (1.0 + passbandCompensation * K);
    return out * comp;
  }

} // end namespace dfl

#endif // dfl_DiodeLadderFilter_h
