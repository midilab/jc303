#ifndef dfl_PhaseshaperOscillator_h
#define dfl_PhaseshaperOscillator_h

#include <cmath>
#include <initializer_list>

namespace dfl
{

  /**

  Single-oscillator phaseshaping algorithms from Kleimola, Lazzarini, Timoney and Valimaki,
  "Phaseshaping Oscillator Algorithms for Musical Sound Synthesis" (SMC 2010):

    TRI_MOD   Roland JP-8000 triangle modulation (Sec. 3.5, Eqs. 26a/b): a bipolar triangle
              xT = aTM * (2|2p - 1| - 1) folded back into [-1, 1] by y = 2(xT - ceil(xT - 0.5)).
              Odd harmonics only, the 3rd dominant; aTM sweeps formant-like sync timbres.
              Shape 0 -> aTM 0.5 (a plain triangle), shape 1 -> aTM 1 (the JP-8000 maximum).

    SUPERSAW  Ripple supersaw simulation (Sec. 3.6, Eq. 28): y = gb(sin(modm(x, m1) + modm(x, m2)))
              with x = a1 * p, a1 = 1.5 and m2 = 0.88 as in the paper. The sine acts on the ripple
              phase in radians, so only its rising quarter is used, as a soft saw. Shape sweeps the
              ripple amount m1 from 0.8 (few, wide ripples) to 0.2 (many fine ripples).

  Drift is the paper's LFO on m1 (Fig. 13), here also applied to aTM: a free-running 0.35 Hz sine
  that keeps the timbre moving like a detuned oscillator bank.

  The wraps are value discontinuities locked to the phase, repaired with a 2-point polyBLEP. Slope
  kinks (the triangle corners, the ripple edges) are left to the surrounding oversampling, as in
  AlphaJunoOscillator.

  */

  class PhaseshaperOscillator
  {

  public:

    enum Mode
    {
      TRI_MOD = 0,
      SUPERSAW,
      NUM_MODES
    };

    static constexpr double SUPERSAW_A1 = 1.5;    // phase scaling a1 (Fig. 12)
    static constexpr double SUPERSAW_M2 = 0.88;   // second ripple modulus m2 (Fig. 12)
    static constexpr double DRIFT_RATE  = 0.35;   // Hz

    static double triModAmountForShape(double shape) { return 0.5 + 0.5 * clip01(shape); }
    static double rippleForShape(double shape)       { return 0.8 - 0.6 * clip01(shape); }

    //---------------------------------------------------------------------------------------------
    // parameter settings:

    void setSampleRate(double newSampleRate)
    {
      if( newSampleRate > 0.0 )
        sampleRateRec = 1.0 / newSampleRate;
    }

    inline void setFrequency(double newFrequency)
    {
      if( newFrequency > 0.0 && newFrequency < 20000.0 )
        freq = newFrequency;
    }

    inline void calculateIncrement() { increment = freq * sampleRateRec; }

    void setMode(int newMode)
    {
      if( newMode >= 0 && newMode < NUM_MODES )
        mode = newMode;
    }

    /** Timbre, 0..1: the triangle modulation amount or the supersaw ripple. */
    inline void setShape(double newShape) { shape = clip01(newShape); }

    /** Depth of the slow LFO on the shape, 0..1. */
    inline void setDrift(double newDrift) { drift = clip01(newDrift); }

    /** Bandlimiting on (default) or the trivial waveform (for tests). */
    void setAntialias(bool on) { antialias = on; }

    void resetPhase() { phase = 0.0; }

    //---------------------------------------------------------------------------------------------
    // audio processing:

    inline double getSample()
    {
      double lfo = 0.0;
      if( drift > 0.0 )
      {
        lfo = std::sin(2.0 * PI * driftPhase);
        driftPhase += DRIFT_RATE * sampleRateRec;
        if( driftPhase >= 1.0 )
          driftPhase -= 1.0;
      }

      const double out = (mode == TRI_MOD) ? triModSample(lfo) : supersawSample(lfo);

      phase += increment;
      if( phase >= 1.0 )
        phase -= std::floor(phase);
      return out;
    }

    //=============================================================================================

  protected:

    static constexpr double PI  = 3.14159265358979323846;
    static constexpr double EPS = 1.0e-9;

    inline double triModSample(double lfo) const
    {
      const double a  = clip(triModAmountForShape(shape) + 0.12 * drift * lfo, 0.5, 1.0);
      const double dt = increment;
      double y = triMod(phase, a);
      if( ! antialias )
        return y;

      // folds where xT crosses +-0.5: |2p - 1| = (1 +- 0.5/a) / 2, each on both slopes
      if( a > 0.5 + EPS )
      {
        for( double level : { 0.5 / a, -0.5 / a } )
        {
          const double c = 0.5 * (1.0 + level);
          for( double edge : { 0.5 * (1.0 - c), 0.5 * (1.0 + c) } )
          {
            const double td = wrap(phase - edge);
            if( td < dt || td > 1.0 - dt )
              y += 0.5 * (triMod(edge + EPS, a) - triMod(edge - EPS, a)) * polyBlep(td, dt);
          }
        }
      }

      return y;
    }

    static inline double triMod(double p, double a)
    {
      const double xT = a * (2.0 * std::fabs(2.0 * wrap(p) - 1.0) - 1.0);
      return 2.0 * (xT - std::ceil(xT - 0.5));
    }

    inline double supersawSample(double lfo) const
    {
      const double m1 = rippleForShape(shape) * (1.0 + 0.25 * drift * lfo);
      const double a1 = SUPERSAW_A1;
      const double x  = a1 * phase;
      const double dt = increment;
      double y = supersaw(x, m1);
      if( ! antialias )
        return y;

      // ripple wraps at x = k*m inside the phase cycle: u drops by m there
      const double step = a1 * dt;   // x advance per sample
      for( double m : { m1, SUPERSAW_M2 } )
      {
        const double k = std::floor(x / m);
        const double past = x - k * m;
        if( k >= 1.0 && past < step )
          y += 0.5 * wrapJump(k * m, m, m1) * blepAfter(past / step);
        const double next = (k + 1.0) * m;
        if( next < a1 && next - x < step )
          y += 0.5 * wrapJump(next, m, m1) * blepBefore((x - next) / step);
      }

      // phase reset: x jumps from a1 back to 0
      if( phase < dt || phase > 1.0 - dt )
      {
        const double jump = -2.0 * std::sin(rippleLeft(a1, m1));
        y += 0.5 * jump * polyBlep(phase, dt);
      }
      return y;
    }

    static inline double supersaw(double x, double m1)
    {
      return 2.0 * std::sin(modm(x, m1) + modm(x, SUPERSAW_M2)) - 1.0;
    }

    // ripple phase u just before x
    static inline double rippleLeft(double x, double m1)
    {
      return modm(x - EPS, m1) + modm(x - EPS, SUPERSAW_M2);
    }

    // jump in y when the modulus-m term wraps at x
    static inline double wrapJump(double x, double m, double m1)
    {
      const double u = rippleLeft(x, m1);
      return 2.0 * (std::sin(u - m) - std::sin(u));
    }

    // 2-point polyBLEP residual for a jump of 2 (same form as AlphaJunoOscillator), scaled by half
    // the jump and added to the trivial waveform. x is the distance to the edge in samples: in [0, 1)
    // just after it, in (-1, 0] just before.
    static inline double blepAfter(double x)  { return x + x - x * x - 1.0; }
    static inline double blepBefore(double x) { return x * x + x + x + 1.0; }

    // the same at wrapped phase distance t past a phase-locked edge
    static inline double polyBlep(double t, double dt)
    {
      if( t < dt )
        return blepAfter(t / dt);
      if( t > 1.0 - dt )
        return blepBefore((t - 1.0) / dt);
      return 0.0;
    }

    static inline double modm(double x, double m) { return x - m * std::floor(x / m); }
    static inline double wrap(double x)           { return x - std::floor(x); }
    static inline double clip(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
    static inline double clip01(double x)         { return clip(x, 0.0, 1.0); }

    double sampleRateRec = 1.0 / 44100.0;
    double freq          = 440.0;
    double increment     = 0.0;
    double phase         = 0.0;
    double driftPhase    = 0.0;
    double shape         = 0.5;
    double drift         = 0.0;
    int    mode          = TRI_MOD;
    bool   antialias     = true;

  };

} // end namespace dfl

#endif // dfl_PhaseshaperOscillator_h
