#ifndef rosic_LeakyIntegrator_h
#define rosic_LeakyIntegrator_h

// rosic-indcludes:
#include "rosic_RealFunctions.h"

namespace rosic
{

  /**

  This is a leaky integrator type lowpass filter with user adjustable time constant which is set 
  up in milliseconds.

  */

  class LeakyIntegrator  
  {

  public:

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. */
    LeakyIntegrator();  

    /** Destructor. */
    ~LeakyIntegrator();  

    //---------------------------------------------------------------------------------------------
    // parameter settings:

    /** Sets the sample-rate. */
    void setSampleRate(double newSampleRate);    

    /** Sets the time constant (tau), value is expected in milliseconds. */
    void setTimeConstant(double newTimeConstant); 

    /** Sets the internal state of the filter to the passed value. */
    void setState(double newState) { y1 = newState; }

    //---------------------------------------------------------------------------------------------
    // inquiry:

    /** Returns the time constant (tau) in milliseconds. */
    double getTimeConstant() const { return tau; }

    /** Returns the normalizer, required to normalize the impulse response of a series connection 
    of two digital RC-type filters with time constants tau1 and tau2 (in milliseconds) to unity at 
    the given samplerate. */
    static double getNormalizer(double tau1, double tau2, double sampleRate);

    //---------------------------------------------------------------------------------------------
    // audio processing:

    /** Calculates one sample at a time. */
    INLINE double getSample(double in);

    //---------------------------------------------------------------------------------------------
    // others:

    /** Resets the internal state of the filter. */
    void reset();

    //=============================================================================================

  protected:

    /** Calculates the filter coefficient. */
    void calculateCoefficient();

    double coeff;        // filter coefficient
    double y1;           // previous output sample
    double sampleRate;   // the samplerate
    double tau;          // time constant in milliseconds

  };

  //-----------------------------------------------------------------------------------------------
  // inlined functions:

  INLINE double LeakyIntegrator::getSample(double in)
  {
    return y1 = in + coeff*(y1-in);
  }

} // end namespace rosic

#endif 
