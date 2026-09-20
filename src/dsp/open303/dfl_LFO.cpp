#include "dfl_LFO.h"

namespace dfl
{

  LFO::LFO()
  {
    sampleRate      = 44100.0;
    rate            = 5.0;      // Default 5 Hz
    phase           = 0.0;
    waveform        = 0;        // Sine
    sampleHoldValue = 0.0;
    pinkCounter     = 0;
    for(int i = 0; i < 7; i++)
      pinkState[i] = 0.0;
    updateIncrement();
  }

  LFO::~LFO()
  {

  }

  void LFO::setSampleRate(double newSampleRate)
  {
    if(newSampleRate > 0.0)
    {
      sampleRate = newSampleRate;
      updateIncrement();
    }
  }

  void LFO::setRate(double newRate)
  {
    if(newRate >= 0.1 && newRate <= 1000.0)
    {
      rate = newRate;
      updateIncrement();
    }
  }

  void LFO::setWaveform(int newWaveform)
  {
    if(newWaveform >= 0 && newWaveform <= 5)
    {
      waveform = newWaveform;
      // Reset sample & hold value when changing waveform
      if(waveform == 4)  // Random S&H
        sampleHoldValue = generateRandomValue();
      // Reset pink noise state when changing to pink noise
      if(waveform == 5)  // Pink Noise
      {
        pinkCounter = 0;
        for(int i = 0; i < 7; i++)
          pinkState[i] = 0.0;
      }
    }
  }

  void LFO::reset()
  {
    phase = 0.0;
    if(waveform == 4)  // Random S&H
      sampleHoldValue = generateRandomValue();
  }

  void LFO::updateIncrement()
  {
    increment = rate / sampleRate;
  }

  double LFO::generateRandomValue()
  {
    // Generate random value between 0.0 and +1.0 (unipolar)
    return static_cast<double>(rand()) / static_cast<double>(RAND_MAX);
  }

  double LFO::generatePinkNoise()
  {
    // Paul Kellett's pink noise algorithm (1/f spectrum, band-limited)
    // Uses 7 octaves of noise, each updated at different rates
    // This creates a warmer, more natural-sounding noise than white noise

    // Update counters at different rates (powers of 2)
    pinkCounter++;

    // Update state variables at different rates (octaves)
    // generateRandomValue() now returns 0.0 to 1.0, so we scale appropriately
    if (pinkCounter & 1)    pinkState[0] = generateRandomValue() * 0.5;
    if (pinkCounter & 2)    pinkState[1] = generateRandomValue() * 0.25;
    if (pinkCounter & 4)    pinkState[2] = generateRandomValue() * 0.125;
    if (pinkCounter & 8)    pinkState[3] = generateRandomValue() * 0.0625;
    if (pinkCounter & 16)   pinkState[4] = generateRandomValue() * 0.03125;
    if (pinkCounter & 32)   pinkState[5] = generateRandomValue() * 0.015625;
    if (pinkCounter & 64)   pinkState[6] = generateRandomValue() * 0.0078125;

    // Sum all octaves - with unipolar input, this gives approximately 0.0 to 1.0 range
    double output = pinkState[0] + pinkState[1] + pinkState[2] + pinkState[3] +
                    pinkState[4] + pinkState[5] + pinkState[6];

    return output;
  }

} // end namespace dfl
