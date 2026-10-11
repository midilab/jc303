#ifndef dfl_TB303OutputNetwork_h
#define dfl_TB303OutputNetwork_h

#include <algorithm>
#include <cmath>

namespace dfl
{

  /**
  Diode Native's output path, from the ladder top to the output jack, as linear coupling filters. It replaces Open303's
  fitted allpass / highpass / notch chain for that model only. Values from the TB-303 service notes; checked against
  SPICE (tools/diode-fidelity/spice/tb303_vca.py, tools/diode-fidelity/native_output.cpp), within 0.25 dB above 20 Hz.

  - C27/C25 into the Q21 bases: the same pole as the feedback network's dominant one (setCouplingPole), set by Q21's
    input impedance and so by its beta.
  - C14 into the resonance pot: 3.2-4.0 Hz across the pot.
  - The VCA input sums two taps of the filter output: R121 220k + C21 10n from the filter output, and R122 100k + C22 10n
    from the resonance-pot wiper, into 2.2k. The wiper tap raises the level by up to 10 dB at full resonance (making up
    about half of the ladder's resonance passband loss) and its own corner (140-156 Hz) moves the bass corner up.
  - VCA, mixer and line out: three subsonic poles (fitted to SPICE within 0.04 dB / 0.4 degrees, 2 Hz-2 kHz).

  Unity gain at high frequency with the pot at 0.
  */

  class TB303OutputNetwork
  {

  public:

    void setSampleRate(double newSampleRate)
    {
      if( newSampleRate > 0.0 )
        sampleRate = newSampleRate;
      updateCoefficients();
    }

    /** Resonance pot rotation, 0..1 (DiodeLadderFilter::getNativePot). */
    void setPot(double newPot)
    {
      pot = std::clamp(newPot, 0.0, 1.0);
      updateCoefficients();
    }

    /** Corner (Hz) of the C27/C25 coupling into Q21 (DiodeLadderFilter::getNativeCouplingPoleHz). */
    void setCouplingPole(double newHz)
    {
      couplingHz = newHz;
      updateCoefficients();
    }

    void reset()
    {
      for(double& s : state) s = 0.0;
    }

    double getSample(double in)
    {
      double x = highpass(Q21, highpass(C14, in));
      x = highpass(TAP_A, x) + tapBGain * highpass(TAP_B, x);
      for(int i = POST1; i < NUM_STAGES; ++i)
        x = highpass(i, x);
      return x;
    }

  protected:

    enum Stage { Q21, C14, TAP_A, TAP_B, POST1, POST2, POST3, NUM_STAGES };

    static constexpr double POT_R       = 50e3;    // VR4 resonance, linear
    static constexpr double WIPER_LOAD  = 96e3;    // C6 / R11 / Q18 follower on the wiper
    static constexpr double TAP_A_R     = 220e3;   // R121
    static constexpr double TAP_B_R     = 100e3;   // R122
    static constexpr double TAP_C       = 10e-9;   // C21, C22
    static constexpr double OTA_R       = 2.2e3;   // R124 into the VCA's +input
    static constexpr double POST_HZ[3]  = { 1.25, 4.75, 8.25 };

    // TPT one-pole highpass: x - lowpass(x)
    double highpass(int i, double x)
    {
      const double v = (x - state[i]) * coeff[i];
      const double lp = v + state[i];
      state[i] = lp + v;
      return x - lp;
    }

    void setCorner(int i, double hz)
    {
      const double g = std::tan(M_PI * std::min(hz, 0.45 * sampleRate) / sampleRate);
      coeff[i] = g / (1.0 + g);
    }

    void updateCoefficients()
    {
      // the pot's lower section is shunted by the wiper load; the wiper tap sees the divider's Thevenin equivalent
      const double lower = pot * POT_R * WIPER_LOAD / (pot * POT_R + WIPER_LOAD);
      const double upper = (1.0 - pot) * POT_R;
      const double wiper = lower / (lower + upper);
      const double rth   = lower * upper / (lower + upper);
      tapBGain = wiper * (TAP_A_R + OTA_R) / (TAP_B_R + OTA_R + rth);   // relative to tap A
      setCorner(Q21, couplingHz);
      setCorner(C14, 3.2 + 0.8 * pot * pot);
      setCorner(TAP_A, 1.0 / (2.0 * M_PI * (TAP_A_R + OTA_R) * TAP_C));
      setCorner(TAP_B, 1.0 / (2.0 * M_PI * (TAP_B_R + OTA_R + rth) * TAP_C));
      for(int i = 0; i < 3; ++i)
        setCorner(POST1 + i, POST_HZ[i]);
    }

    double sampleRate = 44100.0;
    double pot        = 0.0;
    double couplingHz = 92.0;
    double tapBGain   = 0.0;
    double coeff[NUM_STAGES] = {};
    double state[NUM_STAGES] = {};
  };

} // end namespace dfl

#endif // dfl_TB303OutputNetwork_h
