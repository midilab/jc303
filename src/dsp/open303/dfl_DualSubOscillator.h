#ifndef dfl_DualSubOscillator_h
#define dfl_DualSubOscillator_h

#include "rosic_MipMappedWaveTable.h"
#include <cmath>

namespace dfl
{

  /**
  A sub-oscillator that blends between two pulse waves at different sub-frequencies:
  - -2 octaves (0.25x frequency) with 25% pulse width
  - -1 octave  (0.5x frequency) with 50% pulse width (square)
  Blend morphs between both octave and timbre. Pulses are made by subtracting two phase-shifted
  band-limited saws from a mip-mapped wavetable: a flat-topped, band-limited pulse (like the hard digital
  divider sub of an SH-101).
  */

  class DualSubOscillator
  {

  public:

    void setSampleRate(double newSampleRate)
    {
      if( newSampleRate > 0.0 )
        sampleRateRec = 1.0 / newSampleRate;
    }

    void setWaveTable(rosic::MipMappedWaveTable* newWaveTable) { waveTable = newWaveTable; }

    void setFrequency(double newFrequency)
    {
      if( newFrequency > 0.0 && newFrequency < 20000.0 )
        freq = newFrequency;
    }

    void calculateIncrement()
    {
      increment1 = tableLength * (freq * 0.25) * sampleRateRec;
      increment2 = tableLength * (freq * 0.5)  * sampleRateRec;
    }

    /** Returns the next sample; blend 0.0 = -2 octaves, 1.0 = -1 octave. */
    double getSample(double blend)
    {
      if( waveTable == nullptr )
        return 0.0;

      double pulse1 = getPulse(phaseIndex1, increment1, pulseWidth1);
      double pulse2 = getPulse(phaseIndex2, increment2, pulseWidth2);

      phaseIndex1 += increment1;
      phaseIndex2 += increment2;

      return (1.0-blend) * pulse1 + blend * pulse2;
    }

    void resetPhase()
    {
      phaseIndex1 = 0.0;
      phaseIndex2 = 0.0;
    }

  protected:

    double getPulse(double &phaseIndex, double increment, double pulseWidth)
    {
      while( phaseIndex >= tableLength )
        phaseIndex -= tableLength;

      int tableNumber = ((int)EXPOFDBL(increment)) + 2;  // mip-map level for anti-aliasing

      double sawA = getSaw(phaseIndex, tableNumber);
      double phaseB = phaseIndex + pulseWidth * tableLength;
      if( phaseB >= tableLength )
        phaseB -= tableLength;
      double sawB = getSaw(phaseB, tableNumber);

      // the saw table is linear, so sawB - sawA is a band-limited step between 2*pw - 2 (inside the wrapped
      // segment) and 2*pw; map it to +/-0.5 directly. A tanh on it would clip the step's ripple and alias.
      return 0.5 - 0.5*(sawB - sawA - (2.0*pulseWidth - 2.0));
    }

    double getSaw(double phase, int tableNumber)
    {
      int intIndex = rosic::floorInt(phase);
      return waveTable->getValueLinear(intIndex, phase - (double) intIndex, tableNumber);
    }

    static constexpr double tableLength = 2048.0;  // MipMappedWaveTable::tableLength
    static constexpr double pulseWidth1 = 0.25;
    static constexpr double pulseWidth2 = 0.5;

    double phaseIndex1   = 0.0;
    double phaseIndex2   = 0.0;
    double increment1    = 0.0;
    double increment2    = 0.0;
    double freq          = 440.0;
    double sampleRateRec = 1.0 / 44100.0;

    rosic::MipMappedWaveTable *waveTable = nullptr;

  };

} // end namespace dfl

#endif // dfl_DualSubOscillator_h
