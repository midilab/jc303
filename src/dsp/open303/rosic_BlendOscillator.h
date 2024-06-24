#ifndef rosic_BlendOscillator_h
#define rosic_BlendOscillator_h

// rosic-indcludes:
#include "rosic_MipMappedWaveTable.h"

namespace rosic
{

  /**

  This is an oscillator that can continuously blend between two waveforms - this is more efficient
  than using two separate oscillators because the phase-accumulator has to be calculated only once
  for both waveforms.

  */

  class BlendOscillator
  {

  public:

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. */
    BlendOscillator();

    /** Destructor. */
    ~BlendOscillator();

    //---------------------------------------------------------------------------------------------
    // parameter settings:

    /** Sets the sample-rateRate(). */
    void setSampleRate(double newSampleRate);

    /** Sets the 1st waveform of the oscillator. */
    void setWaveForm1(int newWaveForm1);

    /** Sets the 2nd waveform of the oscillator. */
    void setWaveForm2(int newWaveForm2);

    /** Set start phase (range 0 - 360 degrees). */
    void setStartPhase(double StartPhase);

    /** An object of class WaveTable should be passed with this function which will be used in the 
    oscillator. Not to have "our own" WaveTable-object as member-variable avoids the need to have 
    the same waveform for different synth-voices multiple times in the memory. This function sets 
    the 1st wavetable. */
    void setWaveTable1(MipMappedWaveTable* newWaveTable1);

    /** Sets the 2nd wavetable. @see setWaveTable1 */
    void setWaveTable2(MipMappedWaveTable* newWaveTable2);

    /** Sets the blend/mix factor between the two waveforms. The value is expected between 0...1
    where 0 means waveform1 only, 1 means waveform2 only - in between there will be a linear blend
    between the two waveforms. */
    void setBlendFactor(double newBlendFactor) { blend = newBlendFactor; }

    /** Sets the frequency of the oscillator. */
    INLINE void setFrequency(double newFrequency);

    /** Sets the pulse width (or symmetry) of the oscillator. */
    INLINE void setPulseWidth(double newPulseWidth);

    /** Sets the phase increment from outside. */
    INLINE void setIncrement(double newIncrement) { increment = newIncrement; }

    //---------------------------------------------------------------------------------------------
    // inquiry:

    /** Returns the blend/mix factor between the two waveforms as a value between 0...1 where 0 
    means waveform1 only, 1 means waveform2 only - in between there will be a linear blend between 
    the two waveforms. */
    double getBlendFactor() const { return blend; }

    /** Returns the phase increment. */
    INLINE double getIncrement() const { return increment; }

    //---------------------------------------------------------------------------------------------
    // audio processing:

    /** Calculates one output sample at a time. */
    INLINE double getSample();

    //---------------------------------------------------------------------------------------------
    // others:

    /** Calculates the phase-increments for first and second half-period according to freq and 
    pulseWidth. */
    INLINE void calculateIncrement();

    /** Resets the phaseIndex to startIndex. */
    void resetPhase();

    /** Reset the phaseIndex to startIndex+PhaseIndex. */
    void setPhase(double PhaseIndex);

    //=============================================================================================

  protected:

    double tableLengthDbl;    // tableLength as double variable
    double phaseIndex;        // current phase index
    double freq;              // frequency of the oscillator
    double increment;         // phase increment per sample
    double blend;             // the blend factor between the two waveforms
    double startIndex;        // start-phase-index of the osc (range: 0 - tableLength)
    double sampleRate;        // the samplerate
    double sampleRateRec;     // 1/sampleRate

    MipMappedWaveTable *waveTable1, *waveTable2; // the 2 wavetables between which we blend

  };

  //-----------------------------------------------------------------------------------------------
  // inlined functions:

  INLINE void BlendOscillator::setFrequency(double newFrequency)
  {
    if( (newFrequency > 0.0) && (newFrequency < 20000.0) )
      freq = newFrequency;
  }

  INLINE void BlendOscillator::setPulseWidth(double newPulseWidth)
  {
    waveTable1->setSymmetry(0.01*newPulseWidth);
    waveTable2->setSymmetry(0.01*newPulseWidth);
  }

  INLINE void BlendOscillator::calculateIncrement()
  {
    increment = tableLengthDbl*freq*sampleRateRec;
  }

  INLINE double BlendOscillator::getSample()
  {
    double out1, out2;
    int    tableNumber;

    if( waveTable1 == NULL || waveTable2 == NULL )
      return 0.0;

    // from this increment, decide which table is to be used:
    tableNumber  = ((int)EXPOFDBL(increment));
    //tableNumber += 1;           // generate frequencies up to nyquist/2 on the highest note
    tableNumber += 2;             // generate frequencies up to nyquist/4 on the highest note
                                  // \todo: make this number adjustable from outside

    // wraparound if necessary:
    while( phaseIndex>=tableLengthDbl )
      phaseIndex -= tableLengthDbl;

    int    intIndex = floorInt(phaseIndex);
    double frac     = phaseIndex  - (double) intIndex;
    out1 = (1.0-blend) * waveTable1->getValueLinear(intIndex, frac, tableNumber);
    out2 =      blend  * waveTable2->getValueLinear(intIndex, frac, tableNumber);
    
    out2 *= 0.5; // \todo: this is preliminary to scale the square in AciDevil we need to
                 // implement something more general here (like a kind of crest-compensation in 
                 // the wavetable-class)

    phaseIndex += increment;
    return out1 + out2;
  }

} // end namespace rosic

#endif // rosic_BlendOscillator_h
