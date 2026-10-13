#ifndef dfl_BlendOscillator_h
#define dfl_BlendOscillator_h

#include <cmath>

// rosic-includes:
#include "rosic_MipMappedWaveTable.h"

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
  saw table (a band-limited step) - so the pulse width becomes a continuous, per-sample
  parameter (setPulseWidth no longer regenerates any wavetable via FFT). This is what lets the
  pulse width track pitch the way the original TB-303 does. With setClassicSquare(true) the pulse
  is instead read from a second, pre-rendered wavetable (Open303's SQUARE303) exactly as the
  original class did, with a fixed pulse width.

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

    /** Sets the pre-rendered square table read in classic mode (see setClassicSquare). */
    void setWaveTable2(MipMappedWaveTable* newWaveTable2) { waveTable2 = newWaveTable2; }

    /** Classic mode reads the pulse from waveTable2 exactly like the original rosic::BlendOscillator: fixed pulse
    width, tanh drive/offset/phase shift baked into the table. Otherwise the pulse is synthesised from the saw. */
    void setClassicSquare(bool enabled) { classicSquare = enabled; }

    /** Sets the blend/mix factor between saw and pulse. The value is expected between 0...1 where 0
    means saw only, 1 means pulse only - in between there will be a linear blend between the two. */
    void setBlendFactor(double newBlendFactor) { blend = newBlendFactor; }

    /** Sets the frequency of the oscillator. */
    INLINE void setFrequency(double newFrequency);

    /** Sets the pulse width of the synthesised pulse wave (in percent, 0...100). Cheap - just
    stores a value, no wavetable regeneration, so it is safe to call at audio rate. */
    INLINE void setPulseWidth(double newPulseWidth);

    /** Sets the phase shift (in degrees) of the pulse wave relative to the saw. */
    void setSquarePhaseShift(double newShift) { squarePhaseShift = newShift; }

    /** Enables the TB-303 waveshaper envelope on the pulse (on by default): the collector-high segment follows the
    circuit's shape instead of a flat top (see ENV_TABLE). */
    void setCircuitPulseShape(bool enabled) { circuitPulseShape = enabled; }

    /** Scales the waveshaper envelope (with setCircuitPulseShape on): 0 = flat top, 1 = the circuit, up to 2 =
    exaggerated (each envelope node v becomes v^2 at 2: slower rise, deeper sag). Below 1 the circuit's rise is
    compressed in time by the sag around the nominal edge and the top's droop is scaled by it, so a smaller sag is a
    faster transistor rather than a partial hard step. Either way the drawn envelope stays piecewise linear and its
    corners stay band-limited. */
    void setPulseSag(double newSag) { pulseSag = std::min(std::max(newSag, 0.0), 2.0); }

    /** Sets the phase increment from outside. */
    INLINE void setIncrement(double newIncrement) { increment = newIncrement; }

    //---------------------------------------------------------------------------------------------
    // inquiry:

    /** Returns the blend/mix factor between saw and pulse (0...1). */
    double getBlendFactor() const { return blend; }

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
    double squarePhaseShift;  // phase shift of pulse relative to saw (degrees)

    MipMappedWaveTable *waveTable1; // the saw table both saws are read from
    MipMappedWaveTable *waveTable2 = nullptr; // the pre-rendered square read in classic mode
    bool   classicSquare = false;
    double pulseSag = 1.0;

    // TB-303 waveshaper (Q8) envelope. The real square is not flat-topped: in the bottom octave the collector rises
    // slowly after the edge, and at every pitch the top slopes down as the saw keeps falling (the transistor is not
    // fully switched), while the low level (transistor off) is constant. ENV_TABLE is the height of the
    // collector-high segment from an ngspice model of the stage (tools/osc-fidelity/gen_pulse_envelope.py), relative to the
    // high-pitch reference height (about 3.51 V), at ENV_FREQ pitches. The table spans the segment from where the
    // rise starts (level 0.02, ENV_LEAD of the duty before the 50% crossing, about 1.5% of the cycle: the saw crossing
    // the transistor's soft threshold) to the falling 50% crossing, at points (i / (ENV_COLS - 1))^2 of it, so the
    // envelope draws the rising edge itself and the band-limited step under it is only 0.02 high. The falling edge
    // (the saw reset, under 1 us in the circuit) is the band-limited step. Between the nodes the envelope is linear
    // in time, and every node corner gets a polyBLAMP, so the drawn edge stays alias-free even where it is shorter
    // than a sample (see drawnEnvelope).
    static constexpr int ENV_ROWS = 17, ENV_COLS = 48;
    static constexpr double ENV_FREQ[ENV_ROWS] = { 10, 15, 22, 27.5, 32.7032, 41.2, 49, 55, 65.4064, 82.4, 98, 130.81, 196, 261.63, 523.25, 1000, 2000 };
    static constexpr double ENV_LEAD[ENV_ROWS] = { 0.0456, 0.0264, 0.0237, 0.0249, 0.0262, 0.0282, 0.0294, 0.0302, 0.0313, 0.0324, 0.0330, 0.0337, 0.0348, 0.0362, 0.0433, 0.0556, 0.0723 };
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
      { 0.020, 0.021, 0.024, 0.029, 0.039, 0.057, 0.088, 0.143, 0.237, 0.390, 0.617, 0.902, 1.011, 1.013, 1.015, 1.016, 1.017, 1.017, 1.017, 1.017, 1.017, 1.017, 1.016, 1.015, 1.015, 1.014, 1.013, 1.012, 1.011, 1.010, 1.009, 1.007, 1.006, 1.005, 1.003, 1.002, 1.000, 0.999, 0.997, 0.995, 0.994, 0.992, 0.990, 0.988, 0.986, 0.984, 0.982, 0.980 },
      { 0.020, 0.021, 0.023, 0.028, 0.035, 0.048, 0.069, 0.103, 0.160, 0.250, 0.386, 0.579, 0.822, 1.051, 1.055, 1.057, 1.059, 1.060, 1.060, 1.061, 1.061, 1.062, 1.062, 1.062, 1.062, 1.062, 1.061, 1.061, 1.061, 1.060, 1.060, 1.059, 1.059, 1.058, 1.058, 1.057, 1.056, 1.055, 1.055, 1.054, 1.053, 1.052, 1.051, 1.050, 1.049, 1.048, 1.047, 1.045 },
      { 0.020, 0.020, 0.022, 0.026, 0.032, 0.041, 0.056, 0.079, 0.114, 0.168, 0.248, 0.361, 0.515, 0.708, 0.928, 1.091, 1.093, 1.094, 1.095, 1.096, 1.097, 1.098, 1.098, 1.099, 1.099, 1.099, 1.099, 1.100, 1.100, 1.100, 1.100, 1.100, 1.100, 1.099, 1.099, 1.099, 1.099, 1.099, 1.098, 1.098, 1.098, 1.097, 1.097, 1.096, 1.096, 1.095, 1.095, 1.092 } };
    static constexpr double SAW_TABLE_RESET = 0.5;   // phase (0..1) of the saw table's reset
    bool   circuitPulseShape = true;
    int    envRow = 0;            // ENV_FREQ row below the current frequency (set in calculateIncrement)
    double envRowFrac = 0.0;      // interpolation weight towards envRow + 1
    double envLead = 0.0;         // ENV_LEAD at the current frequency
    // below a sag of 1 the envelope is the circuit's with its rise compressed in time by the sag around the nominal
    // edge, up to node warpK (the rise's peak), and the rest stretched to fill the segment; the lead before the edge
    // shrinks by the same factor, so the rise still starts where the segment does (see setupSagWarp)
    bool   warp = false;
    int    warpK = 0;
    double warpPc = 0.0, warpPcTable = 0.0, warpSlope = 1.0, warpPK = 0.0, warpQK = 0.0, warpRiseScale = 1.0;
    INLINE void   setupSagWarp(double pwCircuit, double pwExtended);
    INLINE double rawNode(int i) const;
    INLINE double nodePos(int i) const;
    INLINE double warpBack(double x) const;
    INLINE double pulseEnvelope(double extPos) const;
    INLINE double envNode(int i) const;
    INLINE double drawnEnvelope(double extPos, double dp, double flatLen) const;
    static INLINE double blampResidual(double t);
    INLINE double shapedPulse(double saw1, int tableNumber, double phaseShiftSamples, double pw) const;

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
  }

  INLINE double BlendOscillator::shapedPulse(double saw1, int tableNumber, double phaseShiftSamples, double pw) const
  {
    double phase2 = phaseIndex + pw*tableLengthDbl + phaseShiftSamples;
    while( phase2 >= tableLengthDbl )
      phase2 -= tableLengthDbl;
    while( phase2 < 0.0 )
      phase2 += tableLengthDbl;
    const int    intIndex2 = floorInt(phase2);
    const double saw2      = waveTable1->getValueLinear(intIndex2, phase2 - (double) intIndex2, tableNumber);

    // The saw table is linear (slope 2 per cycle), so saw2 - saw1 is a band-limited step between two constant
    // levels: 2*pw outside the wrapped segment and 2*pw - 2 inside it (saw1 is just before the reset, saw2 just
    // after). Map it to the pulse's levels, +1 inside the segment and -1 outside (negated to match the original
    // 303 square-wave polarity); the circuit's shape comes from the waveshaper envelope below, not from a tanh,
    // which would only clip the step's Gibbs ripple and alias (-31..-40 dB below 20 kHz at 4x).
    const double rawLo   = 2.0*pw - 2.0;
    const double levelLo = 1.0, levelHi = -1.0;
    double pulse = levelLo + 0.5*(saw2 - saw1 - rawLo)*(levelHi - levelLo);
    if( !circuitPulseShape || pulseSag <= 0.0 )
      return pulse;

    // the collector-high segment runs from the second read's reset to the first read's (the saw reset, as in the
    // circuit); the 303 saw table resets at mid-table, so measured from there it is [1 - pw, 1) of the cycle
    double q = (phaseIndex + phaseShiftSamples) / tableLengthDbl + SAW_TABLE_RESET;
    q -= std::floor(q);
    const double segStart = 1.0 - pw;
    // the segment is the extended one (it starts ENV_LEAD early, where the circuit's rise begins); the envelope is
    // evaluated every sample as a function of the unwrapped segment position, with band-limited corners (see
    // drawnEnvelope); outside the segment the table's end values hold, so only the corner residuals reach into the
    // flat segment
    const double flatLen = segStart / pw;   // the flat segment's length in extended-segment units
    double p = (q - segStart) / pw;
    if( p < -0.5*flatLen )
      p += 1.0/pw;
    const double envGain = drawnEnvelope(p, increment / (tableLengthDbl*pw), flatLen);
    // the pulse's level outside the segment is levelHi exactly (inside the segment saw2 - saw1 = 2*pw - 2, so the
    // pulse sits at levelLo there); sampling it from the band-limited pulse would freeze a little of the Gibbs ripple
    // at an arbitrary fractional time, a small hard step every cycle
    return levelHi + (pulse - levelHi) * envGain;
  }

  INLINE double BlendOscillator::rawNode(int i) const
  {
    return ENV_TABLE[envRow][i] + envRowFrac*(ENV_TABLE[envRow+1][i] - ENV_TABLE[envRow][i]);
  }

  INLINE void BlendOscillator::setupSagWarp(double pwCircuit, double pwExtended)
  {
    warp = false;
    int k = 0;
    for( int i = 1; i < ENV_COLS; i++ )
      if( rawNode(i) > rawNode(k) )
        k = i;
    const double N1 = ENV_COLS-1;
    warpK  = k;
    warpPK = (k/N1)*(k/N1);
    // the nominal edge in the table's units (lead at full length) and in this segment's (lead shrunk by the sag);
    // a table position's time from the edge, scaled by the sag, lands at warpPc + warpSlope*(p - warpPcTable)
    warpPcTable = 1.0 - pulseWidth/pwCircuit;
    warpPc      = 1.0 - pulseWidth/pwExtended;
    warpSlope   = pulseSag * pwCircuit/pwExtended;
    if( warpPK <= warpPcTable )
      return;
    warpQK        = warpPc + warpSlope*(warpPK - warpPcTable);
    warpRiseScale = (1.0 + pulseSag*(rawNode(k) - 1.0)) / rawNode(k);
    warp          = true;
  }

  // position of node i in the (possibly warped) envelope
  INLINE double BlendOscillator::nodePos(int i) const
  {
    const double N1 = ENV_COLS-1;
    const double p  = (i/N1)*(i/N1);
    if( !warp )
      return p;
    if( i <= warpK )
      return warpPc + warpSlope*(p - warpPcTable);
    return warpQK + (p - warpPK)*(1.0 - warpQK)/(1.0 - warpPK);
  }

  // the table position a warped position x reads from
  INLINE double BlendOscillator::warpBack(double x) const
  {
    if( !warp )
      return x;
    if( x <= warpQK )
      return warpPcTable + (x - warpPc)/warpSlope;
    return warpPK + (x - warpQK)*(1.0 - warpPK)/(1.0 - warpQK);
  }

  INLINE double BlendOscillator::envNode(int i) const
  {
    const double v = rawNode(i);
    if( warp )
      return i <= warpK ? v*warpRiseScale : 1.0 + pulseSag*(v - 1.0);
    if( pulseSag == 1.0 )
      return v;
    if( pulseSag < 1.0 )
      return 1.0 + pulseSag*(v - 1.0);
    return v * (1.0 + (pulseSag - 1.0)*(v - 1.0));   // v^2 at a sag of 2
  }

  // The envelope is piecewise linear in segment position between the table nodes at (i/(N-1))^2, so in time it is a
  // chain of straight ramps whose only non-band-limited features are the slope changes (corners) at the nodes.
  INLINE double BlendOscillator::pulseEnvelope(double extPos) const
  {
    const double N1 = ENV_COLS-1;
    const double p  = std::min(std::max(extPos, 0.0), 1.0);
    if( p <= nodePos(0) )
      return envNode(0);
    const double xs = std::sqrt(std::min(std::max(warpBack(p), 0.0), 1.0))*N1;
    const int    i  = std::min((int) xs, ENV_COLS-2);
    const double y0 = envNode(i);
    const double p0 = nodePos(i), p1 = nodePos(i+1);
    return y0 + (p - p0)/(p1 - p0) * (envNode(i+1) - y0);
  }

  // 4-point polyBLAMP residual (cubic B-spline kernel): band-limited unit-slope-change corner minus the naive one,
  // t = samples since the corner. r(t) = (2-|t|)^5/120 for 1 <= |t| < 2, 7/30 - |t|/2 + t^2/3 - t^4/12 + |t|^5/40 for |t| < 1.
  static constexpr double BLAMP_BEFORE = 2.0, BLAMP_AFTER = 2.0;   // support of the residual around the corner
  INLINE double BlendOscillator::blampResidual(double t)
  {
    const double a = std::fabs(t);
    if( a >= 2.0 )
      return 0.0;
    if( a >= 1.0 )
    {
      const double b = 2.0 - a, b2 = b*b;
      return b2*b2*b * (1.0/120.0);
    }
    const double a2 = a*a;
    return 7.0/30.0 + a*(-0.5 + a2*(-1.0/12.0*a + a2*(1.0/40.0))) + a2*(1.0/3.0);
  }

  // Envelope at extended-segment position extPos (may run a little below 0 or above 1 around the segment ends, where
  // the end nodes' values hold) with each node corner replaced by its band-limited version: the slope change at
  // node i (in level per sample: node-to-node chord slopes times dp, the position advanced per sample) times the
  // BLAMP residual at the corner's fractional sample time. This makes the rising edge alias-free at any pitch, even
  // when it is shorter than a sample, because every corner lands at its exact fractional time.
  INLINE double BlendOscillator::drawnEnvelope(double extPos, double dp, double flatLen) const
  {
    const double N1 = ENV_COLS-1;
    double env;
    if( extPos >= 0.0 && extPos <= 1.0 )
      env = pulseEnvelope(extPos);
    else
    {
      // Flat segment (the band-limited step is ~0 here, but not exactly: Gibbs ripple and the saw table's two
      // slightly different half-slopes leak through). The envelope must not jump from the table's end value back to
      // its start value, so glide between them with a smoothstep over the part of the flat segment that lies
      // beyond the corner residuals' reach of the last and first node (the corner model holds a zero slope there).
      const double x  = extPos > 0.5 ? (extPos - 1.0)/flatLen : 1.0 + extPos/flatLen;   // 0..1 across the flat segment
      const double xa = std::max(0.25, BLAMP_AFTER*dp/flatLen), xb = std::min(0.75, 1.0 - BLAMP_BEFORE*dp/flatLen);
      const double u  = xb > xa ? std::min(std::max((x - xa)/(xb - xa), 0.0), 1.0) : (x < 0.5 ? 0.0 : 1.0);
      const double w  = u*u*(3.0 - 2.0*u);
      const double yEnd = envNode(ENV_COLS-1), yStart = envNode(0);
      env = yEnd + (yStart - yEnd)*w;
    }
    const double lo = extPos - BLAMP_AFTER*dp, hi = extPos + BLAMP_BEFORE*dp;
    if( hi <= 0.0 || lo >= 1.0 )
      return env;
    // node indices whose corners lie in [lo, hi]: the warp is monotone, so map the window back onto the table
    const double wl = warpBack(lo), wh = warpBack(hi);
    const int i0 = wl <= 0.0 ? 0 : (int) std::ceil(std::sqrt(std::min(wl, 1.0))*N1);
    const int i1 = wh >= 1.0 ? ENV_COLS-1 : (int) (std::sqrt(std::max(wh, 0.0))*N1);
    // chord slope (level per unit position) into node i0; zero before node 0 and after the last node
    double yPrev = envNode(std::max(i0-1, 0)), yCur = envNode(i0);
    double pPrev = nodePos(i0-1), pCur = nodePos(i0);
    double slopeIn = i0 > 0 ? (yCur - yPrev)/(pCur - pPrev) : 0.0;
    for( int i = i0; i <= i1; i++ )
    {
      double slopeOut = 0.0, yNext = yCur, pNext = pCur;
      if( i < ENV_COLS-1 )
      {
        yNext = envNode(i+1); pNext = nodePos(i+1);
        slopeOut = (yNext - yCur)/(pNext - pCur);
      }
      env += (slopeOut - slopeIn)*dp * blampResidual((extPos - pCur)/dp);
      slopeIn = slopeOut; yCur = yNext; pCur = pNext;
    }
    return env;
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

    if( classicSquare && waveTable2 != NULL )
    {
      out2  = blend * waveTable2->getValueLinear(intIndex, frac, tableNumber);
      out2 *= 0.5;
      phaseIndex += increment;
      return out1 + out2;
    }

    // pulse component: synthesise from a second, phase-shifted read of the same saw table - this is
    // what makes the pulse width a continuous, per-sample value.
    if( blend > 0.0 )
    {
      const double phaseShiftSamples = squarePhaseShift * tableLengthDbl / 360.0;
      // with the circuit envelope the wrapped segment starts ENV_LEAD earlier, where the circuit's rise begins; the
      // envelope then reaches the 50% level at the nominal pulse width, so the duty is unchanged
      const bool   shaped = circuitPulseShape && pulseSag > 0.0;
      const double pw     = shaped ? std::min(pulseWidth*(1.0 + envLead*std::min(pulseSag, 1.0)), 0.95) : pulseWidth;
      if( shaped && pulseSag < 1.0 )
        setupSagWarp(std::min(pulseWidth*(1.0 + envLead), 0.95), pw);
      else
        warp = false;
      const double pulse = shapedPulse(saw1, tableNumber, phaseShiftSamples, pw);
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
