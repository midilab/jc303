#ifndef dfl_BlendOscillator_h
#define dfl_BlendOscillator_h

#include <cmath>

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

    /** Enables the TB-303 waveshaper envelope on the pulse (on by default): the collector-high segment follows the
    circuit's shape instead of a flat top (see ENV_TABLE). */
    void setCircuitPulseShape(bool enabled) { circuitPulseShape = enabled; }

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

    // TB-303 waveshaper (Q8) envelope. The real square is not flat-topped: in the bottom octave the collector rises
    // slowly after the edge, and at every pitch the top slopes down as the saw keeps falling (the transistor is not
    // fully switched), while the low level (transistor off) is constant. ENV_TABLE is the height of the
    // collector-high segment from an ngspice model of the stage (tools/osc-fidelity/gen_pulse_envelope.py), relative to the
    // high-pitch reference height (about 3.51 V), at ENV_COLS points from the segment's start to its end (the 50% crossings)
    // and at ENV_FREQ pitches; the circuit's fast edges (under ~6% of the segment) are left to the band-limited pulse.
    // The pulse's other segment is saturated and flat, so the envelope only switches mid-way through it.
    static constexpr int ENV_ROWS = 15, ENV_COLS = 33;
    static constexpr double ENV_FREQ[ENV_ROWS] = { 10, 15, 22, 27.5, 32.7032, 41.2, 49, 55, 65.4064, 82.4, 98, 130.81, 196, 261.63, 523.25 };
    static constexpr double ENV_TABLE[ENV_ROWS][ENV_COLS] = {
      { 0.339, 0.368, 0.387, 0.404, 0.419, 0.435, 0.451, 0.467, 0.482, 0.498, 0.514, 0.529, 0.545, 0.561, 0.576, 0.592, 0.608, 0.623, 0.639, 0.655, 0.670, 0.667, 0.649, 0.634, 0.621, 0.611, 0.603, 0.596, 0.591, 0.586, 0.583, 0.579, 0.576 },
      { 0.415, 0.436, 0.473, 0.497, 0.515, 0.531, 0.547, 0.562, 0.577, 0.592, 0.606, 0.621, 0.636, 0.651, 0.666, 0.680, 0.695, 0.710, 0.724, 0.716, 0.697, 0.679, 0.663, 0.649, 0.637, 0.627, 0.619, 0.611, 0.605, 0.599, 0.594, 0.590, 0.586 },
      { 0.528, 0.528, 0.576, 0.611, 0.637, 0.657, 0.675, 0.691, 0.706, 0.720, 0.734, 0.748, 0.762, 0.775, 0.789, 0.797, 0.782, 0.762, 0.743, 0.726, 0.710, 0.695, 0.682, 0.670, 0.660, 0.650, 0.641, 0.634, 0.626, 0.620, 0.614, 0.609, 0.604 },
      { 0.606, 0.606, 0.647, 0.688, 0.719, 0.744, 0.765, 0.783, 0.799, 0.814, 0.828, 0.842, 0.853, 0.844, 0.825, 0.806, 0.787, 0.769, 0.753, 0.738, 0.724, 0.711, 0.699, 0.688, 0.678, 0.669, 0.660, 0.652, 0.645, 0.638, 0.632, 0.626, 0.620 },
      { 0.673, 0.673, 0.709, 0.754, 0.790, 0.818, 0.842, 0.863, 0.881, 0.897, 0.901, 0.885, 0.866, 0.847, 0.829, 0.812, 0.795, 0.780, 0.765, 0.752, 0.739, 0.727, 0.716, 0.706, 0.696, 0.687, 0.678, 0.670, 0.663, 0.656, 0.649, 0.643, 0.637 },
      { 0.777, 0.777, 0.805, 0.854, 0.894, 0.928, 0.955, 0.956, 0.940, 0.923, 0.906, 0.888, 0.872, 0.856, 0.841, 0.826, 0.813, 0.800, 0.787, 0.775, 0.764, 0.754, 0.743, 0.734, 0.725, 0.716, 0.707, 0.700, 0.692, 0.685, 0.678, 0.671, 0.665 },
      { 0.870, 0.870, 0.889, 0.940, 0.981, 0.982, 0.969, 0.954, 0.938, 0.923, 0.908, 0.893, 0.879, 0.866, 0.853, 0.840, 0.828, 0.817, 0.806, 0.795, 0.785, 0.775, 0.766, 0.757, 0.748, 0.740, 0.732, 0.724, 0.716, 0.709, 0.702, 0.696, 0.690 },
      { 0.939, 0.939, 0.953, 0.996, 0.992, 0.980, 0.966, 0.952, 0.938, 0.924, 0.911, 0.898, 0.885, 0.873, 0.861, 0.850, 0.839, 0.828, 0.818, 0.808, 0.799, 0.790, 0.781, 0.772, 0.764, 0.756, 0.748, 0.740, 0.733, 0.726, 0.719, 0.713, 0.707 },
      { 1.000, 1.000, 1.006, 0.998, 0.986, 0.975, 0.963, 0.951, 0.939, 0.928, 0.917, 0.906, 0.895, 0.885, 0.875, 0.865, 0.855, 0.846, 0.837, 0.828, 0.819, 0.811, 0.803, 0.795, 0.787, 0.780, 0.773, 0.766, 0.759, 0.752, 0.745, 0.739, 0.733 },
      { 0.997, 1.002, 0.998, 0.989, 0.980, 0.970, 0.961, 0.952, 0.943, 0.934, 0.925, 0.916, 0.908, 0.899, 0.891, 0.883, 0.875, 0.867, 0.859, 0.852, 0.845, 0.838, 0.830, 0.824, 0.817, 0.810, 0.804, 0.797, 0.791, 0.785, 0.779, 0.773, 0.768 },
      { 0.988, 0.994, 0.990, 0.983, 0.975, 0.968, 0.960, 0.952, 0.945, 0.937, 0.930, 0.922, 0.915, 0.908, 0.901, 0.894, 0.887, 0.881, 0.874, 0.868, 0.861, 0.855, 0.849, 0.842, 0.836, 0.831, 0.825, 0.819, 0.813, 0.808, 0.802, 0.797, 0.792 },
      { 0.976, 0.983, 0.981, 0.976, 0.971, 0.965, 0.960, 0.954, 0.948, 0.943, 0.937, 0.932, 0.926, 0.921, 0.916, 0.910, 0.905, 0.900, 0.895, 0.890, 0.885, 0.880, 0.875, 0.870, 0.865, 0.860, 0.855, 0.851, 0.846, 0.841, 0.837, 0.832, 0.828 },
      { 0.970, 0.977, 0.978, 0.975, 0.972, 0.969, 0.965, 0.961, 0.958, 0.954, 0.950, 0.947, 0.943, 0.939, 0.936, 0.932, 0.929, 0.925, 0.921, 0.918, 0.914, 0.911, 0.907, 0.904, 0.900, 0.897, 0.893, 0.890, 0.887, 0.883, 0.880, 0.876, 0.873 },
      { 0.974, 0.981, 0.983, 0.982, 0.980, 0.977, 0.975, 0.972, 0.970, 0.967, 0.964, 0.961, 0.959, 0.956, 0.953, 0.950, 0.947, 0.945, 0.942, 0.939, 0.936, 0.934, 0.931, 0.928, 0.926, 0.923, 0.920, 0.918, 0.915, 0.912, 0.909, 0.907, 0.904 },
      { 1.009, 1.012, 1.016, 1.017, 1.017, 1.017, 1.016, 1.015, 1.014, 1.013, 1.012, 1.010, 1.009, 1.008, 1.006, 1.005, 1.003, 1.002, 1.001, 0.999, 0.998, 0.996, 0.995, 0.993, 0.992, 0.990, 0.989, 0.987, 0.986, 0.984, 0.983, 0.982, 0.980 } };
    static constexpr double SAW_TABLE_RESET = 0.5;   // phase (0..1) of the saw table's reset
    bool   circuitPulseShape = true;
    int    envRow = 0;            // ENV_FREQ row below the current frequency (set in calculateIncrement)
    double envRowFrac = 0.0;      // interpolation weight towards envRow + 1
    double envGain = 1.0;         // envelope value in effect
    double pulseBase = 0.0;       // the pulse's other (flat) level, sampled mid-segment
    INLINE double pulseEnvelope(double segPos) const;

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

    // envelope row: ENV_FREQ is roughly log-spaced, so interpolate in log frequency (clamped to the table)
    const double f = std::min(std::max(freq, ENV_FREQ[0]), ENV_FREQ[ENV_ROWS-1]);
    int r = 0;
    while( r < ENV_ROWS-2 && ENV_FREQ[r+1] < f )
      r++;
    envRow     = r;
    envRowFrac = std::log(f/ENV_FREQ[r]) / std::log(ENV_FREQ[r+1]/ENV_FREQ[r]);
  }

  INLINE double BlendOscillator::pulseEnvelope(double segPos) const
  {
    const double x  = std::min(std::max(segPos, 0.0), 1.0) * (ENV_COLS-1);
    const int    c  = std::min((int) x, ENV_COLS-2);
    const double cf = x - c;
    const double* a = ENV_TABLE[envRow];
    const double* b = ENV_TABLE[envRow+1];
    const double lo = a[c] + cf*(a[c+1]-a[c]);
    const double hi = b[c] + cf*(b[c+1]-b[c]);
    return lo + envRowFrac*(hi-lo);
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
      // The saw table is linear (slope 2 per cycle), so saw2 - saw1 is a band-limited step between two constant
      // levels: 2*pw - 2 outside the wrapped segment and 2*pw inside it. Shaping that step with the tanh directly
      // hard-clips its Gibbs ripple and aliases (-31..-40 dB below 20 kHz at 4x); instead take the tanh of the two
      // levels and crossfade between them with the band-limited step, which keeps the same levels and no aliasing.
      double pulseRaw    = saw2 - saw1;
      double rawLo       = 2.0*pulseWidth - 2.0;
      double levelLo     = -rosic::tanhApprox(tanhDrive*rawLo + tanhOffset);
      double levelHi     = -rosic::tanhApprox(tanhDrive*(rawLo + 2.0) + tanhOffset);
      double pulseShaped = levelLo + 0.5*(pulseRaw - rawLo)*(levelHi - levelLo);
      if( circuitPulseShape )
      {
        // the collector-high segment runs from the second read's reset to the first read's (the saw reset, as in the
        // circuit); the 303 saw table resets at mid-table, so measured from there it is [1 - pw, 1) of the cycle
        double q = (phaseIndex + phaseShiftSamples) / tableLengthDbl + SAW_TABLE_RESET;
        q -= std::floor(q);
        const double segStart = 1.0 - pulseWidth;
        if( q >= segStart )
          envGain = pulseEnvelope((q - segStart) / pulseWidth);
        else if( q >= 0.25*segStart && q < 0.75*segStart )
        {
          envGain   = pulseEnvelope(0.0);   // switch in the middle of the flat segment, well away from both edges
          pulseBase = pulseShaped;          // (the band-limited edges spread a little either side of segStart)
        }
        pulseShaped = pulseBase + (pulseShaped - pulseBase) * envGain;
      }

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
