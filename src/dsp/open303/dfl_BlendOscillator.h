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
    // high-pitch reference height (about 3.51 V), at ENV_FREQ pitches. The table spans the segment from where the
    // rise starts (level 0.02, ENV_LEAD of the duty before the 50% crossing, about 1.5% of the cycle: the saw crossing
    // the transistor's soft threshold) to the falling 50% crossing, at points (i / (ENV_COLS - 1))^2 of it, so the
    // envelope draws the rising edge itself and the band-limited step under it is only 0.02 high. The falling edge
    // (the saw reset, under 1 us in the circuit) is the band-limited step. The pulse's other segment is saturated and
    // flat, so the envelope only switches mid-way through it.
    static constexpr int ENV_ROWS = 15, ENV_COLS = 48;
    static constexpr double ENV_FREQ[ENV_ROWS] = { 10, 15, 22, 27.5, 32.7032, 41.2, 49, 55, 65.4064, 82.4, 98, 130.81, 196, 261.63, 523.25 };
    static constexpr double ENV_LEAD[ENV_ROWS] = { 0.0456, 0.0264, 0.0237, 0.0249, 0.0262, 0.0282, 0.0294, 0.0302, 0.0313, 0.0324, 0.0330, 0.0337, 0.0348, 0.0362, 0.0433 };
    static constexpr double ENV_EDGE_END[ENV_ROWS] = { 0.0453, 0.0548, 0.0652, 0.0652, 0.0765, 0.0765, 0.0887, 0.0887, 0.0765, 0.0652, 0.0548, 0.0548, 0.0548, 0.0548, 0.0652 };
    static constexpr double ENV_TABLE[ENV_ROWS][ENV_COLS] = {
      { 0.020, 0.023, 0.035, 0.066, 0.129, 0.207, 0.259, 0.288, 0.308, 0.325, 0.339, 0.351, 0.361, 0.370, 0.378, 0.386, 0.394, 0.402, 0.411, 0.420, 0.429, 0.439, 0.449, 0.459, 0.471, 0.482, 0.494, 0.507, 0.520, 0.534, 0.548, 0.562, 0.577, 0.592, 0.608, 0.625, 0.642, 0.659, 0.673, 0.657, 0.637, 0.621, 0.609, 0.599, 0.592, 0.586, 0.581, 0.576 },
      { 0.020, 0.022, 0.032, 0.057, 0.111, 0.202, 0.290, 0.344, 0.375, 0.397, 0.417, 0.434, 0.449, 0.463, 0.475, 0.486, 0.496, 0.505, 0.515, 0.524, 0.533, 0.542, 0.552, 0.562, 0.573, 0.583, 0.595, 0.606, 0.618, 0.631, 0.644, 0.657, 0.671, 0.686, 0.700, 0.715, 0.724, 0.706, 0.685, 0.666, 0.650, 0.636, 0.624, 0.614, 0.605, 0.598, 0.591, 0.586 },
      { 0.020, 0.022, 0.030, 0.050, 0.092, 0.175, 0.289, 0.388, 0.446, 0.480, 0.505, 0.526, 0.546, 0.565, 0.582, 0.598, 0.612, 0.625, 0.638, 0.650, 0.661, 0.671, 0.682, 0.692, 0.702, 0.713, 0.724, 0.735, 0.746, 0.758, 0.770, 0.782, 0.795, 0.792, 0.774, 0.755, 0.735, 0.717, 0.700, 0.685, 0.671, 0.658, 0.647, 0.636, 0.627, 0.618, 0.611, 0.604 },
      { 0.020, 0.022, 0.029, 0.045, 0.081, 0.153, 0.269, 0.394, 0.484, 0.534, 0.565, 0.590, 0.612, 0.633, 0.652, 0.671, 0.688, 0.705, 0.720, 0.734, 0.747, 0.760, 0.772, 0.784, 0.795, 0.806, 0.818, 0.829, 0.840, 0.851, 0.850, 0.835, 0.817, 0.798, 0.780, 0.763, 0.746, 0.730, 0.716, 0.702, 0.689, 0.677, 0.665, 0.655, 0.645, 0.636, 0.628, 0.620 },
      { 0.020, 0.022, 0.028, 0.042, 0.073, 0.137, 0.248, 0.387, 0.504, 0.576, 0.618, 0.646, 0.670, 0.692, 0.713, 0.734, 0.753, 0.772, 0.789, 0.806, 0.821, 0.836, 0.850, 0.863, 0.876, 0.888, 0.900, 0.901, 0.889, 0.873, 0.856, 0.839, 0.822, 0.806, 0.790, 0.774, 0.759, 0.745, 0.732, 0.719, 0.706, 0.695, 0.684, 0.673, 0.663, 0.654, 0.645, 0.637 },
      { 0.020, 0.021, 0.027, 0.039, 0.065, 0.119, 0.217, 0.361, 0.512, 0.624, 0.691, 0.731, 0.759, 0.783, 0.806, 0.829, 0.850, 0.871, 0.891, 0.911, 0.929, 0.947, 0.959, 0.956, 0.946, 0.933, 0.920, 0.906, 0.892, 0.878, 0.864, 0.850, 0.836, 0.822, 0.809, 0.795, 0.782, 0.770, 0.758, 0.746, 0.734, 0.723, 0.713, 0.702, 0.692, 0.683, 0.674, 0.665 },
      { 0.020, 0.021, 0.026, 0.038, 0.061, 0.107, 0.195, 0.336, 0.504, 0.648, 0.745, 0.801, 0.837, 0.864, 0.888, 0.912, 0.934, 0.957, 0.978, 0.986, 0.982, 0.974, 0.965, 0.955, 0.944, 0.932, 0.921, 0.909, 0.897, 0.885, 0.873, 0.861, 0.849, 0.837, 0.825, 0.813, 0.802, 0.790, 0.779, 0.768, 0.757, 0.747, 0.737, 0.727, 0.717, 0.707, 0.698, 0.690 },
      { 0.020, 0.021, 0.026, 0.037, 0.058, 0.101, 0.182, 0.318, 0.492, 0.658, 0.777, 0.850, 0.894, 0.925, 0.950, 0.974, 0.993, 0.997, 0.993, 0.987, 0.979, 0.971, 0.962, 0.953, 0.943, 0.933, 0.923, 0.912, 0.901, 0.891, 0.880, 0.869, 0.858, 0.847, 0.836, 0.825, 0.814, 0.804, 0.793, 0.783, 0.773, 0.763, 0.753, 0.743, 0.734, 0.724, 0.715, 0.707 },
      { 0.020, 0.021, 0.026, 0.035, 0.054, 0.092, 0.164, 0.290, 0.468, 0.660, 0.817, 0.922, 0.986, 1.006, 1.007, 1.004, 0.999, 0.994, 0.988, 0.982, 0.975, 0.968, 0.960, 0.952, 0.944, 0.935, 0.927, 0.918, 0.909, 0.900, 0.890, 0.881, 0.872, 0.862, 0.853, 0.843, 0.834, 0.824, 0.815, 0.805, 0.796, 0.787, 0.777, 0.768, 0.759, 0.750, 0.742, 0.733 },
      { 0.020, 0.021, 0.025, 0.034, 0.051, 0.084, 0.148, 0.262, 0.437, 0.652, 0.855, 0.996, 1.002, 1.001, 0.998, 0.995, 0.991, 0.986, 0.981, 0.976, 0.971, 0.965, 0.959, 0.953, 0.946, 0.940, 0.933, 0.926, 0.919, 0.911, 0.904, 0.896, 0.889, 0.881, 0.873, 0.865, 0.857, 0.849, 0.841, 0.833, 0.824, 0.816, 0.808, 0.800, 0.792, 0.784, 0.776, 0.768 },
      { 0.020, 0.021, 0.025, 0.033, 0.049, 0.079, 0.137, 0.243, 0.414, 0.639, 0.873, 0.991, 0.994, 0.993, 0.991, 0.988, 0.985, 0.981, 0.977, 0.973, 0.968, 0.963, 0.959, 0.953, 0.948, 0.942, 0.937, 0.931, 0.925, 0.919, 0.912, 0.906, 0.899, 0.892, 0.886, 0.879, 0.872, 0.865, 0.858, 0.850, 0.843, 0.836, 0.829, 0.821, 0.814, 0.806, 0.799, 0.792 },
      { 0.020, 0.021, 0.025, 0.032, 0.047, 0.075, 0.127, 0.224, 0.387, 0.621, 0.890, 0.981, 0.983, 0.983, 0.982, 0.980, 0.977, 0.975, 0.972, 0.969, 0.966, 0.962, 0.959, 0.955, 0.951, 0.947, 0.943, 0.938, 0.934, 0.929, 0.924, 0.919, 0.914, 0.909, 0.904, 0.898, 0.893, 0.887, 0.882, 0.876, 0.870, 0.864, 0.858, 0.852, 0.846, 0.840, 0.834, 0.828 },
      { 0.020, 0.021, 0.025, 0.032, 0.045, 0.070, 0.118, 0.205, 0.357, 0.591, 0.886, 0.974, 0.977, 0.978, 0.978, 0.977, 0.976, 0.975, 0.973, 0.971, 0.969, 0.967, 0.965, 0.962, 0.960, 0.957, 0.954, 0.951, 0.948, 0.945, 0.942, 0.938, 0.935, 0.931, 0.928, 0.924, 0.920, 0.916, 0.912, 0.908, 0.904, 0.900, 0.896, 0.891, 0.887, 0.882, 0.878, 0.873 },
      { 0.020, 0.021, 0.024, 0.031, 0.044, 0.067, 0.110, 0.190, 0.330, 0.551, 0.846, 0.978, 0.981, 0.982, 0.983, 0.983, 0.982, 0.981, 0.980, 0.979, 0.978, 0.976, 0.975, 0.973, 0.971, 0.969, 0.967, 0.965, 0.962, 0.960, 0.958, 0.955, 0.952, 0.950, 0.947, 0.944, 0.941, 0.938, 0.935, 0.932, 0.929, 0.925, 0.922, 0.919, 0.915, 0.912, 0.908, 0.904 },
      { 0.020, 0.021, 0.024, 0.029, 0.039, 0.057, 0.088, 0.143, 0.237, 0.390, 0.617, 0.902, 1.011, 1.013, 1.015, 1.016, 1.017, 1.017, 1.017, 1.017, 1.017, 1.017, 1.016, 1.015, 1.015, 1.014, 1.013, 1.012, 1.011, 1.010, 1.009, 1.007, 1.006, 1.005, 1.003, 1.002, 1.000, 0.999, 0.997, 0.995, 0.994, 0.992, 0.990, 0.988, 0.986, 0.984, 0.982, 0.980 } };
    static constexpr double SAW_TABLE_RESET = 0.5;   // phase (0..1) of the saw table's reset
    bool   circuitPulseShape = true;
    int    envRow = 0;            // ENV_FREQ row below the current frequency (set in calculateIncrement)
    double envRowFrac = 0.0;      // interpolation weight towards envRow + 1
    double envLead = 0.0;         // ENV_LEAD at the current frequency
    double envEdgeEnd = 0.0;      // ENV_EDGE_END at the current frequency
    double drawnEdgeWeight = 1.0; // 1 = drawn rising edge, 0 = band-limited step (see getSample)
    static constexpr double DRAWN_EDGE_FULL = 65.4064;   // C2
    static constexpr double DRAWN_EDGE_NONE = 130.81;    // C3
    struct PulseState
    {
      double envGain = 1.0;       // envelope value in effect
      double base = 0.0;          // the pulse's other (flat) level, sampled mid-segment
    };
    PulseState flatState, drawnState, stepState;
    INLINE double pulseEnvelope(double extPos) const;
    INLINE double shapedPulse(double saw1, int tableNumber, double phaseShiftSamples, double pw, bool drawnEdge,
                              PulseState& st) const;

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
    envLead    = ENV_LEAD[r] + envRowFrac*(ENV_LEAD[r+1]-ENV_LEAD[r]);
    envEdgeEnd = ENV_EDGE_END[r] + envRowFrac*(ENV_EDGE_END[r+1]-ENV_EDGE_END[r]);
    drawnEdgeWeight = std::min(std::max(std::log(DRAWN_EDGE_NONE/freq) / std::log(DRAWN_EDGE_NONE/DRAWN_EDGE_FULL), 0.0), 1.0);
  }

  INLINE double BlendOscillator::shapedPulse(double saw1, int tableNumber, double phaseShiftSamples, double pw,
                                             bool drawnEdge, PulseState& st) const
  {
    double phase2 = phaseIndex + pw*tableLengthDbl + phaseShiftSamples;
    while( phase2 >= tableLengthDbl )
      phase2 -= tableLengthDbl;
    while( phase2 < 0.0 )
      phase2 += tableLengthDbl;
    const int    intIndex2 = floorInt(phase2);
    const double saw2      = waveTable1->getValueLinear(intIndex2, phase2 - (double) intIndex2, tableNumber);

    // The saw table is linear (slope 2 per cycle), so saw2 - saw1 is a band-limited step between two constant
    // levels: 2*pw - 2 outside the wrapped segment and 2*pw inside it. Shaping that step with the tanh directly
    // hard-clips its Gibbs ripple and aliases (-31..-40 dB below 20 kHz at 4x); instead take the tanh of the two
    // levels and crossfade between them with the band-limited step, which keeps the same levels and no aliasing.
    // Negated to match the original 303 square-wave polarity.
    const double rawLo   = 2.0*pw - 2.0;
    const double levelLo = -rosic::tanhApprox(tanhDrive*rawLo + tanhOffset);
    const double levelHi = -rosic::tanhApprox(tanhDrive*(rawLo + 2.0) + tanhOffset);
    double pulse = levelLo + 0.5*(saw2 - saw1 - rawLo)*(levelHi - levelLo);
    if( !circuitPulseShape )
      return pulse;

    // the collector-high segment runs from the second read's reset to the first read's (the saw reset, as in the
    // circuit); the 303 saw table resets at mid-table, so measured from there it is [1 - pw, 1) of the cycle
    double q = (phaseIndex + phaseShiftSamples) / tableLengthDbl + SAW_TABLE_RESET;
    q -= std::floor(q);
    const double segStart = 1.0 - pw;
    if( q >= segStart )
    {
      const double segPos = (q - segStart) / pw;
      // drawn edge: the segment is the extended one; step edge: it starts at the 50% crossing, so map into the
      // extended table and hold the envelope over the circuit's (fast) rising edge
      const double extPos = drawnEdge ? segPos : std::max((envLead + segPos) / (1.0 + envLead), envEdgeEnd);
      st.envGain = pulseEnvelope(extPos);
    }
    else if( q >= 0.25*segStart && q < 0.75*segStart )
    {
      // switch in the middle of the flat segment, well away from both band-limited edges
      st.envGain = pulseEnvelope(drawnEdge ? 0.0 : envEdgeEnd);
      st.base    = pulse;
    }
    return st.base + (pulse - st.base) * st.envGain;
  }

  INLINE double BlendOscillator::pulseEnvelope(double extPos) const
  {
    const double x  = std::sqrt(std::min(std::max(extPos, 0.0), 1.0)) * (ENV_COLS-1);   // points at (i/(N-1))^2
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
      const double phaseShiftSamples = squarePhaseShift * tableLengthDbl / 360.0;
      double pulse;
      if( !circuitPulseShape )
        pulse = shapedPulse(saw1, tableNumber, phaseShiftSamples, pulseWidth, false, flatState);
      else
      {
        // bottom octaves: the envelope draws the circuit's slow rising edge (the wrapped segment starts ENV_LEAD early,
        // where the rise begins, so the band-limited step under it is only 0.02 high); upper octaves: the circuit's
        // edge is faster than the band limit, so the band-limited step forms it and the envelope holds over it. A
        // drawn edge only a few samples long aliases (-49 dB at 1 kHz), so crossfade by pitch, log-linear from
        // DRAWN_EDGE_FULL to DRAWN_EDGE_NONE.
        const double wd = drawnEdgeWeight;
        pulse = 0.0;
        if( wd > 0.0 )
          pulse += wd * shapedPulse(saw1, tableNumber, phaseShiftSamples, std::min(pulseWidth*(1.0 + envLead), 0.95), true, drawnState);
        if( wd < 1.0 )
          pulse += (1.0 - wd) * shapedPulse(saw1, tableNumber, phaseShiftSamples, pulseWidth, false, stepState);
      }
      out2 = blend * pulse * 0.5;  // 0.5 to match the original square level
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
