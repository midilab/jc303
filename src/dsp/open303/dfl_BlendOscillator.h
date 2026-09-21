#ifndef dfl_BlendOscillator_h
#define dfl_BlendOscillator_h

// rosic-includes:
#include "rosic_MipMappedWaveTable.h"
#include "rosic_RealFunctions.h"   // for rosic::tanhApprox

namespace dfl
{

  // pull in the rosic helpers this oscillator relies on (the class lives in namespace dfl now,
  // but the wavetable and math helpers remain Robin Schmidt's rosic code):
  using rosic::MipMappedWaveTable;
  using rosic::floorInt;

  /**

  Blend oscillator derived from Robin Schmidt's rosic::BlendOscillator (Open303). The phase
  accumulator, mip-map table selection and linear interpolation are unchanged from the original.

  What differs: the 303 pulse/square waveform is no longer read from a pre-rendered, fixed-symmetry
  wavetable. Instead it is synthesised at audio rate by subtracting two phase-offset copies of the
  saw table and applying a tanh shaper - so the pulse width becomes a continuous, per-sample
  parameter (setPulseWidth no longer regenerates any wavetable via FFT). This is what lets the
  pulse width track pitch the way the original TB-303 does. Only the saw table (waveTable1) is
  used; the second wavetable of the original class has been removed.

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

    /** Sets the samplerate. */
    void setSampleRate(double newSampleRate);

    /** Sets the waveform of the oscillator (the saw table the pulse is derived from). */
    void setWaveForm1(int newWaveForm1);

    /** Set start phase (range 0 - 360 degrees). */
    void setStartPhase(double StartPhase);

    /** An object of class WaveTable should be passed with this function which will be used in the
    oscillator. Not to have "our own" WaveTable-object as member-variable avoids the need to have
    the same waveform for different synth-voices multiple times in the memory. */
    void setWaveTable1(MipMappedWaveTable* newWaveTable1);

    /** Sets the blend/mix factor between saw and pulse. The value is expected between 0...1 where 0
    means saw only, 1 means pulse only - in between there will be a linear blend between the two. */
    void setBlendFactor(double newBlendFactor) { blend = newBlendFactor; }

    /** Sets the frequency of the oscillator. */
    INLINE void setFrequency(double newFrequency);

    /** Sets the pulse width of the synthesised pulse wave (in percent, 0...100). Cheap - just
    stores a value, no wavetable regeneration, so it is safe to call at audio rate. */
    INLINE void setPulseWidth(double newPulseWidth);

    /** Sets the drive (in dB) for the tanh-shaper that forms the 303-style pulse edges. */
    void setTanhShaperDrive(double newDriveDb) { tanhDrive = dB2amp(newDriveDb); }

    /** Sets the DC offset (raw value) for the tanh-shaper. */
    void setTanhShaperOffset(double newOffset) { tanhOffset = newOffset; }

    /** Sets the phase shift (in degrees) of the pulse wave relative to the saw. */
    void setSquarePhaseShift(double newShift) { squarePhaseShift = newShift; }

    /** Sets the phase increment from outside. */
    INLINE void setIncrement(double newIncrement) { increment = newIncrement; }

    //---------------------------------------------------------------------------------------------
    // inquiry:

    /** Returns the blend/mix factor between saw and pulse (0...1). */
    double getBlendFactor() const { return blend; }

    /** Returns the drive (in dB) of the tanh-shaper. */
    double getTanhShaperDrive() const { return amp2dB(tanhDrive); }

    /** Returns the DC offset of the tanh-shaper. */
    double getTanhShaperOffset() const { return tanhOffset; }

    /** Returns the phase shift (in degrees) of the pulse wave relative to the saw. */
    double getSquarePhaseShift() const { return squarePhaseShift; }

    /** Returns the phase increment. */
    INLINE double getIncrement() const { return increment; }

    //---------------------------------------------------------------------------------------------
    // audio processing:

    /** Calculates one output sample at a time. */
    INLINE double getSample();

    //---------------------------------------------------------------------------------------------
    // others:

    /** Calculates the phase-increment according to freq. */
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
    double blend;             // the blend factor between saw and pulse
    double startIndex;        // start-phase-index of the osc (range: 0 - tableLength)
    double sampleRate;        // the samplerate
    double sampleRateRec;     // 1/sampleRate
    double pulseWidth;        // pulse width as 0-1 fraction (for two-saw pulse generation)
    double tanhDrive;         // drive (amplitude) for the pulse-edge tanh shaper
    double tanhOffset;        // DC offset for the pulse-edge tanh shaper
    double squarePhaseShift;  // phase shift of pulse relative to saw (degrees)

    MipMappedWaveTable *waveTable1; // the saw table both saws are read from

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
    // Store pulse width as a 0-1 fraction for audio-rate PWM via two-saw subtraction.
    pulseWidth = newPulseWidth / 100.0;
  }

  INLINE void BlendOscillator::calculateIncrement()
  {
    increment = tableLengthDbl*freq*sampleRateRec;
  }

  INLINE double BlendOscillator::getSample()
  {
    double out1, out2;
    int    tableNumber;

    if( waveTable1 == NULL )
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
    double frac     = phaseIndex - (double) intIndex;

    // saw component from waveTable1:
    double saw1 = waveTable1->getValueLinear(intIndex, frac, tableNumber);
    out1 = (1.0-blend) * saw1;

    // pulse component: synthesise from a second, phase-shifted read of the same saw table and
    // shape it with a tanh - this is what makes the pulse width a continuous, per-sample value.
    if( blend > 0.0 )
    {
      double phaseShiftSamples = squarePhaseShift * tableLengthDbl / 360.0;
      double pwOffsetSamples   = pulseWidth * tableLengthDbl;
      double phase2            = phaseIndex + pwOffsetSamples + phaseShiftSamples;

      // wrap the second phase into [0, tableLength):
      while( phase2 >= tableLengthDbl )
        phase2 -= tableLengthDbl;
      while( phase2 < 0.0 )
        phase2 += tableLengthDbl;

      int    intIndex2 = floorInt(phase2);
      double frac2     = phase2 - (double) intIndex2;
      double saw2      = waveTable1->getValueLinear(intIndex2, frac2, tableNumber);

      // pulse = saw2 - saw1; the tanh (with DC offset) forms the 303-style edges. Negated to match
      // the original 303 square-wave polarity. tanhApprox keeps us off a real transcendental, in
      // keeping with the rest of the engine (the ladder filter shaper is a polynomial too).
      double pulseRaw    = saw2 - saw1;
      double pulseShaped = -rosic::tanhApprox(tanhDrive*pulseRaw + tanhOffset);

      out2 = blend * pulseShaped * 0.5;  // 0.5 to match the original square level
    }
    else
    {
      out2 = 0.0;
    }

    phaseIndex += increment;
    return out1 + out2;
  }

} // end namespace dfl

#endif // dfl_BlendOscillator_h
