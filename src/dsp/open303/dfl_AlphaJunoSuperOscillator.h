#ifndef dfl_AlphaJunoSuperOscillator_h
#define dfl_AlphaJunoSuperOscillator_h

#include "dfl_AlphaJunoOscillator.h"
#include <cmath>

namespace dfl
{

  /**

  A detuned ensemble ("supersaw") of AlphaJunoOscillator voices, based on Adam
  Szabo's analysis of the Roland JP-8000 Super Saw (KTH thesis). One center
  voice plus six detuned side voices, with:

    - detune spread  : how far the side voices are pushed apart (Szabo curve)
    - mix            : balance between the center voice and the side voices
                       (Szabo center/side gain curves)

  Because it wraps AlphaJunoOscillator, the ensemble can render ANY of the
  Alpha Juno saw waveforms - not just a plain saw. SAW01 + spread gives the
  classic JP-8000 supersaw; SAW03 + spread + pulse width gives a detuned
  PWM "hoover" stack. Since the synth is monophonic, running seven full SAW03
  voices (each with its own +1-octave chop) is cheap.

  When the detune spread is (near) zero the side voices are bypassed, so the
  ensemble collapses to a single voice - identical, and no more expensive,
  than a lone AlphaJunoOscillator. This keeps the non-detuned Juno waveforms
  regression-free.

  */

  class AlphaJunoSuperOscillator
  {

  public:

    static const int NUM_SIDES = 6;

    AlphaJunoSuperOscillator()
    {
      freq   = 440.0;
      spread = 0.0;
      mix    = 0.75;
      updateMixGains();
      rngState = 0x2545F491u;  // arbitrary nonzero seed
    }

    //---------------------------------------------------------------------------------------------
    // parameter settings:

    void setSampleRate(double newSampleRate)
    {
      master.setSampleRate(newSampleRate);
      for( int i = 0; i < NUM_SIDES; i++ )
        sides[i].setSampleRate(newSampleRate);
    }

    void setWaveform(int newWaveform)
    {
      master.setWaveform(newWaveform);
      for( int i = 0; i < NUM_SIDES; i++ )
        sides[i].setWaveform(newWaveform);
    }

    inline void setPulseWidth(double pulseWidthPercent)
    {
      master.setPulseWidth(pulseWidthPercent);
      for( int i = 0; i < NUM_SIDES; i++ )
        sides[i].setPulseWidth(pulseWidthPercent);
    }

    /** Saw <-> square morph (0..1) applied to every voice - the Waveform knob. */
    inline void setSquareBlend(double blend)
    {
      master.setSquareBlend(blend);
      for( int i = 0; i < NUM_SIDES; i++ )
        sides[i].setSquareBlend(blend);
    }

    inline void setFrequency(double newFrequency) { freq = newFrequency; }

    /** Detune spread, 0..1 (0 = unison/bypassed, 1 = maximum spread). */
    inline void setDetune(double newSpread)
    {
      spread = (newSpread < 0.0) ? 0.0 : (newSpread > 1.0 ? 1.0 : newSpread);
    }

    /** Center/side balance, 0..1 (higher = more side-voice level). */
    inline void setMix(double newMix)
    {
      double m = (newMix < 0.0) ? 0.0 : (newMix > 1.0 ? 1.0 : newMix);
      if( m != mix )
      {
        mix = m;
        updateMixGains();
      }
    }

    //---------------------------------------------------------------------------------------------
    // audio processing:

    inline void calculateIncrement()
    {
      master.setFrequency(freq);
      master.calculateIncrement();

      if( spread > SPREAD_EPS )
      {
        double det = calcDetune(spread);
        for( int i = 0; i < NUM_SIDES; i++ )
        {
          sides[i].setFrequency(freq * (1.0 + det * offsets[i]));
          sides[i].calculateIncrement();
        }
      }
    }

    inline double getSample()
    {
      // Below threshold: single voice, identical to a lone AlphaJunoOscillator.
      if( spread <= SPREAD_EPS )
        return master.getSample();

      double out = centerGain * master.getSample();
      for( int i = 0; i < NUM_SIDES; i++ )
        out += sideGain * sides[i].getSample();

      return NORMALIZE * out;
    }

    /** Called on note-on. The center voice resets to phase 0 so it stays
    phase-coherent with the sub-oscillator (which also resets to 0) - matching
    the Alpha Juno's single-DCO behavior for a consistent, punchy attack. Only
    the six side voices are randomized, which is all that's needed for the
    JP-8000 supersaw's decorrelated ensemble. */
    void reset()
    {
      master.resetPhase();  // phase 0: coherent with the sub-oscillator
      for( int i = 0; i < NUM_SIDES; i++ )
        sides[i].setPhase(nextRandom());
    }

    //=============================================================================================

  protected:

    static constexpr double NORMALIZE  = 0.5;
    static constexpr double SPREAD_EPS = 1.0e-4;

    // Szabo's detune curve: how the fixed offsets are scaled by the spread knob.
    static double calcDetune(double x) { return 1.0 - std::pow(1.0 - x, 0.2); }
    // Szabo's gain curves for the center voice and the side voices vs. mix.
    static double calcCenter(double x) { return -0.55366 * x + 0.99785; }
    static double calcSide(double x)   { return -0.73764 * x * x + 1.2841 * x + 0.044372; }

    void updateMixGains()
    {
      centerGain = calcCenter(mix);
      sideGain   = calcSide(mix);
    }

    // xorshift32 - cheap, self-contained, no global RNG or time source.
    double nextRandom()
    {
      rngState ^= rngState << 13;
      rngState ^= rngState >> 17;
      rngState ^= rngState << 5;
      return (rngState & 0xFFFFFF) / static_cast<double>(0x1000000);  // [0,1)
    }

    AlphaJunoOscillator master;
    AlphaJunoOscillator sides[NUM_SIDES];

    double freq;
    double spread;      // detune spread, 0..1
    double mix;         // center/side balance, 0..1
    double centerGain;
    double sideGain;
    unsigned int rngState;

    // Fixed relative detune offsets from Szabo's JP-8000 analysis.
    static constexpr double offsets[NUM_SIDES] = {
      -0.11002313, -0.06288439, -0.01952356, 0.01991221, 0.06216538, 0.10745242
    };

  };

} // end namespace dfl

#endif // dfl_AlphaJunoSuperOscillator_h
