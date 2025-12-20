#ifndef dfl_DiodeLadderFilter_h
#define dfl_DiodeLadderFilter_h

// standard-library includes:
#include <stdlib.h>
#include <cmath>

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

    // Filter parameters
    double cutoff;
    double drive;
    double driveFactor;
    double passbandCompensation;  // 0.0 = no compensation, 1.0 = full (1+K) boost
    double resonance;             // resonance parameter (0-1, pre-skewed by Open303)
    double sampleRate;
    bool   octaveMode;            // true = 1st pole one octave above (TB-303 style)
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

  INLINE double DiodeLadderFilter::shape(double x)
  {
    // Soft clipping - tanh approximation
    return math_approx::tanh<7>(x);
  }

  INLINE double DiodeLadderFilter::getSample(double in)
  {
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

    // Apply input nonlinearity
    un = shape(driveFactor * un);

    // 1st stage (optionally one octave above for TB-303 style slope)
    double xin = un * gamma1 + S2 + epsilon1 * S1;
    double v = (a1 * xin - z1) * (octaveMode ? alpha2 : alpha);
    double lp = v + z1;
    z1 = lp + v;

    // 2nd stage
    xin = lp * gamma2 + S3 + epsilon2 * S2;
    v = (a2 * xin - z2) * alpha;
    lp = v + z2;
    z2 = lp + v;

    // 3rd stage
    xin = lp * gamma3 + S4 + epsilon3 * S3;
    v = (a3 * xin - z3) * alpha;
    lp = v + z3;
    z3 = lp + v;

    // 4th stage
    v = (a4 * lp - z4) * alpha;
    lp = v + z4;
    z4 = lp + v;

    // Oberheim variations
    // return c.mA * dU + c.mB * dLP1 + c.mC * dLP2 + c.mD * dLP3 +  c.mE * dLP4;

    // Apply passband gain compensation at output (keeps saturation independent of compensation)
    return lp * (1.0 + passbandCompensation * K);
  }

} // end namespace dfl

#endif // dfl_DiodeLadderFilter_h
