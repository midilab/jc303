#ifndef rosic_DecayEnvelope_h
#define rosic_DecayEnvelope_h

// rosic-indcludes:
#include "rosic_RealFunctions.h"

namespace rosic
{

  /**

  This is a class implements an envelope generator that realizes a pure exponential decay. The 
  output of the envelope is normalized to the range 0...1.

  */

  class DecayEnvelope
  {

  public:

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. */
    DecayEnvelope();  

    /** Destructor. */
    ~DecayEnvelope(); 

    //---------------------------------------------------------------------------------------------
    // parameter settings:

    /** Sets the sample-rate. */
    void setSampleRate(double newSampleRate);  

    /** Sets the time constant for the multiplicative accumulator (which we consider as primarily 
    responsible for the decaying part) in milliseconds. */
    void setDecayTimeConstant(double newTimeConstant);

    /** Switches into a mode where the normalization is not made with respect to the peak amplitude 
    but to the sum of the impulse response - if true, the output will be equivalent to a leaky 
    integrator's impulse response. */
    void setNormalizeSum(bool shouldNormalizeSum);

    //---------------------------------------------------------------------------------------------
    // inquiry:

    /** Returns the length of the decay phase (in milliseconds). */
    double getDecayTimeConstant() const { return tau; }

    /** True, if output is below some threshold. */
    bool endIsReached(double threshold);  

    //---------------------------------------------------------------------------------------------
    // audio processing:

    /** Calculates one output sample at a time. */
    INLINE double getSample();    

    //---------------------------------------------------------------------------------------------
    // others:

    /** Triggers the envelope - the next sample retrieved via getSample() will be 1. */
    void trigger();

  protected:

    /** Calculates the coefficient for multiplicative accumulation. */
    void calculateCoefficient();

    double c;             // coefficient for multiplicative accumulation
    double y;             // previous output
    double yInit;         // initial yalue for previous output (= y/c)
    double tau;           // time-constant (in milliseconds)
    double fs;            // sample-rate
    bool   normalizeSum;  // flag to indicate that the output should be normalized such that the 
                          // sum of the impulse response is unity (instead of the peak) - if true
                          // the output will be equivalent to a leaky integrator's impulse 
                          // response

  };

  //-----------------------------------------------------------------------------------------------
  // inlined functions:

  INLINE double DecayEnvelope::getSample()
  {
    y *= c;
    return y;
  }

} // end namespace rosic

#endif // rosic_DecayEnvelope_h
