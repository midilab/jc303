#ifndef dfl_DiodeLadderFilter_h
#define dfl_DiodeLadderFilter_h

// standard-library includes:
#include <stdlib.h>
#include <cmath>
#include <algorithm>

// rosic includes:
#include "rosic_RealFunctions.h"

namespace dfl
{

  /**
   * 4-pole diode ladder filter based on Will Pirkle's TPT analysis.
   * http://www.willpirkle.com/Downloads/AN-6DiodeLadderFilter.pdf
   *
   * This is an alternative to TeeBeeFilter with a different character.
   * The diode ladder has asymmetric clipping characteristics and a
   * different resonance behavior compared to the transistor ladder.
   *
   * TB-303 Nonlinearity Structure:
   * ------------------------------
   * Three nonlinearity points model the real diode ladder behavior:
   *
   * 1. diodeShape() - Input saturation
   *    - Asymmetric: positive side saturates via exp(), negative ~linear
   *    - Produces even harmonics, dynamic cutoff shift under drive
   *    - Models the diode input pair asymmetry
   *
   * 2. fbShape() - Feedback path saturation (applied to SIGMA)
   *    - Gentler than input, causes resonance collapse at high levels
   *    - Creates the characteristic "chirp" on resonant sweeps
   *    - Models the feedback diode clipping behavior
   *
   * 3. stageNL() - Per-stage asymmetry (additive on each LP output)
   *    - Very small (~1.5%) cubic nonlinearity
   *    - Adds grit and "hollowness" under strong resonance
   *    - Models diode pair influence on each integrator stage
   *
   * All nonlinearities are monotonic and preserve TPT stability.
   * They appear before the implicit solver, not inside the feedback loop.
   *
   * Signal Flow:
   * ------------
   *  Input ──┬───────────────────────────────────────────────────┐
   *          │                                                   │
   *          │  ┌─────────────────────────────────────────────┐  │
   *          │  │              FEEDBACK PATH                  │  │
   *          │  │  SIGMA = SG1*S1 + SG2*S2 + SG3*S3 + SG4*S4 │  │
   *          │  │  SIGMA = fbShape(SIGMA)  <── (2) FB shaper  │  │
   *          │  └─────────────────────────────────────────────┘  │
   *          │                      │                            │
   *          v                      v                            │
   *       ┌──────────────────────────────────────┐               │
   *       │  un = (input - K*SIGMA)/(1+K*GAMMA)  │               │
   *       │  un = diodeShape(drive*un)  <── (1) Input shaper    │
   *       └──────────────────────────────────────┘               │
   *                         │                                    │
   *          ┌──────────────┼──────────────┬──────────────┐      │
   *          v              v              v              v      │
   *       ┌─────┐       ┌─────┐        ┌─────┐        ┌─────┐   │
   *       │ LP1 │──────>│ LP2 │───────>│ LP3 │───────>│ LP4 │───┘
   *       │+stageNL     │+stageNL      │+stageNL      │+stageNL
   *       └─────┘       └─────┘        └─────┘        └─────┘
   *          (3)           (3)            (3)            (3)
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

    /** Sets the passband gain compensation amount (0.0 = none, 1.0 = full compensation). */
    void setPassbandCompensation(double newCompensation) { passbandCompensation = newCompensation; }

    /** Sets whether the first pole is one octave above (TB-303 style ~18dB/oct slope). */
    void setOctaveMode(bool enabled) { octaveMode = enabled; }

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

    //---------------------------------------------------------------------------------------------
    // audio processing:

    /** Calculates one output sample at a time. */
    INLINE double getSample(double in);

    //---------------------------------------------------------------------------------------------
    // others:

    /** Causes the filter to re-calculate the coefficients. */
    INLINE void calculateCoefficients();

    /** Asymmetric diode-like waveshaping nonlinearity (input stage). */
    INLINE double diodeShape(double x);

    /** Feedback path saturation (resonance collapse / chirp). */
    INLINE double fbShape(double x);

    /** Small asymmetric nonlinearity for stage outputs (diode pair influence). */
    INLINE double stageNL(double x, double threshold, double amount);

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

    // Filter parameters
    double cutoff;
    double drive;
    double driveFactor;
    double passbandCompensation;  // 0.0 = no compensation, 1.0 = full (1+K) boost
    double resonance;             // resonance parameter (0-1, pre-skewed by Open303)
    double sampleRate;
    bool   octaveMode;            // true = 1st pole one octave above (TB-303 style)

    // Diode nonlinearity parameters
    static constexpr double diodeSharp = 4.5;   // steepness of diode conduction
    static constexpr double diodeKnee  = 0.10;  // diode knee voltage
    static constexpr double nlAtten    = 0.2;   // input attenuation before NL (headroom for drive)

    // DC blocker state and coefficient
    double dcPrev, dcOut;
    double dcR;  // coefficient: exp(-2*PI*fc/sampleRate), fc ~5Hz

    // Filter FM (Devilfish mod) - audio-rate cutoff modulation from input
    // AC-coupled input signal modulates filter cutoff frequency
    double filterFmDepth;                         // User parameter: 0 (off) to 1 (full)
    double acCouplingState;                       // One-pole HPF state for AC coupling
    static constexpr double acCouplingFreq = 20.0; // AC coupling corner frequency (Hz)
    static constexpr double filterFmScale = 0.4;  // Scale factor for FM modulation depth
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
    // DC blocker coefficient: ~5Hz cutoff
    dcR = exp(-2.0 * PI * 5.0 / sampleRate);

    // Bilinear transform calculations
    double wd = 2.0 * PI * cutoff;
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

    // Alpha for 1st stage (one octave above, like TB-303)
    double wa2 = (2.0 / T) * tan(2*wd * T / 2.0);
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

    // K maps resonance 0-1 to approximately 0-17 for self-oscillation
    // (17 is the theoretical self-oscillation point for diode ladder)
    K = 17.0 * resonance;
  }

  INLINE double DiodeLadderFilter::diodeShape(double x)
  {
    // Asymmetric diode-like waveshaper
    // Negative side: mostly linear (diode reverse bias)
    if (x < 0.0)
      return 0.98 * x;

    // Positive side: exponential diode knee - saturates to ~1.0
    // Small DC offset is intentional (diode forward voltage)
    return 1.0 - exp(-diodeSharp * (x + diodeKnee));
  }

  INLINE double DiodeLadderFilter::fbShape(double x)
  {
    // Feedback saturation - gentler than input, causes resonance collapse/chirp
    // Asymmetric: positive saturates, negative mostly linear
    if (x >= 0.0)
      return 1.0 - exp(-3.0 * x);  // softer saturation than input
    else
      return 0.9 * x;  // linear on negative side
  }

  INLINE double DiodeLadderFilter::stageNL(double x, double threshold, double amount)
  {
    // Per-stage asymmetry - models diode pair influence
    // Soft-clipped cubic: x³/(1+x²) preserves character but can't runaway
    // Each stage has different threshold and amount for variety
    double driveNorm = drive / 9.0;  // 0dB→0, 9dB→1
    double stageAmt = (driveNorm > threshold) ? (driveNorm - threshold) / (1.0 - threshold) : 0.0;
    double x2 = x * x;
    return amount * stageAmt * x * x2 / (1.0 + x2);
  }

  INLINE double DiodeLadderFilter::getSample(double in)
  {
    // Input without compensation - compensation applied at output
    double input = in;

    // -------------------------------------------------------------------------
    // Filter FM (Devilfish mod): AC-coupled input modulates cutoff
    // Uses one-pole HPF for AC coupling (~20Hz corner)
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

    double alphaFM = alpha * fmMod;
    double alpha2FM = alpha2 * fmMod;

    // Calculate feedback signals (S4 -> S3 -> S2 -> S1)
    double S4 = beta4 * z4;
    double S3 = beta3 * (z3 + S4 * delta3);
    double S2 = beta2 * (z2 + S3 * delta2);
    double S1 = beta1 * (z1 + S2 * delta1);

    // SIGMA - weighted sum of feedback signals
    double SIGMA = SG1 * S1 + SG2 * S2 + SG3 * S3 + SG4 * S4;

    // TB-303 style feedback saturation (resonance collapse / chirp)
    SIGMA = fbShape(SIGMA);

    // Form input to the ladder (with feedback)
    double un = (input - K * SIGMA) / (1.0 + K * GAMMA);

    // Apply input nonlinearity (diode saturation)
    // nlAtten provides headroom - inputDrive pushes into saturation
    un = diodeShape(nlAtten * driveFactor * un);

    // 1st stage (optionally one octave above for TB-303 style slope)
    double xin = un * gamma1 + S2 + epsilon1 * S1;
    double v = (a1 * xin - z1) * (octaveMode ? alpha2FM : alphaFM);
    double lp1 = v + z1;
    z1 = lp1 + v;
    lp1 = lp1 + stageNL(lp1, 0.1, 0.02);  // kicks in early, subtle

    // 2nd stage
    xin = lp1 * gamma2 + S3 + epsilon2 * S2;
    v = (a2 * xin - z2) * alphaFM;
    double lp2 = v + z2;
    z2 = lp2 + v;
    lp2 = lp2 + stageNL(lp2, 0.25, 0.03);  // medium threshold

    // 3rd stage
    xin = lp2 * gamma3 + S4 + epsilon3 * S3;
    v = (a3 * xin - z3) * alphaFM;
    double lp3 = v + z3;
    z3 = lp3 + v;
    lp3 = lp3 + stageNL(lp3, 0.4, 0.04);  // higher threshold

    // 4th stage
    v = (a4 * lp3 - z4) * alphaFM;
    double lp4 = v + z4;
    z4 = lp4 + v;
    lp4 = lp4 + stageNL(lp4, 0.55, 0.05);  // kicks in late, strongest

    // Apply passband gain compensation at output
    double out = lp4 * (1.0 + passbandCompensation * K);

    // DC blocker on final output
    dcOut = dcR * (dcOut + out - dcPrev);
    dcPrev = out;

    return dcOut;
  }

} // end namespace dfl

#endif // dfl_DiodeLadderFilter_h
