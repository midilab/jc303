#ifndef dfl_AlphaJunoOscillator_h
#define dfl_AlphaJunoOscillator_h

#include <cmath>

namespace dfl
{

  /**

  Roland Alpha Juno (DCO) sawtooth waveform family, reconstructed from the
  DCO's binary-counter architecture. A single phase accumulator drives a rising
  ramp; higher-octave "chopping" gates (derived from counter bits at +1 and +3
  octaves) switch the ramp on/off, exactly as the hardware derives its
  synchronous switching waveforms.

    SAW01  plain saw                         (ramp)
    SAW02  saw chopped by +1 octave, 50%     (ramp * gate(2f, 50%))
    SAW03  saw chopped by +1 octave, PW      (ramp * gate(2f, d))   <- the "money" wave
    SAW04  saw chopped by +3 octaves, 50%    (ramp * gate(8f, 50%))
    SAW05  saw chopped by +1 AND +3 octaves  (ramp * gate(2f)*gate(8f))

  Because the chopping introduces hard value discontinuities that are all
  phase-locked to the oscillator, we generate the naive (aliased) waveform and
  repair each discontinuity with a 2-point polyBLEP residual. This gives
  audio-rate pulse-width control (d) with clean bandlimiting - the wavetable
  approach used by BlendOscillator cannot sweep PW without re-running an FFT,
  which is why this uses a realtime accumulator instead.

  The pulse width d is applied to the +1-octave gate (SAW02/03/05). Slope
  discontinuities (which alias ~12 dB/oct more weakly than value jumps) are
  currently left for the surrounding oversampling to attenuate; a polyBLAMP
  pass is the natural next refinement.

  */

  class AlphaJunoOscillator
  {

  public:

    enum Waveform
    {
      SAW01 = 0,  // plain saw
      SAW02,      // +1 octave chop, 50%
      SAW03,      // +1 octave chop, adjustable PW
      SAW04,      // +3 octave chop, 50%
      SAW05,      // +1 and +3 octave chop
      NUM_WAVEFORMS
    };

    AlphaJunoOscillator()
    {
      sampleRate    = 44100.0;
      sampleRateRec = 1.0 / 44100.0;
      freq          = 440.0;
      increment     = 0.0;
      phase         = 0.0;
      duty          = 0.5;
      waveform      = SAW03;
      buildEdges();
    }

    //---------------------------------------------------------------------------------------------
    // parameter settings:

    void setSampleRate(double newSampleRate)
    {
      if( newSampleRate > 0.0 )
      {
        sampleRate    = newSampleRate;
        sampleRateRec = 1.0 / newSampleRate;
      }
    }

    inline void setFrequency(double newFrequency)
    {
      if( (newFrequency > 0.0) && (newFrequency < 20000.0) )
        freq = newFrequency;
    }

    /** Selects the waveform (see Waveform enum). */
    void setWaveform(int newWaveform)
    {
      if( newWaveform >= 0 && newWaveform < NUM_WAVEFORMS && newWaveform != waveform )
      {
        waveform = newWaveform;
        buildEdges();
      }
    }

    int getWaveform() const { return waveform; }

    /** Sets the +1-octave chopping pulse width, in percent (1-99). Only affects
    SAW02/03/05. Safe to call at audio rate (block rate). */
    inline void setPulseWidth(double pulseWidthPercent)
    {
      double d = pulseWidthPercent * 0.01;
      if( d < 0.02 ) d = 0.02;   // keep a nonzero off-interval so edges don't collide
      if( d > 0.98 ) d = 0.98;
      if( d != duty )
      {
        duty = d;
        if( waveform == SAW02 || waveform == SAW03 || waveform == SAW05 )
          buildEdges();
      }
    }

    /** Saw <-> square morph, 0..1 (0 = selected saw waveform, 1 = pulse of
    width `duty`). Driven by the main Waveform knob. */
    inline void setSquareBlend(double newBlend)
    {
      squareBlend = (newBlend < 0.0) ? 0.0 : (newBlend > 1.0 ? 1.0 : newBlend);
    }

    inline void calculateIncrement()
    {
      increment = freq * sampleRateRec;   // cycles per sample (phase in [0,1))
    }

    inline void setIncrement(double newIncrement) { increment = newIncrement; }
    inline double getIncrement() const { return increment; }

    //---------------------------------------------------------------------------------------------
    // audio processing:

    inline double getSample()
    {
      const double dt = increment;
      double p = phase;

      // --- selected saw-family waveform (bandlimited via polyBLEP) ---
      // naive (aliased) value: rising ramp where the chopping gate is open, else 0
      double sawOut = gateOpen(p) ? (2.0 * p - 1.0) : 0.0;

      // repair every phase-locked discontinuity with a 2-point polyBLEP residual.
      // each edge is evaluated every sample; polyBLEP is ~0 except within dt of the edge.
      for( int i = 0; i < numEdges; i++ )
      {
        double td = p - edgePhase[i];
        td -= std::floor(td);                 // wrapped distance past the edge, in [0,1)
        sawOut += 0.5 * edgeJump[i] * polyBlep(td, dt);
      }

      double out = sawOut;

      // --- optional saw <-> square morph (Waveform knob) ---
      // A bipolar pulse of width `duty`: +1 on [0,duty), -1 on [duty,1). Two
      // value-jump edges (+2 at phase 0, -2 at phase duty), bandlimited the same
      // way. Blends against the saw-family output; both keep their own BLEPs.
      if( squareBlend > 0.0 )
      {
        double fp = p - std::floor(p);
        double pulseOut = (fp < duty) ? 1.0 : -1.0;
        pulseOut += 0.5 * (+2.0) * polyBlep(fp, dt);                          // rising edge at phase 0
        double td = p - duty; td -= std::floor(td);
        pulseOut += 0.5 * (-2.0) * polyBlep(td, dt);                          // falling edge at phase duty

        out = (1.0 - squareBlend) * sawOut + squareBlend * pulseOut;
      }

      phase += dt;
      if( phase >= 1.0 )
        phase -= std::floor(phase);

      return out;
    }

    void resetPhase() { phase = 0.0; }

    /** Sets the phase directly (0..1). Used by the supersaw ensemble to
    randomize phases on note-on. */
    void setPhase(double p) { phase = p - std::floor(p); }

    //=============================================================================================

  protected:

    static const int MAX_EDGES = 16;

    // 2-point polyBLEP residual, normalized so that subtracting it from a
    // (2*phase-1) ramp yields a bandlimited sawtooth (jump of 2 at phase 0/1).
    static inline double polyBlep(double t, double dt)
    {
      if( t < dt )
      {
        double x = t / dt;
        return x + x - x * x - 1.0;          // just after the edge
      }
      if( t > 1.0 - dt )
      {
        double x = (t - 1.0) / dt;
        return x * x + x + x + 1.0;           // just before the edge
      }
      return 0.0;
    }

    // is the chopping gate open at this phase (i.e. is the ramp passed through)?
    inline bool gateOpen(double p) const
    {
      switch( waveform )
      {
      case SAW01: return true;
      case SAW02: return frac(2.0 * p) < 0.5;
      case SAW03: return frac(2.0 * p) < duty;
      case SAW04: return frac(8.0 * p) < 0.5;
      case SAW05: return (frac(2.0 * p) < duty) && (frac(8.0 * p) < 0.5);
      default:    return true;
      }
    }

    static inline double frac(double x) { return x - std::floor(x); }

    // ramp value at phase p (the "on" value)
    static inline double ramp(double p) { return 2.0 * p - 1.0; }

    /** Precomputes the list of discontinuity phases and their value jumps
    (valueAfter - valueBefore) for the current waveform / duty. Called whenever
    waveform or duty changes - never on the per-sample path. */
    void buildEdges()
    {
      numEdges = 0;

      if( waveform == SAW01 )
      {
        // plain saw: single reset, +1 -> -1 (jump of -2), no gate
        addEdge(0.0, -2.0);
        return;
      }

      // Collect the on/off boundary phases for the active gate(s), then emit an
      // edge at each. At an "on" boundary the value jumps 0 -> ramp(p); at an
      // "off" boundary ramp(p) -> 0.
      double bounds[MAX_EDGES];
      int    n = 0;

      if( waveform == SAW02 || waveform == SAW03 )
      {
        // +1-octave gate, duty d: on-intervals [0, d/2) and [0.5, 0.5 + d/2)
        double d = (waveform == SAW02) ? 0.5 : duty;
        bounds[n++] = 0.0;            // on
        bounds[n++] = d * 0.5;        // off
        bounds[n++] = 0.5;            // on
        bounds[n++] = 0.5 + d * 0.5;  // off
      }
      else if( waveform == SAW04 )
      {
        // +3-octave gate, 50%: on-intervals [k/8, k/8 + 1/16), k = 0..7
        for( int k = 0; k < 8; k++ )
        {
          bounds[n++] = k / 8.0;              // on
          bounds[n++] = k / 8.0 + 1.0 / 16.0; // off
        }
      }
      else if( waveform == SAW05 )
      {
        // +1 AND +3 octave gates. Sample the combined gate on a 1/16 grid
        // (the finest gate boundary) and emit an edge wherever it toggles.
        bool prev = gateOpen(15.5 / 16.0);    // state in the interval ending at phase 0
        for( int j = 0; j < 16; j++ )
        {
          double p = j / 16.0;
          bool cur = gateOpen((j + 0.5) / 16.0);
          if( cur != prev )
            bounds[n++] = p;
          prev = cur;
        }
      }

      // emit edges with correct jump sign, determined by whether the gate is
      // open just after the boundary
      for( int i = 0; i < n; i++ )
      {
        double p = bounds[i];
        bool onAfter = gateOpen(p + 1e-9);
        double jump = onAfter ? ramp(p) : -ramp(p);
        addEdge(p, jump);
      }
    }

    inline void addEdge(double p, double jump)
    {
      if( numEdges < MAX_EDGES )
      {
        edgePhase[numEdges] = p - std::floor(p);
        edgeJump[numEdges]  = jump;
        numEdges++;
      }
    }

    double sampleRate;
    double sampleRateRec;
    double freq;
    double increment;   // cycles per sample (phase advance)
    double phase;       // current phase in [0,1)
    double duty;        // +1-octave chopping pulse width as 0-1 fraction (also pulse width for square morph)
    double squareBlend = 0.0;  // 0 = saw waveform, 1 = square/pulse
    int    waveform;

    // precomputed discontinuity table for the current waveform/duty
    int    numEdges = 0;
    double edgePhase[MAX_EDGES];
    double edgeJump[MAX_EDGES];  // valueAfter - valueBefore

  };

} // end namespace dfl

#endif // dfl_AlphaJunoOscillator_h
