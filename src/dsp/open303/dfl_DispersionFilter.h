// dfl_DispersionFilter.h
// Multi-stage all-pass filter cascade for harmonic dispersion
//
// Creates frequency-dependent phase delay that "smears" harmonics,
// changing how subsequent waveshaping affects the signal.

#ifndef dfl_DispersionFilter_h
#define dfl_DispersionFilter_h

#include "dfl_AllPass2p.h"
#include <array>
#include <cmath>

namespace dfl {

/**
 * Dispersion filter: cascade of all-pass filters with spread frequencies.
 *
 * When stages > 1, creates frequency-dependent delay that causes different
 * harmonics to shift in phase relative to each other. This "smears" the
 * waveform before it hits the waveshaper, creating different distortion
 * character than a clean signal would produce.
 *
 * Parameters:
 *   stages: 0-1 = bypass, 2+ = number of cascaded APFs
 *   centerFreq: center frequency of the spread
 *   spread: total spread in octaves (e.g., 2.0 = frequencies span ±1 octave)
 *   Q: resonance of each APF stage
 */
class DispersionFilter {
public:
  static constexpr int MAX_STAGES = 8;

  DispersionFilter() {
    sampleRate = 44100.0;
    stages = 0;
    centerFreq = 800.0;
    spread = 2.0;
    Q = 0.707;
    updateFrequencies();
  }

  void setSampleRate(double sr) {
    sampleRate = sr;
    updateCoefficients();
  }

  void reset() {
    for (int i = 0; i < MAX_STAGES; ++i) {
      apfs[i].clearBuffers();
    }
  }

  /** Set number of stages (0-1 = bypass) */
  void setStages(int n) {
    stages = (n < 0) ? 0 : (n > MAX_STAGES) ? MAX_STAGES : n;
    updateFrequencies();
  }

  int getStages() const { return stages; }

  /** Set center frequency in Hz */
  void setCenterFreq(double hz) {
    centerFreq = hz;
    updateFrequencies();
  }

  /** Set spread in octaves (total range around center) */
  void setSpread(double octaves) {
    spread = (octaves < 0.0) ? 0.0 : octaves;
    updateFrequencies();
  }

  /** Set Q for all stages */
  void setQ(double q) {
    Q = (q < 0.1) ? 0.1 : (q > 10.0) ? 10.0 : q;
    updateCoefficients();
  }

  /**
   * Update all APF coefficients with optional frequency modulation.
   * @param modOctaves Modulation offset in octaves (positive = higher freq)
   */
  void updateWithModulation(double modOctaves) {
    if (stages < 2)
      return;

    double modMultiplier = std::pow(2.0, modOctaves);
    double nyquist = sampleRate * 0.49;

    for (int i = 0; i < stages; ++i) {
      double freq = stageFreqs[i] * modMultiplier;
      // Clamp to valid range
      freq = (freq < 20.0) ? 20.0 : (freq > nyquist) ? nyquist : freq;
      setAPFCoefficients(i, freq, Q);
    }
  }

  /** Process a single sample through the cascade */
  float process(float x) {
    if (stages < 2)
      return x; // Bypass

    for (int i = 0; i < stages; ++i) {
      x = apfs[i].process(x);
    }
    return x;
  }

  /** Process a single sample with per-sample modulation */
  float process(float x, double modOctaves) {
    if (stages < 2)
      return x; // Bypass

    updateWithModulation(modOctaves);
    return process(x);
  }

private:
  void updateFrequencies() {
    if (stages < 2) {
      // No need to calculate frequencies for bypass
      return;
    }

    // Spread frequencies logarithmically around center
    // e.g., spread=2 octaves: frequencies span from center/2 to center*2
    double halfSpread = spread * 0.5;
    double minFreq = centerFreq / std::pow(2.0, halfSpread);
    double maxFreq = centerFreq * std::pow(2.0, halfSpread);

    // Distribute stages evenly on log scale
    double logMin = std::log(minFreq);
    double logMax = std::log(maxFreq);
    double logStep = (stages > 1) ? (logMax - logMin) / (stages - 1) : 0.0;

    for (int i = 0; i < stages; ++i) {
      stageFreqs[i] = std::exp(logMin + i * logStep);
    }

    updateCoefficients();
  }

  void updateCoefficients() {
    if (stages < 2)
      return;

    for (int i = 0; i < stages; ++i) {
      setAPFCoefficients(i, stageFreqs[i], Q);
    }
  }

  void setAPFCoefficients(int stageIndex, double freq, double q) {
    // 2nd order all-pass filter coefficients
    double w0 = 2.0 * 3.14159265358979323846 * freq / sampleRate;
    double alpha = std::sin(w0) / (2.0 * q);

    double a0 = 1.0 + alpha;
    double b0 = (1.0 - alpha) / a0;
    double b1 = -2.0 * std::cos(w0) / a0;

    apfs[stageIndex].setCoef(static_cast<float>(b0), static_cast<float>(b1));
  }

  double sampleRate;
  int stages;
  double centerFreq;
  double spread;
  double Q;

  std::array<AllPass2p, MAX_STAGES> apfs;
  std::array<double, MAX_STAGES> stageFreqs;
};

} // namespace dfl

#endif // dfl_DispersionFilter_h
