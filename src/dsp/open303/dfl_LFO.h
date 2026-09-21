#ifndef dfl_LFO_h
#define dfl_LFO_h

#include "GlobalDefinitions.h"
#include <cstdlib>
#include <cmath>

namespace dfl
{

  /**

  Low Frequency Oscillator with multiple waveforms inspired by the Roland SH-101.
  Provides sine, triangle, sawtooth, square, sample & hold random, and noise waveforms.
  All waveforms output unipolar range: 0.0 to +1.0.

  */

  class LFO
  {

  public:

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. */
    LFO();

    /** Destructor. */
    ~LFO();

    //---------------------------------------------------------------------------------------------
    // parameter settings:

    /** Sets the sample rate (in Hz). */
    void setSampleRate(double newSampleRate);

    /** Sets the LFO rate/frequency (in Hz). */
    void setRate(double newRate);

    /** Sets the waveform type (0=Triangle, 1=Saw Up, 2=Saw Down, 3=Square, 4=Random S&H, 5=Noise/Pink). */
    void setWaveform(int newWaveform);

    //---------------------------------------------------------------------------------------------
    // inquiry:

    /** Returns the current LFO rate (in Hz). */
    double getRate() const { return rate; }

    /** Returns the current waveform type. */
    int getWaveform() const { return waveform; }

    //---------------------------------------------------------------------------------------------
    // audio processing:

    /** Calculates one output sample at a time. */
    INLINE double getSample();

    //---------------------------------------------------------------------------------------------
    // others:

    /** Resets the LFO phase to zero. */
    void reset();

  protected:

    /** Updates the phase increment based on current rate and sample rate. */
    void updateIncrement();

    /** Generates a random value between -1.0 and +1.0. */
    double generateRandomValue();

    /** Generates pink noise using Paul Kellett algorithm (band-limited white noise). */
    double generatePinkNoise();

    double sampleRate;      // sample rate in Hz
    double rate;            // LFO frequency in Hz
    double phase;           // current phase (0.0 to 1.0)
    double increment;       // phase increment per sample
    int waveform;           // 0=Triangle, 1=Saw Up, 2=Saw Down, 3=Square, 4=Random, 5=Pink Noise
    double sampleHoldValue; // stored random value for S&H waveform

    // Pink noise state (Paul Kellett algorithm)
    double pinkState[7];    // 7 octaves of pink noise state
    int pinkCounter;        // counter for pink noise updates

  };

  //-----------------------------------------------------------------------------------------------
  // inlined functions:

  INLINE double LFO::getSample()
  {
    double output = 0.0;

    switch(waveform)
    {
      case 0: // Triangle (unipolar)
        // Unipolar triangle: 0.0 to +1.0
        if(phase < 0.5)
          output = 2.0 * phase;         // Rising: 0 to +1
        else
          output = 2.0 - 2.0 * phase;   // Falling: +1 to 0
        break;

      case 1: // Sawtooth Up (unipolar)
        // Unipolar rising sawtooth: 0.0 to +1.0
        output = phase;
        break;

      case 2: // Sawtooth Down (unipolar)
        // Unipolar falling sawtooth: +1.0 to 0.0
        output = 1.0 - phase;
        break;

      case 3: // Square (unipolar)
        // Unipolar square: 0.0 or +1.0
        output = (phase < 0.5) ? 1.0 : 0.0;
        break;

      case 4: // Random (Sample & Hold, unipolar)
        // Generate new random value at phase wrap
        if(phase < increment)  // Just wrapped around
          sampleHoldValue = generateRandomValue();
        output = sampleHoldValue;
        break;

      case 5: // Noise (pink noise, unipolar)
        output = generatePinkNoise();
        break;

      default:
        output = 0.0;
        break;
    }

    // Advance phase
    phase += increment;
    if(phase >= 1.0)
      phase -= 1.0;

    return output;
  }

} // end namespace dfl

#endif // dfl_LFO_h
