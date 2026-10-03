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
  band-limited saws from a mip-mapped wavetable, then shaped with the 303-style tanh stage.
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

      return -std::tanh(tanhDrive * (sawB - sawA) + tanhOffset) * 0.5;
    }

    double getSaw(double phase, int tableNumber)
    {
      int intIndex = rosic::floorInt(phase);
      return waveTable->getValueLinear(intIndex, phase - (double) intIndex, tableNumber);
    }

    static constexpr double tableLength = 2048.0;  // MipMappedWaveTable::tableLength
    static constexpr double pulseWidth1 = 0.25;
    static constexpr double pulseWidth2 = 0.5;
    static constexpr double tanhDrive   = 69.98;   // 36.9 dB, 303-style
    static constexpr double tanhOffset  = 4.37;

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
