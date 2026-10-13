// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Lowenfels

#ifndef dfl_ARP4075Filter_h
#define dfl_ARP4075Filter_h

// ARP 4075 four-pole low-pass filter (Odyssey Mk III, Axxe, Omni, Quadra), circuit-native, in the Odyssey's wiring.
//
//  * Input: the "signal limiter" Q9 / Q10, a 2N5087 pair on R24 56k from +15 V, its bases on R35 / R36 3k3. The
//    Odyssey's mixer drives pin 1 (a VCO through R123 1M and C24 into R35); the resonance drives pin 2 (the output,
//    after C6, through the RES pot R144 100k lin, R142 22k || C37 220p into R36). Its collector currents go into stage
//    1's base resistors, so everything that enters the filter passes one tanh with a ~0.26 mA limit.
//  * Four stages: a 2N5087 pair whose base nodes (R3 / R4 220R to the internal reference) sum the previous output and
//    its own through 12k1 (a one-pole low-pass with unity DC gain), the pair's collectors into an LM3900 Norton
//    amplifier with 4700 pF across it (the integrator), on 0 / -14 V about a -7 V reference.
//  * Tail currents: the exponential converter Q13 / Q14 through R2 / R7 / R12 / R17 4k7 per stage; Q14's collector
//    saturates at about 1.4 mA a stage, ARP's "design miscalculation" that limits the cutoff to ~12 kHz; the 2k2 mod
//    (the schematic's note 7, setTailMod) raises that to ~3 mA. The 4k7s share Q14's current by the stages' base voltages: stage 1's sit ~28 mV higher (the
//    limiter's current in its 220R), so it gets less, a lot less at low cutoffs (half the others' at 30 Hz), and the
//    signal's common mode on each pair modulates its tail.
//
// Each sample solves the four integrator nodes (trapezoidal capacitors, Newton); the base nodes and the input pair
// are memoryless. Component values from ARP's 4075 schematic (rev. E, ARPtech) and the Odyssey Mk III VCF page;
// checked against an ngspice model of the circuit with real 2N5087s (tools/arp-fidelity).
//
// The LP / BP / HP morph mixes the stage outputs (binomial taps of the cascade, signs chosen so the crossfades have
// no notches): the circuit runs unchanged, with its resonance and limiter, in every mode. Saturation Bias offsets the
// limiter pair (its resting current trimmed out), Saturation Rail sags the supplies (setRail).

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

  class ARP4075Filter
  {

  public:

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    ARP4075Filter();
    ~ARP4075Filter();

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
    // 4075 specific:

    /** The RES pot position, 0..1 (the resonance knob with Open303's skew undone; R144 is linear). */
    double getResPot() const { return resPot; }

    /** Saturation bias, 0..1: the limiter pair gets an input offset of 2 VT b with its resting output current trimmed
        out, i = I (tanh(u + b) - tanh b) / (1 - tanh^2 b): the same slope at rest, unequal current limits (even
        harmonics once it is driven). 0 = the circuit. Driven, it rectifies: its mean current follows the level (an
        output DC that moves with each note's envelope, and, through C6, a limiter offset that settles over ~1 s).
        Intended: Open303's 24 Hz output highpass leaves it as an even-order sub-40 Hz part 29..36 dB under the
        signal on a gated sequence (a 2..3 % pulse over a note's first ~20 ms), as the Wasp's and MS-20's bias. */
    void setSaturationBias(double b) { satBias_ = std::clamp(b, 0.0, 1.0); satBiasT = std::tanh(satBias_); }
    double getSaturationBias() const { return satBias_; }

    /** Supply rail, 0..1: 1 = the circuit; lower sags the +15 V and -14 V supplies together by up to RAIL_SAG of
        themselves (the -7 V reference, half the -14 V, follows). The +15 V sets the limiter's tail through R24: its
        current ceiling and its gain fall together (the pair's threshold, 2 VT, does not move: no earlier limiting),
        so the filter gets quieter (-8.2 dB at 0) and the resonance loop's gain falls with it: less peak and bass
        loss (more of the input reaches the limiter: more passband distortion at moderate RES), the oscillation
        onset from pot 0.84 to 0.99 at rail 0.3, none below. The LM3900s' swing about the reference shrinks with
        the stage signals, and Q14's headroom above the stage emitters with the reference (the cutoff ceiling,
        ~12 kHz, falls to ~5 kHz at 0). */
    void setRail(double r) { rail_ = std::clamp(r, 0.0, 1.0); updateRails(); }
    double getRail() const { return rail_; }

    /** The 2k2 mod (the schematic's note 7): R2 / R7 / R12 / R17 4k7 -> 2k2. Q14 then saturates at ~3 mA a stage
        instead of ~1.4 mA (the cutoff reaches past 20 kHz instead of ~12 kHz); the tail split, through less
        resistance, follows the stages' base voltages more (stage 1 starves more at low cutoffs). Off = stock. */
    void setTailMod(bool on) { tailMod_ = on; rTail = on ? R_TAIL_MOD : R_TAIL; }
    bool getTailMod() const { return tailMod_; }

    /** Filter FM depth (0..1): Devilfish-style audio-rate cutoff modulation from the AC-coupled input. */
    void setFilterFmDepth(double amount) { filterFmDepth_ = std::clamp(amount, 0.0, 1.0); }
    double getFilterFmDepth() const { return filterFmDepth_; }

    /** Stage outputs relative to the reference (volts), for the fidelity tools. */
    double getStageOutput(int k) const { return y[std::clamp(k, 0, 3)]; }
    /** Per-stage tail current for a cutoff (amps), after Q14's saturation. */
    double getTail(double fc) const { return stageTail(fc); }

    //---------------------------------------------------------------------------------------------
    // circuit constants (public for the fidelity tools):

    static constexpr double VT = 0.025865;   // thermal voltage at SPICE's 27 C

    // Stages: R3 / R4 220R base resistors, R1 / R21 12k1 (1 %) summing, C1..C4 4700 pF
    static constexpr double R_B = 220.0, R_SUM = 12.1e3, C_ST = 4700e-12;
    static constexpr double R_B_FB = R_B * R_SUM / (R_B + R_SUM);   // the base node with the summing resistor
    static constexpr double K_SUM = R_B_FB / R_SUM;                   // stage input: vd = K_SUM (y_prev - y)
    // Tail: R2 4k7 per stage from Q14's collector, which saturates VQ14_HEAD above the stage emitters
    // (VQ14_HEAD fitted to the SPICE model: Q14 saturates sharply at 1.50 mA a stage through 4k7, 3.2 mA through 2k2)
    static constexpr double R_TAIL = 4.7e3, R_TAIL_MOD = 2.2e3, VQ14_HEAD = 7.05, TAIL_KNEE = 0.02;
    // Limiter: R24 56k from +15 V, VBE ~0.6 V; 2N5087 beta (its bases load R35 / R36)
    static constexpr double R24 = 56e3, V_POS = 15.0, VBE = 0.6, BETA = 254.0;
    // collector / emitter current: the 2N5087 pairs (beta 254) and Q14 (2N3906, beta 181)
    static constexpr double ALPHA = BETA / (BETA + 1.0), ALPHA_Q14 = 181.0 / 182.0;
    // Odyssey input: a VCO through R123 1M and C24 0.05u into R35 3k3
    static constexpr double R_MIX = 1e6, R35 = 3.3e3, C24 = 0.05e-6;
    // Resonance: R144 100k (wiper from the ground end), R142 22k || C37 220p, into R36 3k3; C6 10u output coupling
    static constexpr double R_RES = 100e3, R142 = 22e3, C37 = 220e-12, R36 = 3.3e3, C6 = 10e-6;
    // LM3900s on 0 / -14 V about the -7 V reference (Q16): output swing re the reference
    static constexpr double V_NEG = 14.0, SWING_HI_DROP = 1.0, SWING_LO_DROP = 0.1, CLAMP_IS = 1e-14;

    // Character ranges
    static constexpr double RAIL_SAG = 0.6;

    // Input level: volts at the mixer pot for a full-scale (+-1) signal at 0 dB drive (the Odyssey's VCOs, 10 V p-p:
    // ~16 mV at the limiter, 0.6 % THD in the passband). 0 dB is one source at full slider; the mixer's sources sum into
    // the same pin, so both VCOs full reach +6 dB. +16 dB puts ~104 mV (4 VT) on the limiter: near its hard limit (a
    // 200 Hz sine at RES 0: 4.5 dB of compression, 14 % THD), the knee's far side. The output is divided by
    // driveFactor^MAKEUP_EXP: LP within +0.6 / -1.9 dB from 0 to +16 dB at RES 0 (tools/arp-fidelity/levels).
    static constexpr double INPUT_VOLTS = 5.0;
    static constexpr double MAKEUP_EXP = 0.9;
    // BP / HP output trims (dB): the pure modes sit near LP loudness on a saw at a typical resonant setting (pot
    // 0.5..0.8, cutoff 1 kHz, tools/arp-fidelity/levels), as the other circuit models'. BP is cut: the cascade's
    // 4-pole tap peaks hard near the resonance.
    static constexpr double BP_TRIM_DB = -4.7, HP_TRIM_DB = 4.5;

    //=============================================================================================

  protected:

    void updatePot();
    void updateRails();

    /** Per-stage tail current for a stage pole at fc (prewarped), with Q14's saturation (a soft minimum). */
    INLINE double stageTail(double fc) const
    {
      const double w = 2.0 / T * std::tan(PI * std::min(fc, 0.45 * sampleRate) * T);
      // the Iabc the pole wants, as Q14's emitter current (its collector carries ALPHA_Q14 of it, the pairs' collectors
      // ALPHA of their tails: the cutoff convention is the analog want without them, as SPICE's)
      const double want = ALPHA_Q14 * w * 2.0 * VT * C_ST / K_SUM, imax = q14Head / rTail, e = TAIL_KNEE * imax;
      return 0.5 * (want + imax - std::sqrt((want - imax) * (want - imax) + e * e)) + 0.5 * (std::sqrt((imax) * (imax) + e * e) - imax);
    }

    /** Q14's current (4 it) shared by the four tail resistors (rTail: R_TAIL, or R_TAIL_MOD with the 2k2 mod) from
        their emitters: each stage's emitters sit a Veb above its base common mode cm, Veb = VT ln I + const, so
        I_k = it + (V - cm_k - VT ln(I_k / it)) / rTail with V set by the sum. Linear in cm for fixed per-stage
        resistances r_k = rTail + VT ln(I_k / it) / (I_k - it) (the secants of the pairs' Veb): tailSplit applies them
        (V the r-weighted mean of cm), tailSecants refines them (twice, from VT / it), once per sample. Within ~1 % of
        the exact split down to a starved stage at a quarter of the others'. */
    INLINE void tailSplit(double it, const double (&cm)[4], const double (&r)[4], double (&tail)[4]) const
    {
      const double fl = 0.02 * it;
      double sg = 0.0, sc = 0.0;
      for (int k = 0; k < 4; ++k) { sg += 1.0 / r[k]; sc += cm[k] / r[k]; }
      const double mean = sc / sg;
      for (int k = 0; k < 4; ++k)
      {
        const double x = it + (mean - cm[k]) / r[k];
        tail[k] = 0.5 * (x + std::sqrt(x * x + fl * fl));
      }
    }

    INLINE void tailSecants(double it, const double (&cm)[4], double (&r)[4]) const
    {
      for (int k = 0; k < 4; ++k) r[k] = rTail + VT / it;
      double tail[4];
      for (int pass = 0; pass < 2; ++pass)
      {
        tailSplit(it, cm, r, tail);
        for (int k = 0; k < 4; ++k)
        {
          const double q = tail[k] / it;
          r[k] = rTail + (std::fabs(q - 1.0) > 1e-6 ? VT * std::log(q) / (tail[k] - it) : VT / it);
        }
      }
    }

    /** One integrator's output compliance (the LM3900's swing): current into the node, d / dy. */
    INLINE double swing(double v, double& d) const;

    /** Limiter pair: the differential collector current for a differential input u (volts), with the bias; d / du. */
    INLINE double limiter(double u, double& d) const;

    /** The limiter's differential current at rest (no input, the bias trimmed out: 0). */
    double limiterRest() const { double d; return limiter(0.0, d); }

    /** Mode output from the stage-1 equivalent input u0 and the stage taps. */
    INLINE double modeMix(double u0) const;

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
    double satBias_ = 0.0, satBiasT = 0.0, rail_ = 1.0;
    bool tailMod_ = false;
    double rTail = R_TAIL;                // R2 / R7 / R12 / R17 (the 2k2 mod)

    // derived
    double iIn = (V_POS - VBE) / R24;     // limiter tail
    double q14Head = VQ14_HEAD;           // Q14's headroom above the stage emitters (the reference follows the rail)
    double rD = 1e5;                      // the limiter's differential input resistance, 2 (beta + 1) re
    double kLoad = 1.0;                   // base loading: rD / (rD + the pins' source resistances)
    double vHi = 0.5 * V_NEG - SWING_HI_DROP, vLo = -(0.5 * V_NEG - SWING_LO_DROP);   // stage swing re the reference
    double wiper = 0.0, rTh = 0.0;        // RES pot: wiper / output, Thevenin resistance
    double gC = 0.0;                      // 2 C / T (stages)
    double g37 = 0.0, h24 = 0.0;          // C37 conductance; C24 half step
    double c6Coef = 0.0;                  // C6: DC tracker
    double gainLp = 1.0, gainBp = 1.0, gainHp = 1.0;

    // state: stage outputs (re the reference, = their capacitor voltages) and capacitor currents; C37 / C24 / C6
    double y[4] = {};
    double yPrev[4] = {};                 // one sample back (the predictor)
    double ic[4] = {};
    double v37 = 0.0, i37 = 0.0;          // C37 voltage / current (R142's node)
    double v24 = 0.0, i24 = 0.0;          // C24 (the mixer coupling)
    double dc6 = 0.0;                     // C6's charge (the output's DC)
    double u0 = 0.0;                      // stage-1 equivalent input, last sample
    double rest[5] = {};                  // u0, y1..y4 at the DC operating point
    double outDc = 0.0;                   // output coupling: DC tracker
    bool dcSolve = false;

    // Filter FM (Devilfish) state
    double acCouplingState_ = 0.0;
    static constexpr double acCouplingFreq_ = 20.0;
    static constexpr double filterFmScale_ = 0.4;

    // Open303's resonance skew normaliser, 1 - exp(-3)
    static constexpr double RES_SKEW_NORM = 0.950213;
  };

  //-----------------------------------------------------------------------------------------------
  // inlined functions:

  INLINE void ARP4075Filter::setCutoff(double newCutoff, bool /*updateCoefficients*/)
  {
    cutoff = std::clamp(newCutoff, 10.0, 20000.0);
  }

  INLINE void ARP4075Filter::setResonance(double newResonance, bool /*updateCoefficients*/)
  {
    resonance = newResonance;
    updatePot();
  }

  INLINE double ARP4075Filter::swing(double v, double& d) const
  {
    double i = 0.0; d = 0.0;
    if (v > vHi - 0.3) { const double e = std::exp(std::min((v - vHi) / VT, 80.0)); i -= CLAMP_IS * (e - 1.0); d -= CLAMP_IS * e / VT; }
    if (v < vLo + 0.3) { const double e = std::exp(std::min((vLo - v) / VT, 80.0)); i += CLAMP_IS * (e - 1.0); d -= CLAMP_IS * e / VT; }
    return i;
  }

  INLINE double ARP4075Filter::limiter(double u, double& d) const
  {
    const double t = std::tanh(u / (2.0 * VT) + satBias_), g = ALPHA * iIn / (1.0 - satBiasT * satBiasT);
    d = g * (1.0 - t * t) / (2.0 * VT);
    return g * (t - satBiasT);
  }

  INLINE double ARP4075Filter::modeMix(double u) const
  {
    // taps re the DC point (the limiter's and stages' small offsets)
    const double z[4] = { y[0] - rest[1], y[1] - rest[2], y[2] - rest[3], y[3] - rest[4] };
    const double lp = z[3] / gainLp;
    if (mode <= 0.0) return lp;
    // non-inverting stages: BP = -(z2 - 2 z3 + z4), HP = u - 4 z1 + 6 z2 - 4 z3 + z4
    const double bp = -(z[1] - 2.0 * z[2] + z[3]) / gainBp;
    const double hp = (u - rest[0] - 4.0 * z[0] + 6.0 * z[1] - 4.0 * z[2] + z[3]) / gainHp;
    const double wl = std::max(0.0, 1.0 - 2.0 * mode), wh = std::max(0.0, 2.0 * mode - 1.0), wb = 1.0 - wl - wh;
    return wl * lp + wb * bp + wh * hp;
  }

  INLINE double ARP4075Filter::getSample(double in)
  {
    double fc = cutoff;
    if (filterFmDepth_ > 0.0)
    {
      double acAlpha = 2.0 * PI * acCouplingFreq_ / sampleRate;
      acCouplingState_ += acAlpha * (in - acCouplingState_);
      fc *= std::clamp(1.0 + filterFmDepth_ * filterFmScale_ * (in - acCouplingState_), 0.5, 1.5);
    }
    const double it = stageTail(fc), twoVt = 2.0 * VT;

    // pin 1: the mixer resistor and C24 into R35 (trapezoidal; open at DC)
    const double vmix = in * driveFactor * INPUT_VOLTS;
    const double g24 = dcSolve ? 0.0 : 1.0 / (R_MIX + R35 + h24);
    const double i24n = (vmix - v24 - h24 * i24) * g24;
    const double v1 = R35 * i24n;

    // pin 2: the output (after C6) through the RES wiper (Thevenin rTh), R142 || C37, into R36: linear in y4,
    // i = (wiper vout (G142 + g37) - J37) / (1 + (rTh + R36) (G142 + g37))
    constexpr double G142 = 1.0 / R142;
    const double gz = G142 + (dcSolve ? 0.0 : g37), J37 = dcSolve ? 0.0 : g37 * v37 + i37;
    const double den = 1.0 + (rTh + R36) * gz;
    // C6 is open at DC (no feedback of the DC point)
    const double a2 = dcSolve ? 0.0 : R36 * wiper * gz / den, b2 = -R36 * J37 / den;   // v2 = a2 (y4 - dc6) + b2

    double J[4];
    for (int k = 0; k < 4; ++k) J[k] = dcSolve ? 0.0 : gC * y[k] + ic[k];
    const double gc = dcSolve ? 0.0 : gC;

    // predictor: the stages' linear extrapolation from the last two samples, kept inside the swing (past it the clamps'
    // exponentials throw Newton off); if Newton does not converge from it, again from the last sample
    double yLast[4];
    for (int k = 0; k < 4; ++k) yLast[k] = y[k];
    if (!dcSolve)
      for (int k = 0; k < 4; ++k) y[k] = std::clamp(2.0 * y[k] - yPrev[k], vLo, vHi);
    for (int k = 0; k < 4; ++k) yPrev[k] = yLast[k];

    double din = 0.0, dIn = 0.0;
    double tail[4];
    for (int iter = 0; iter < 60; ++iter)
    {
      if (iter == 30 && !dcSolve) for (int k = 0; k < 4; ++k) y[k] = yLast[k];   // the fallback start
      // limiter (PNP: a higher base takes less current): Q9's collector, I/2 - di/2, into stage 1's feedback base node
      // (R_B || R_SUM, with y1), Q10's, I/2 + di/2, into the other (R_B); vd1 = v(Q1 base) - v(Q2 base)
      const double v2 = a2 * (y[3] - dc6) + b2;
      din = limiter(kLoad * (v1 - v2), dIn);
      dIn *= kLoad;
      const double i9 = 0.5 * (iIn - din), i10 = 0.5 * (iIn + din);
      const double vd1 = R_B * i10 - R_B_FB * i9 - K_SUM * y[0];
      const double dvd1_4 = -0.5 * (R_B + R_B_FB) * dIn * a2;   // d vd1 / d y4 (through the limiter)
      const double vd[4] = { vd1, K_SUM * (y[0] - y[1]), K_SUM * (y[1] - y[2]), K_SUM * (y[2] - y[3]) };
      // the stages' base common mode shares Q14's current through the tail resistors: the split at the predicted point
      // (and again at the fallback start), held through the solve so the Jacobian stays exact
      if (iter == 0 || (iter == 30 && !dcSolve))
      {
        const double cm[4] = { 0.5 * (R_B * i10 + R_B_FB * i9 + K_SUM * y[0]), 0.5 * K_SUM * (y[0] + y[1]),
                               0.5 * K_SUM * (y[1] + y[2]), 0.5 * K_SUM * (y[2] + y[3]) };
        double rSec[4];
        tailSecants(it, cm, rSec);
        tailSplit(it, cm, rSec, tail);
      }

      double F[4], Jm[4][4] = {};
      for (int k = 0; k < 4; ++k)
      {
        const double t = std::tanh(vd[k] / twoVt), g = ALPHA * tail[k] * (1.0 - t * t) / twoVt;
        double dCl;
        const double iCl = swing(y[k], dCl);
        F[k] = ALPHA * tail[k] * t + iCl - (gc * y[k] - J[k]);
        if (k == 0) { Jm[0][0] = -g * K_SUM + dCl - gc; Jm[0][3] = g * dvd1_4; }
        else { Jm[k][k - 1] = g * K_SUM; Jm[k][k] = -g * K_SUM + dCl - gc; }
      }
      double dx[4];
      solveLinear<4>(Jm, F, dx);
      double mx = 0.0;
      for (int k = 0; k < 4; ++k) { const double d = std::clamp(dx[k], -1.0, 1.0); y[k] += d; mx = std::max(mx, std::fabs(d)); }
      if (mx < (dcSolve ? 1e-11 : 1e-9)) break;
    }
    if (dcSolve) return 0.0;

    for (int k = 0; k < 4; ++k) ic[k] = gc * y[k] - J[k];
    // C24, C37, C6
    v24 += h24 * (i24n + i24); i24 = i24n;
    const double vout = y[3] - dc6;
    const double iz = (wiper * vout * gz - J37) / den;               // the R142 || C37 branch current
    const double vz = wiper * vout - iz * (rTh + R36);               // its voltage
    i37 = g37 * vz - J37; v37 = vz;
    dc6 += c6Coef * (y[3] - dc6);

    // the stage-1 equivalent input: the y1 its pair drives toward
    const double i9 = 0.5 * (iIn - din), i10 = 0.5 * (iIn + din);
    u0 = (R_B * i10 - R_B_FB * i9) / K_SUM;
    double out = modeMix(u0);
    out -= outDc;   // the output's coupling (C6 into the next stage's input)
    outDc += c6Coef * out;
    return out / driveMakeup;
  }

} // end namespace dfl

#endif // dfl_ARP4075Filter_h
