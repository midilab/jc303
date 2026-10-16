#ifndef dfl_DynamicSmoothingFilter_h
#define dfl_DynamicSmoothingFilter_h

#include "GlobalDefinitions.h"
#include <algorithm>
#include <cmath>

namespace rosic
{
  /**
   * Dynamic Smoothing Filter using Self Modulating Bandpass
   * Based on Andrew Simper @ Cytomic - Dynamic Smoothing
   * https://cytomic.com/files/dsp/DynamicSmoothing.pdf
   *
   * This filter adapts its smoothing rate based on the rate of change of the input signal.
   * Fast changes get more smoothing (to prevent zipper noise), while slow changes pass through
   * with minimal latency.
   */
  class DynamicSmoothingFilter
  {
  public:
    /** Constructor. */
    DynamicSmoothingFilter()
    {
      baseFreq = 2.0;      // Hz - base cutoff frequency for smoothing
      sensitivity = 2.0;   // sensitivity to rate of change
      sampleRate = 44100.0;
      wc = baseFreq / sampleRate;
      clear();
    }

    /** Sets the sample rate. */
    void setSampleRate(double newSampleRate)
    {
      sampleRate = newSampleRate;
      wc = baseFreq / sampleRate;
    }

    /** Sets the base frequency (in Hz). Lower values = more smoothing. Typical range: 1-10 Hz. */
    void setBaseFreq(double newBaseFreq)
    {
      baseFreq = newBaseFreq;
      wc = baseFreq / sampleRate;
    }

    /** Sets the sensitivity to rate of change. Higher values = more aggressive dynamic smoothing.
     *  Typical range: 1-5. */
    void setSensitivity(double newSensitivity)
    {
      sensitivity = newSensitivity;
    }

    /** Resets the filter state. */
    void clear()
    {
      low1 = 0.0;
      low2 = 0.0;
      inz = 0.0;
    }

    /** Seeds the filter state so a subsequent getSample() starts at the given value (no glide
     *  from zero on the first sample after a reset). */
    void setState(double value)
    {
      low1 = value;
      low2 = value;
      inz = value;
    }

    /** Processes one sample through the filter. */
    INLINE double getSample(double in)
    {
      // Store previous states
      double low1z = low1;
      double low2z = low2;

      // Calculate bandpass output (difference between two lowpass stages)
      double bandz = low1z - low2z;

      // Dynamic cutoff based on rate of change
      double wd = wc + sensitivity * std::abs(bandz);

      // Calculate filter coefficient (polynomial approximation for accurate linear frequency mapping)
      double g = wd * (5.9948827 + wd * (-11.969296 + wd * 15.959062));
      g = std::min(g, 1.0);

      // Two-stage cascaded lowpass filter
      low1 = low1z + g * (0.5 * (in + inz) - low1z);
      low2 = low2z + g * (0.5 * (low1 + low1z) - low2z);

      // Store input for next iteration
      inz = in;

      // Output is the second lowpass stage
      return low2;
    }

  protected:
    double baseFreq;     // Base frequency in Hz
    double sensitivity;  // Sensitivity to rate of change
    double sampleRate;   // Sample rate in Hz
    double wc;           // Normalized cutoff frequency
    double low1;         // First lowpass filter state
    double low2;         // Second lowpass filter state
    double inz;          // Previous input sample
  };

} // namespace rosic

#endif // dfl_DynamicSmoothingFilter_h
