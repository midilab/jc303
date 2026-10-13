// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Lowenfels

#ifndef dfl_RolandFilter_h
#define dfl_RolandFilter_h

// Roland IR3109 four-pole low-pass filter with a BA662 resonance VCA, circuit-native: the Juno-106's 80017A module
// (values from dksynth's reverse engineering / Open80017a; the Juno-6 / 60 topology). The IR3109 on +-15 V with
// 270 pF, a 4.7k input, and a BA662 OTA as the resonance VCA: Out4 into its - input, the filter input into its + input
// (the passband compensation), its output current into the first stage; the RES CV sets its Iabc, and its tanh limits
// the self-oscillation. (The SH-101 / MC-202 use the same IR3109 core with a different resonance circuit.)
//
// Each IR3109 stage is an undegenerated OTA (Iabc tanh(Vd / 2 VT)) into its capacitor, with the output's compliance
// to the rails, and a PMOS follower (a level shift with a ceiling below the supply) whose output closes the stage's
// local 68k feedback. Each sample solves the four capacitor nodes (trapezoidal capacitors, Newton). Checked against
// the ngspice model in tools/roland-fidelity/spice (see tools/roland-fidelity/README.md).
//
// The output passes through the VCA's BA662 input (an OTA: its tanh depends only on the input level, so it is a
// static stage here, at the circuit's level; Open303's own VCA applies the envelope after it).
//
// Character controls (shared with the MS-20, 0 / 0.5 = the circuit): Bite = current-mirror mismatch in the stage
// OTAs, Clip = the resonance OTA's input scale, Sharpness = the stage OTAs' VT, Warmth = the VCA drive. Saturation
// Bias offsets stage 1's input pair, Saturation Rail sags the +-15 V supplies (the stage OTAs' output compliance and
// the followers' ceiling).
//
// The LP / BP / HP morph mixes the stage outputs (binomial taps of the cascade, signs chosen so the crossfades have
// no notches): the circuit runs unchanged, with its resonance, in every mode.

// standard-library includes:
#include <stdlib.h>
#include <cmath>
#include <algorithm>

// rosic includes:
#include "rosic_RealFunctions.h"
#include "GlobalFunctions.h"
#include "dfl_CircuitSolve.h"


namespace dfl
{

  class RolandFilter
  {

  public:

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. */
    RolandFilter();

    /** Destructor. */
    ~RolandFilter();

    //---------------------------------------------------------------------------------------------
    // parameter settings:

    void setSampleRate(double newSampleRate);
    INLINE void setCutoff(double newCutoff, bool updateCoefficients = true);
    INLINE void setResonance(double newResonance, bool updateCoefficients = true);
    void setInputDrive(double newDrive);
    /** Response mode, 0 = LP, 0.5 = BP, 1 = HP (mixes the stage outputs). Cheap, so it may change per sample. */
    void setMode(double newMode) { mode = std::clamp(newMode, 0.0, 1.0); }

    //---------------------------------------------------------------------------------------------
    // inquiry:

    double getCutoff() const { return cutoff; }
    double getResonance() const { return resonance; }
    double getDrive() const { return drive; }
    double getMode() const { return mode; }

    //---------------------------------------------------------------------------------------------
    // audio processing:

    INLINE double getSample(double in);

    //---------------------------------------------------------------------------------------------
    // others:

    void reset();

    //---------------------------------------------------------------------------------------------
    // IR3109 specific:

    /** The RES pot / slider position, 0..1 (the resonance knob with Open303's skew undone; both are linear). */
    double getResPot() const { return resPot; }

    /** Current-mirror mismatch, 0..1: each stage OTA's output mirror is off by up to BITE_MIRROR (alternating
        signs), so its operating point leaves the tanh's centre (even harmonics). The stage gain is renormalised, so
        the cutoff holds. 0 = matched, as the circuit. */
    void setBite(double amount) { bite_ = std::clamp(amount, 0.0, 1.0); updateCharacter(); }
    double getBite() const { return bite_; }

    /** Limiter threshold, 0..1 (0 = early, 0.5 = the circuit, 1 = late): scales the BA662's input dividers by
        4^(1 - 2 amount) and its Iabc inversely, so the small-signal resonance holds and the oscillation amplitude
        moves. */
    void setClipThreshold(double amount) { clipThreshold_ = std::clamp(amount, 0.0, 1.0); updateCharacter(); }
    double getClipThreshold() const { return clipThreshold_; }

    /** OTA sharpness, 0..1: the stage OTAs' thermal voltage is scaled by 2^(1 - 2 amount) (the cutoff holds).
        0.5 = the circuit. */
    void setSharpness(double amount) { sharpness_ = std::clamp(amount, 0.0, 1.0); vtj = VT * std::pow(2.0, 1.0 - 2.0 * sharpness_); updateCharacter(); }
    double getSharpness() const { return sharpness_; }

    /** Warmth, 0..1: the VCA's input is driven VCA_DRIVE_MAX^amount harder. 0 = the circuit. */
    void setWarmth(double amount) { warmth_ = std::clamp(amount, 0.0, 1.0); updateCharacter(); }
    double getWarmth() const { return warmth_; }

    /** Saturation bias, 0..1: stage 1's OTA (the one the input drives) gets an input offset of 2 VT b with its resting
        output current trimmed out, i = -Iabc (tanh(u + b) - tanh b) / (1 - tanh^2 b): the same slope and operating point
        at rest, but unequal current limits, 1 - tanh b and 1 + tanh b (even harmonics once the stage is driven). The
        shape of the diode ladder's Saturation Bias (DiodeLadderFilter::setSaturationBias) on this circuit's input
        pair; with no resting current there is nothing for the stage's DC feedback (x120 at its output) to correct. */
    void setSaturationBias(double b) { satBias_ = std::clamp(b, 0.0, 1.0); updateCharacter(); }
    double getSaturationBias() const { return satBias_; }

    /** Supply rail, 0..1: 1 = +-15 V, lower sags both rails by up to RAIL_SAG of themselves (the stage OTAs' output
        compliance and the followers' ceiling: less headroom). */
    void setRail(double r) { rail_ = std::clamp(r, 0.0, 1.0); updateSupply(); }
    double getRail() const { return rail_; }

    /** Filter FM depth (0..1): Devilfish-style audio-rate cutoff modulation from the AC-coupled input. */
    void setFilterFmDepth(double amount) { filterFmDepth_ = std::clamp(amount, 0.0, 1.0); }
    double getFilterFmDepth() const { return filterFmDepth_; }

    /** Stage outputs (volts re ground), for the fidelity tools. */
    double getStageOutput(int k) const { return y[std::clamp(k, 0, 3)]; }

    //---------------------------------------------------------------------------------------------
    // circuit constants (public for the fidelity tools):

    // Thermal voltage at SPICE's 27 C.
    static constexpr double VT = 0.025865;

    // IR3109 stages: 68k input / local feedback, 560R shunt, 270 pF (the 80017A; Juno-6 / SH-101: 240 pF)
    static constexpr double R_FB = 68e3, R_SH = 560.0, C_ST = 270e-12;
    static constexpr double K_IN = 1.0 / (2.0 + R_FB / R_SH);   // stages 2..4: Vd = (y_prev + y) K_IN
    static constexpr double VSH = 1.6;                          // PMOS follower level shift
    static constexpr double VCLAMP = 0.8;                       // OTA output compliance from either rail
    static constexpr double Y_CEIL_DROP = 0.2, Y_CEIL_S = 0.05; // follower ceiling below the supply (softplus width)
    static constexpr double CLAMP_IS = 1e-14;
    // rails: +-15 V re ground; Saturation Rail sags them by up to RAIL_SAG
    static constexpr double V_POS = 15.0, V_NEG = -15.0, RAIL_SAG = 0.3;

    // Input 4.7k; BA662 resonance VCA inputs 100k / 1.5k (from Out4), 24k / 1.5k (from the input; the Juno-6 has 10k
    // and 47k); its Iabc at full RES for the service notes' 4.8 V p-p oscillation (VR26), and the RES CV's Tr18 offset
    // (of its 0..10 V)
    static constexpr double R_IN = 4.7e3;
    static constexpr double RES_KN = 1.5e3 / (100e3 + 1.5e3), RES_KP = 1.5e3 / (24e3 + 1.5e3);
    static constexpr double RES_IABC_MAX = 0.2326e-3;
    static constexpr double RES_OFFSET = 0.06;

    // VCA input: the BA662's differential input per volt of the LP output (circuit volts): VCF out through C59, VR27
    // (50kB, taken mid) and R108 82k into the 80017A's 4.7k / 560R pad.
    static constexpr double VCA_K = 560.0 / (82e3 + 25e3 + 4.7e3 + 560.0);
    static constexpr double VCA_DRIVE_MAX = 4.0;

    // Character ranges
    static constexpr double BITE_MIRROR = 0.1;

    // Input level: volts at VCF IN (pin 1) for a full-scale (+-1) signal at 0 dB drive: a 0.31 V p-p saw, 4.5 V p-p at
    // Out4, 0.6 dB under the 4.8 V p-p self-oscillation. HS-60 / Juno-106S service notes (Jul. 1985): the MC5534 makes
    // its saw and pulse at ~12 V p-p (p. 11, Sawtooth) and mixes them inside to WAVE Xout (pin 14), which drives VCF IN
    // through C56 with the sub (SUB LEVEL 0..+10 V through R102 33k / Tr19 / R101 27k / D6) and the noise bus (p. 15,
    // Module board); VR26 sets 4.8 V p-p of oscillation at TP19 (C4, RESO 10), VR27 then 6 V p-p at the VCA output
    // TP8, VR32 4 V p-p of noise there (FREQ 10, RES 0: 3.2 V p-p at VCF out) (p. 22, Adjustment 5, 6, 9). The notes
    // give no level at VCF IN (the MC5534's mix resistors are internal), so the saw / oscillation ratio is the Juno-6's,
    // whose mix is discrete (service notes May 1982, p. 9: 12 V p-p saw through 15k + 68k into 10k || 10k (its IR3109
    // input) || 47k || 33k, Out4 3.7 V p-p against its 4 V p-p oscillation, p. 16). It sits between the 106's noise
    // (3.2 V p-p at VCF out) and the sub's ceiling (0.58 V p-p into VCF IN's ~4k, 8.4 V p-p at Out4).
    static constexpr double INPUT_VOLTS = 0.155;
    // Drive make-up exponent: the output is divided by driveFactor^exp.
    static constexpr double MAKEUP_EXP = 0.95;
    // Newton: iterations from the last sample, then the fallback's (from it again, steps up to FALLBACK_STEP volts, the
    // capacitor nodes at most FALLBACK_OVER past the compliance clamps' knees)
    static constexpr int NEWTON_IT = 24, FALLBACK_IT = 40;
    static constexpr double FALLBACK_STEP = 2.0, FALLBACK_OVER = 0.75;
    // BP / HP output trims (dB).
    static constexpr double BP_TRIM_DB = 0.1, HP_TRIM_DB = 5.7;

    //=============================================================================================

  protected:

    void updatePot();
    void updateGains();
    void updateCharacter();
    void updateSupply();

    INLINE double getSampleCircuit(double vin, double fc);

    /** One IR3109 stage's capacitor-node current (OTA + compliance clamps) minus the capacitor's, for the OTA input
        voltage vi; dO = d i_ota / d vi, dCl = d (clamps - capacitor) / d vc. */
    INLINE double stageResidual(int k, double vi, double iabc, double twoVt, const double* J, double& dO, double& dCl) const;

    /** Iabc for a stage pole at fc (prewarped: the trapezoidal integrators then put it exactly at fc). */
    INLINE double stageIabc(double fc) const { return 2.0 * vtj * C_ST * (2.0 / T * std::tan(PI * fc * T)) / K_IN; }

    /** The VCA input's BA662 tanh on a normalised output. */
    INLINE double vca(double out);

    /** Mode output from the LP output, the stage-1 equivalent input u0 and the stage taps, through the VCA input. */
    INLINE double modeMix(double lp, double u0);

    /** Follower: y = vc + VSH below the ceiling (softplus); dy / dvc. */
    INLINE double follower(double vc, double& dy) const;

    // parameters
    double cutoff = 1000.0;
    double resonance = 0.0;
    double resPot = 0.0;
    double drive = 0.0;
    double driveFactor = 1.0;
    double driveMakeup = 1.0;
    double mode = 0.0;
    double sampleRate = 44100.0;
    double T = 1.0 / 44100.0;
    double filterFmDepth_ = 0.0;

    // derived
    double vtj = VT;
    double resIabc = 0.0;                 // the BA662's Iabc (with the Clip scaling)
    double resKn = RES_KN, resKp = RES_KP;
    double bite_ = 0.0, clipThreshold_ = 0.5, sharpness_ = 0.5, warmth_ = 0.0;
    double mirror[4] = {}, gmNorm[4] = { 1.0, 1.0, 1.0, 1.0 };   // per-stage mirror mismatch, gain renormalisation
    double satBias_ = 0.0, rail_ = 1.0;
    double yCeil = V_POS - Y_CEIL_DROP;   // the followers' ceiling
    double vHi = V_POS - VCLAMP, vLo = V_NEG + VCLAMP;   // the stage OTAs' output compliance
    double inOff[4] = {}, inOffT[4] = {};  // per-stage input offset (in 2 VT units) and its tanh, trimmed out at rest
    double vcaK = VCA_K;
    double gC = 0.0;                      // 2 C / T
    double gainLp = 1.0, gainBp = 1.0, gainHp = 1.0;

    // state: capacitor node voltages (re ground) and currents, stage outputs
    double vc[4] = { -VSH, -VSH, -VSH, -VSH };
    double ic[4] = {};
    double y[4] = {};
    bool converged = true;                // the last sample's Newton converged
    double vcaDc = 0.0, vcaDcCoef = 0.0;  // the VCA input's coupling capacitor (C59): DC tracker
    double preVca = 0.0;                  // last output before the coupling capacitor (normalised)

    // Filter FM (Devilfish) state
    double acCouplingState_ = 0.0;
    static constexpr double acCouplingFreq_ = 20.0;
    static constexpr double filterFmScale_ = 0.4;

    // Open303's resonance skew normaliser, 1 - exp(-3)
    static constexpr double RES_SKEW_NORM = 0.950213;
  };

  //-----------------------------------------------------------------------------------------------
  // inlined functions:

  INLINE void RolandFilter::setCutoff(double newCutoff, bool /*updateCoefficients*/)
  {
    cutoff = std::clamp(newCutoff, 10.0, 20000.0);
  }

  INLINE void RolandFilter::setResonance(double newResonance, bool /*updateCoefficients*/)
  {
    resonance = newResonance;
    updatePot();
  }

  INLINE double RolandFilter::follower(double v, double& dy) const
  {
    const double x = v + VSH;
    const double u = (x - yCeil) / Y_CEIL_S;
    if (u < -30.0) { dy = 1.0; return x; }
    if (u > 30.0) { dy = 0.0; return yCeil; }
    const double e = std::exp(u);
    dy = 1.0 / (1.0 + e);
    return x - Y_CEIL_S * std::log1p(e);
  }

  INLINE double RolandFilter::getSample(double in)
  {
    double fc = cutoff;
    if (filterFmDepth_ > 0.0)
    {
      double acAlpha = 2.0 * PI * acCouplingFreq_ / sampleRate;
      acCouplingState_ += acAlpha * (in - acCouplingState_);
      fc *= std::clamp(1.0 + filterFmDepth_ * filterFmScale_ * (in - acCouplingState_), 0.5, 1.5);
    }
    fc = std::min(fc, 0.45 * sampleRate);

    return getSampleCircuit(in * driveFactor, fc) / driveMakeup;
  }

  //-----------------------------------------------------------------------------------------------
  // circuit

  INLINE double RolandFilter::stageResidual(int k, double vi, double iabc, double twoVt, const double* J,
                                            double& dO, double& dCl) const
  {
    // mirror mismatch m: i = -Iabc (t + m (1 + t) / 2), renormalised to unit slope at its operating point; the
    // saturation bias offsets the pair (t at vi / 2VT + inOff) with its resting current (inOffT) trimmed out
    const double t = std::tanh(vi / twoVt + inOff[k]);
    const double h = 0.5 * mirror[k], gi = iabc * gmNorm[k];
    dO = -gi * (1.0 - t * t) * (1.0 + h) / twoVt;
    double iCl = 0.0;
    dCl = -gC;
    if (vc[k] > vHi - 0.3)
    {
      const double e = std::exp(std::min((vc[k] - vHi) / VT, 80.0));
      iCl -= CLAMP_IS * (e - 1.0); dCl -= CLAMP_IS * e / VT;
    }
    if (vc[k] < vLo + 0.3)
    {
      const double e = std::exp(std::min((vLo - vc[k]) / VT, 80.0));
      iCl += CLAMP_IS * (e - 1.0); dCl -= CLAMP_IS * e / VT;
    }
    return -gi * ((t - inOffT[k]) * (1.0 + h) + h) + iCl - (gC * vc[k] - J[k]);
  }

  INLINE double RolandFilter::vca(double out)
  {
    // the coupling capacitor (C59), then the BA662's tanh (normalised units -> circuit volts at the LP scale),
    // unit small-signal gain
    preVca = out;
    out -= vcaDc;
    vcaDc += vcaDcCoef * out;
    const double x = vcaK * out * gainLp / (2.0 * VT);
    if (std::fabs(x) < 1e-6) return out;
    return out * std::tanh(x) / x;
  }

  INLINE double RolandFilter::modeMix(double lp, double u0)
  {
    if (mode <= 0.0)
      return vca(lp / gainLp);
    // stage taps z_k = (-1)^k y_k: BP = -(z2 - 2 z3 + z4), HP = z0 - 4 z1 + 6 z2 - 4 z3 + z4
    const double bp = -(y[1] + 2.0 * y[2] + y[3]);
    const double hp = u0 + 4.0 * y[0] + 6.0 * y[1] + 4.0 * y[2] + y[3];
    const double wl = std::max(0.0, 1.0 - 2.0 * mode);
    const double wh = std::max(0.0, 2.0 * mode - 1.0);
    const double wb = 1.0 - wl - wh;
    return vca(wl * lp / gainLp + wb * bp / gainBp + wh * hp / gainHp);
  }

  INLINE double RolandFilter::getSampleCircuit(double x, double fc)
  {
    const double vin = x * INPUT_VOLTS;
    const double twoVt = 2.0 * vtj;
    const double iabc = stageIabc(fc);

    // stage 1's input node: y1 through 68k, the input through 4.7k, 560R to ground, the BA662's output current
    constexpr double G68 = 1.0 / R_FB, GSH = 1.0 / R_SH, G14 = 1.0 / R_IN;
    constexpr double S1 = G68 + G14 + GSH;
    const double a1y = G68 / S1;
    const double ain = vin * G14 / S1;
    const double twoVtR = 2.0 * VT;   // the BA662 is not scaled by sharpness
    const double vp = resKp * vin;

    double J[4];
    for (int k = 0; k < 4; ++k) J[k] = gC * vc[k] + ic[k];

    // Newton from the last sample; if it has not converged in NEWTON_IT iterations (a jump of more than their 0.5 V
    // steps can reach, or a node deep in a clamp junction's exponential), again from the last sample with longer steps
    // kept out of the junctions
    double vcLast[4];
    for (int k = 0; k < 4; ++k) vcLast[k] = vc[k];
    converged = false;
    double ires = 0.0;
    for (int it = 0; it < NEWTON_IT + FALLBACK_IT; ++it)
    {
      if (it == NEWTON_IT) for (int k = 0; k < 4; ++k) vc[k] = vcLast[k];   // the fallback start
      const double lim = it < NEWTON_IT ? 0.5 : FALLBACK_STEP;
      double dy[4];
      for (int k = 0; k < 4; ++k) y[k] = follower(vc[k], dy[k]);

      const double tr = std::tanh((vp - resKn * y[3]) / twoVtR);
      ires = resIabc * tr;
      const double dires4 = -resIabc * (1.0 - tr * tr) / twoVtR * resKn * dy[3];

      const double vi1 = a1y * y[0] + ain + ires / S1;
      const double vi[4] = { vi1, K_IN * (y[0] + y[1]), K_IN * (y[1] + y[2]), K_IN * (y[2] + y[3]) };

      double F[4], Jm[4][4] = {};
      for (int k = 0; k < 4; ++k)
      {
        double dO, dCl;
        F[k] = stageResidual(k, vi[k], iabc, twoVt, J, dO, dCl);
        if (k == 0)
        {
          Jm[0][0] = dO * a1y * dy[0] + dCl;
          Jm[0][3] = dO * dires4 / S1;
        }
        else
        {
          Jm[k][k - 1] = dO * K_IN * dy[k - 1];
          Jm[k][k] = dO * K_IN * dy[k] + dCl;
        }
      }

      double dx[4];
      solveLinear<4>(Jm, F, dx);
      double mx = 0.0;
      for (int k = 0; k < 4; ++k)
      {
        const double d = std::clamp(dx[k], -lim, lim);
        vc[k] += d; mx = std::max(mx, std::fabs(d));
        // the fallback keeps the capacitor nodes out of the clamp junctions' far exponential (where Newton crawls back
        // a VT per step)
        if (it >= NEWTON_IT) vc[k] = std::clamp(vc[k], vLo - FALLBACK_OVER, vHi + FALLBACK_OVER);
      }
      if (mx < 1e-9) { converged = true; break; }
    }

    double dyk;
    for (int k = 0; k < 4; ++k)
    {
      y[k] = follower(vc[k], dyk);
      ic[k] = gC * vc[k] - J[k];
    }
    ires = resIabc * std::tanh((vp - resKn * y[3]) / twoVtR);

    return modeMix(y[3], R_FB * (vin * G14 + ires));
  }

} // end namespace dfl

#endif // dfl_RolandFilter_h
