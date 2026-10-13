// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Lowenfels

#ifndef dfl_MS20Filter_h
#define dfl_MS20Filter_h

// Korg MS-20 low-pass filter, circuit-native: the Korg35 revision (MS-10 and early MS-20, KLM-127 old production)
// solved from its schematic. A Sallen-Key low-pass whose two resistors are the Korg35 chip's transistors Q2 / Q3 run
// in reverse saturation (current-controlled resistors), fed from a shared base node that Q1's base-collector junction
// robs for exponential cutoff control; a JFET + PNP buffer, a x58 gain stage limited by back-to-back diodes in the
// forward path, and the PEAK pot feeding back to the bottom of C29.
//
// Each sample solves the circuit's node equations (Ebers-Moll junctions, trapezoidal capacitors) with Newton's method. Component values from the MS-20 service manual and Stinchcombe, "A Study of the Korg MS10 & MS20
// Filters" (2006); checked against the ngspice models in tools/ms20-fidelity/spice (see tools/ms20-fidelity/README.md).
//
// The LP / BP / HP morph moves the input between the circuit's injection points: LP into the input pad, BP into the
// bottom of the resonance feedback, HP into the lifted bottom of the second capacitor (the MS-20's own HPF wiring).

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

  class MS20Filter
  {

  public:

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. */
    MS20Filter();

    /** Destructor. */
    ~MS20Filter();

    //---------------------------------------------------------------------------------------------
    // parameter settings:

    void setSampleRate(double newSampleRate);
    INLINE void setCutoff(double newCutoff, bool updateCoefficients = true);
    INLINE void setResonance(double newResonance, bool updateCoefficients = true);
    void setInputDrive(double newDrive);
    /** Response mode, 0 = LP, 0.5 = BP, 1 = HP (moves the input between the circuit's injection points). Cheap, so
        it may change per sample. */
    void setMode(double newMode) { mode = std::clamp(newMode, 0.0, 1.0); }

    //---------------------------------------------------------------------------------------------
    // inquiry:

    double getCutoff() const { return cutoff; }
    double getResonance() const { return resonance; }
    double getDrive() const { return drive; }
    double getMode() const { return mode; }
    /** The last sample's Newton solve converged. */
    bool isConverged() const { return converged; }

    //---------------------------------------------------------------------------------------------
    // audio processing:

    INLINE double getSample(double in);

    //---------------------------------------------------------------------------------------------
    // others:

    void reset();

    //---------------------------------------------------------------------------------------------
    // MS-20 specific:

    /** The PEAK pot's rotation, 0..1 (the resonance knob with Open303's skew undone; the pot is linear). */
    double getPeakPot() const { return peakPot; }

    /** Diode pair mismatch, 0..1: one limiter diode's saturation current is raised by up to BITE_MAX_DECADES, so
        it conducts earlier than its partner (even harmonics). 0 = matched, as the circuit. */
    void setBite(double amount) { bite_ = std::clamp(amount, 0.0, 1.0); updateDiodes(); }
    double getBite() const { return bite_; }

    /** Limiter diode threshold, 0..1 (0 = early, 1 = late). Scales the diodes' saturation current by
        10^(CLIP_DECADES * (0.5 - amount)): each decade moves the knee by N VT ln 10 (~0.1 V per diode).
        0.5 = the circuit's 1S1588. */
    void setClipThreshold(double amount) { clipThreshold_ = std::clamp(amount, 0.0, 1.0); updateDiodes(); }
    double getClipThreshold() const { return clipThreshold_; }

    /** Junction sharpness, 0..1: the thermal voltage of the cutoff elements (the Korg35's Q1-Q3) is scaled by 2^(1 - 2 amount), so they saturate earlier (sharper) or later (softer) relative to the signal.
        The cutoff is re-solved, so only the large-signal behaviour changes. 0.5 = the circuit. */
    void setSharpness(double amount);
    double getSharpness() const { return sharpness_; }

    /** Buffer warmth, 0..1: starves the cutoff stage's output buffer (the JFET follower) of bias current, so its
        level shift follows the signal (even harmonics). 0 = the circuit. */
    void setWarmth(double amount) { warmth_ = std::clamp(amount, 0.0, 1.0); }
    double getWarmth() const { return warmth_; }

    /** Saturation bias, 0..1, on the element the input drives (no input pair; the input drives the forward
        limiter): one limiter diode's saturation current raised by BITE_MAX_DECADES b, on top of Bite. */
    void setSaturationBias(double b) { satBias_ = std::clamp(b, 0.0, 1.0); updateDiodes(); }
    double getSaturationBias() const { return satBias_; }

    /** Filter FM depth (0..1): Devilfish-style audio-rate cutoff modulation from the AC-coupled input. */
    void setFilterFmDepth(double amount) { filterFmDepth_ = std::clamp(amount, 0.0, 1.0); }
    double getFilterFmDepth() const { return filterFmDepth_; }

    //---------------------------------------------------------------------------------------------
    // circuit constants (public for the fidelity tools):

    // Thermal voltage at SPICE's 27 C.
    static constexpr double VT = 0.025865;

    // Korg35 chip transistors (2SC1623; BF / BR from the PSpice model Stinchcombe quotes) and network
    static constexpr double Q_IS = 1.8e-14, Q_BF = 206.7, Q_BR = 4.21;
    static constexpr double VCC = 15.0, R3 = 470e3;
    // Q1's collector source: R1 47k from the control voltage, R2 680 into R58 220 || R55 15k
    static constexpr double RB = 1.0 / (1.0 / 47e3 + 1.0 / (680.0 + 1.0 / (1.0 / 220.0 + 1.0 / 15e3)));
    static constexpr double C29 = 3.3e-9, C28 = 1e-9, C30 = 1e-9, C31 = 10e-6;
    static constexpr double R111 = 220.0, R113 = 5.6e3, R116 = 4.7e3, R117 = 270e3, R127 = 2.2e3, VR22 = 10e3;
    static constexpr double K35_PAD_LP = 220.0 / (15e3 + 220.0);   // R115 / R112
    static constexpr double K35_PAD_HP = 47.0 / (20e3 + 47.0);     // the MS-20 HPF's R100 / R104
    static constexpr double JFET_BUF_GAIN = 1.0797;               // JFET + 2SA812 follower, measured in SPICE
    // IC11 (4558): one-pole op amp, DC gain A0, pole 1 / (2 pi tau) = 30 Hz (3 MHz GBW, as the SPICE model)
    static constexpr double OPAMP_A0 = 1e5, OPAMP_TAU = 5.3e-3;
    static constexpr double OPAMP_SCALE = 1e-3;                   // residual scaling to amperes (1 kohm)
    static constexpr double OPAMP_RAIL = 13.5;                    // output swing on +-15 V: RAIL tanh(vI / RAIL)
    // D20 / D21 junction capacitance (1S1588, 4 pF each) across R117
    static constexpr double D_CJ = 4e-12;

    // Limiter diodes (1S1588 as 1N4148): saturation current and emission coefficient
    static constexpr double D_IS = 2.52e-9, D_N = 1.752;
    static constexpr double CLIP_DECADES = 4.0;
    static constexpr double BITE_MAX_DECADES = 2.0;

    // Input level: volts at the filter input for a full-scale (+-1) signal at 0 dB drive. The Korg35 LPF is fed from
    // the HPF's x58 stage, which its diodes hold to about +-0.7 V; 0.5 V leaves the drive knob's +9 dB to reach it and
    // beyond.
    static constexpr double K35_INPUT_VOLTS = 0.5;

    // Drive make-up: the output is divided by driveFactor^exp, so the drive knob adds grit, not level. The Korg35's
    // forward-path limiter already compresses at 0 dB (its level falls 0.7 dB per dB of drive with full make-up).
    // Fitted on a saw (tools/ms20-fidelity/levels).
    static constexpr double K35_MAKEUP_EXP = 0.3;
    // Korg35 limiter normalisation: at 0 dB the output is renormalised to the diodes' resting conductance (an earlier
    // knee leaks at rest, a small-signal gain loss), driven harder the knee sets the level instead, so the share
    // falls by this much per dB of drive. Keeps Clip / Bite / the saturation bias within ~1.5 dB of the circuit's level
    // from 0 to +16 dB (saw, resonance 0 / 60, C2 / C3).
    static constexpr double K35_LIMITER_NORM_SLOPE = 0.0175;

    // BP / HP output trims (dB): the pure modes sit near LP loudness on a saw at a typical resonant setting (pot
    // 0.5..0.7, cutoff 1 kHz, tools/ms20-fidelity/levels). Their gap at low resonance and high cutoff is the
    // filter's character (a saw's energy is in the low harmonics that BP / HP remove).
    static constexpr double K35_BP_TRIM_DB = 7.7, K35_HP_TRIM_DB = -1.0;

    // Warmth: the buffer input swing that cuts the starved JFET follower off (V); it sees the chip's
    // tens-of-millivolt node.
    static constexpr double K35_BUF_SWING = 0.1;

    //=============================================================================================

  protected:

    void updatePot();
    void updateDiodes();
    void updateCutoffTable();
    void updateSmallSignalGains();

    INLINE double getSampleK35(double vin, double fc);

    /** Korg35 DC operating point (x: vA, vBn, vY, vZ; in: Newton start, out: solution) and the small-signal corner
        (rad/s) for the control voltage vb at Q1's collector source. */
    double k35CornerAt(double vb, double* x);

    /** Korg35 control voltage (Q1's collector source) for a corner of fc Hz, from the table. */
    INLINE double k35ControlFor(double fc) const;

    /** Solves the forward gain-stage limiter: diode voltage vd for the feedback current i, and dvd/di.
        Vd / R + Ia (exp(u) - 1) - Ib (exp(-u) - 1) = i,  u = vd / (n VT). */
    INLINE void solveLimiter(double i, double R, double nVt, double& vd, double& dvdi) const;

    /** SPICE's junction voltage limiting for the limiter diodes: the voltage vNew a Newton step proposes, from vOld, held
        to a logarithmic advance past the critical voltage vCrit (where the diode's current outgrows its linearisation). */
    static INLINE double pnjlim(double vNew, double vOld, double nVt, double vCrit);

    /** Follower with a bias-starved level shift (warmth), normalised to unity small-signal gain. A follower's level
        shift grows as the square root of its current, and a load resistor ties the current to the output: in units of
        the swing xs that cuts the bias off, u = z - lambda (sqrt(1 + u) - 1), solved exactly. Monotonic; the negative
        swing floors at -xs (cut-off) and the positive one is expanded (even harmonics). lambda = 2 warmth. */
    INLINE double buffer(double x, double xs, double& dydx) const;

    // parameters
    double cutoff = 1000.0;
    double resonance = 0.0;
    double peakPot = 0.0;
    double drive = 0.0;
    double driveFactor = 1.0;
    double driveMakeup = 1.0;        // driveFactor^makeup exponent
    double mode = 0.0;
    double sampleRate = 44100.0;
    double T = 1.0 / 44100.0;

    double bite_ = 0.0, clipThreshold_ = 0.5, sharpness_ = 0.5, warmth_ = 0.0, filterFmDepth_ = 0.0;
    double satBias_ = 0.0;

    // derived
    double vtj = VT;                 // junction thermal voltage (sharpness)
    double diodeIa = D_IS, diodeIb = D_IS;
    double gainLp = 1.0, gainBp = 1.0, gainHp = 1.0;   // small-signal output normalisation per injection point

    // Korg35 feedback network (E -> pot -> F -> R113 || C30 -> G -> R111): node conductances
    double fbA = 0.0, fbB = 0.0, fbC = 0.0, fbD = 1.0 / R111, fbS = 1.0, fbRth = 0.0;
    double g29 = 0.0, g28 = 0.0, g30 = 0.0, h31 = 0.0;

    // Korg35 state: node voltages (Newton warm start) and capacitor histories
    double vA = 0.52, vBn = 0.0, vY = 0.0, vZ = 0.0, vN = 0.0, vI = 0.0, vO = 0.0;   // vI: op amp before its rails
    double v29 = 0.0, i29 = 0.0, v28 = 0.0, i28 = 0.0, v30 = 0.0, i30 = 0.0;
    double vC31 = 0.0, iR116 = 0.0;
    double gCd = 0.0, vCd = 0.0, iCd = 0.0;   // diode capacitance across R117
    double vIp = 0.0, opPrev = 0.0;           // op amp internal node and A0 (v+ - v-) - vI, previous sample
    double vZ0 = 0.0;                // Z at the operating point (the buffer's rest point)
    bool converged = true;

    // Korg35 cutoff table: control voltage vs log corner
    static constexpr int CT_N = 96;
    double ctLogF0 = 0.0, ctDLogF = 1.0;
    double ctVb[CT_N] = {};

    // output coupling (C32 / C7 into the pot): one-pole DC tracker
    double dcOut = 0.0, dcCoef = 0.0;

    // Filter FM (Devilfish) state
    double acCouplingState_ = 0.0;
    static constexpr double acCouplingFreq_ = 20.0;
    static constexpr double filterFmScale_ = 0.4;

    // Open303's resonance skew normaliser, 1 - exp(-3)
    static constexpr double RES_SKEW_NORM = 0.950213;
  };

  //-----------------------------------------------------------------------------------------------
  // inlined functions:

  INLINE void MS20Filter::setCutoff(double newCutoff, bool /*updateCoefficients*/)
  {
    cutoff = std::clamp(newCutoff, 10.0, 20000.0);
  }

  INLINE void MS20Filter::setResonance(double newResonance, bool /*updateCoefficients*/)
  {
    resonance = newResonance;
    updatePot();
  }

  INLINE double MS20Filter::k35ControlFor(double fc) const
  {
    double x = (std::log(fc) - ctLogF0) / ctDLogF;
    x = std::clamp(x, 0.0, (double)(CT_N - 1) - 1e-9);
    int i = (int)x;
    double f = x - i;
    return ctVb[i] + f * (ctVb[i + 1] - ctVb[i]);
  }

  INLINE void MS20Filter::solveLimiter(double i, double R, double nVt, double& v, double& dvdi) const
  {
    // Newton from the previous solution; the function is monotonic and convex either side of 0
    for (int it = 0; it < 30; ++it)
    {
      double u = std::clamp(v / nVt, -40.0, 40.0);
      double ep = std::exp(u), em = 1.0 / ep;
      double f = v / R + diodeIa * (ep - 1.0) - diodeIb * (em - 1.0) - i;
      double df = 1.0 / R + (diodeIa * ep + diodeIb * em) / nVt;
      double dv = -f / df;
      dv = std::clamp(dv, -2.0 * nVt, 2.0 * nVt);
      v += dv;
      if (std::fabs(dv) < 1e-10) break;
    }
    double u = std::clamp(v / nVt, -40.0, 40.0);
    double ep = std::exp(u);
    dvdi = 1.0 / (1.0 / R + (diodeIa * ep + diodeIb / ep) / nVt);
  }

  INLINE double MS20Filter::pnjlim(double vNew, double vOld, double nVt, double vCrit)
  {
    // antiparallel pair: the negative side mirrored
    const double sg = vNew < 0.0 ? -1.0 : 1.0;
    const double vn = sg * vNew, vo = sg * vOld;
    if (vn <= vCrit || std::fabs(vn - vo) <= 2.0 * nVt) return vNew;
    if (vo > 0.0)
    {
      const double arg = 1.0 + (vn - vo) / nVt;
      return sg * (arg > 0.0 ? vo + nVt * std::log(arg) : vCrit);
    }
    return sg * nVt * std::log(vn / nVt);
  }

  INLINE double MS20Filter::buffer(double x, double xs, double& dydx) const
  {
    if (warmth_ <= 0.0) { dydx = 1.0; return x; }
    const double lam = 2.0 * warmth_;
    const double z = x / xs;
    const double disc = std::max(lam * lam + 4.0 * (1.0 + z + lam), 0.0);
    const double sq = std::sqrt(disc);
    const double r = std::max(0.5 * (sq - lam), 0.0);
    const double norm = 0.5 * (2.0 + lam);   // 1 / small-signal slope at z = 0
    dydx = (r > 0.0 && sq > 0.0) ? norm * 2.0 * r / sq : 0.0;
    return xs * norm * (r * r - 1.0);
  }

  INLINE double MS20Filter::getSample(double in)
  {
    double fc = cutoff;
    if (filterFmDepth_ > 0.0)
    {
      double acAlpha = 2.0 * PI * acCouplingFreq_ / sampleRate;
      acCouplingState_ += acAlpha * (in - acCouplingState_);
      fc *= std::clamp(1.0 + filterFmDepth_ * filterFmScale_ * (in - acCouplingState_), 0.5, 1.5);
    }
    fc = std::min(fc, 0.45 * sampleRate);

    return getSampleK35(in * driveFactor, fc) / driveMakeup;
  }

  //-----------------------------------------------------------------------------------------------
  // Korg35

  INLINE double MS20Filter::getSampleK35(double x, double fc)
  {
    const double vin = x * K35_INPUT_VOLTS;
    const double wl = std::max(0.0, 1.0 - 2.0 * mode);
    const double wh = std::max(0.0, 2.0 * mode - 1.0);
    const double wb = 1.0 - wl - wh;
    const double vX  = wl * K35_PAD_LP * vin;   // Q2 collector (input pad, 217 ohm: taken as ideal)
    const double vH  = wh * K35_PAD_HP * vin;   // bottom of C28
    const double vBp = wb * K35_PAD_LP * vin;   // bottom of R111

    const double vB = k35ControlFor(fc);
    const double isq1 = Q_IS * (1.0 / (1.0 + Q_BF) + 1.0 / Q_BR);   // Q1 open emitter: base-collector diode
    const double isf = Q_IS / Q_BF, isr = Q_IS / Q_BR;

    // trapezoidal histories: i = g v - J
    const double J29 = g29 * v29 + i29, J28 = g28 * v28 + i28, J30 = g30 * v30 + i30, JCd = gCd * vCd + iCd;
    const double nVtD = D_N * VT;
    const double kOp = T / (2.0 * OPAMP_TAU);
    const double dvthdO = fbRth * fbC * fbA / fbS;
    const double gR = 1.0 / (R116 + h31);

    double vth = 0.0, ic29 = 0.0, iR = 0.0, vP = 0.0;
    const double vCritA = nVtD * std::log(nVtD / (std::sqrt(2.0) * diodeIa)), vCritB = nVtD * std::log(nVtD / (std::sqrt(2.0) * diodeIb));
    converged = false;
    for (int it = 0; it < 32; ++it)
    {
      // JFET follower into the op amp's + input
      double dbuf;
      vP = JFET_BUF_GAIN * buffer(vZ - vZ0, K35_BUF_SWING, dbuf);
      const double dPdZ = JFET_BUF_GAIN * dbuf;

      // gain stage: N = op amp - input; R116 through C31 (trapezoidal) to ground; R117 || D20 / D21 || their
      // junction capacitance from the output O
      iR = (vN - vC31 - h31 * iR116) * gR;
      const double tO = std::tanh(vI / OPAMP_RAIL);
      vO = OPAMP_RAIL * tO;
      const double dO = 1.0 - tO * tO;
      const double vd = vO - vN;
      const double u = std::clamp(vd / nVtD, -80.0, 80.0);
      const double ep = std::exp(u), em = 1.0 / ep;
      const double iF = vd / R117 + diodeIa * (ep - 1.0) - diodeIb * (em - 1.0) + gCd * vd - JCd;
      const double gF = 1.0 / R117 + (diodeIa * ep + diodeIb * em) / nVtD + gCd;

      // feedback network Thevenin at G: vG = vth + Rth i29
      const double vE = vO - dcOut;
      vth = fbRth * (fbD * vBp - J30 + fbC * (fbA * vE + J30) / fbS);
      ic29 = (g29 * (vY - vth) - J29) / (1.0 + g29 * fbRth);
      const double di29dY = g29 / (1.0 + g29 * fbRth);
      const double di29dO = -di29dY * dvthdO;
      const double ic28 = g28 * (vZ - vH) - J28;

      // junctions
      const double eQ1 = std::exp(std::clamp((vA - vBn) / vtj, -80.0, 80.0));
      const double eBE2 = std::exp(std::clamp((vA - vY) / vtj, -80.0, 80.0));
      const double eBC2 = std::exp(std::clamp((vA - vX) / vtj, -80.0, 80.0));
      const double eBE3 = std::exp(std::clamp((vA - vZ) / vtj, -80.0, 80.0));
      const double eBC3 = eBE2;   // Q3's collector is Q2's emitter (node Y)

      const double iQ1 = isq1 * (eQ1 - 1.0);
      const double iB2 = isf * (eBE2 - 1.0) + isr * (eBC2 - 1.0);
      const double iE2 = Q_IS * (eBE2 - eBC2) + isf * (eBE2 - 1.0);
      const double iB3 = isf * (eBE3 - 1.0) + isr * (eBC3 - 1.0);
      const double iE3 = Q_IS * (eBE3 - eBC3) + isf * (eBE3 - 1.0);
      const double iC3 = Q_IS * (eBE3 - eBC3) - isr * (eBC3 - 1.0);

      // residuals: KCL at A, Bn, Y, Z, N (currents), and the op amp (scaled to amperes by OPAMP_SCALE)
      double F[6] = {
        (VCC - vA) / R3 - iQ1 - iB2 - iB3,
        iQ1 - (vBn - vB) / RB,
        iE2 - iC3 - ic29,
        iE3 - ic28,
        iF - iR,
        OPAMP_SCALE * (vI - vIp - kOp * ((OPAMP_A0 * (vP - vN) - vI) + opPrev)) };

      // Jacobian (columns: vA, vBn, vY, vZ, vN, vI); d e / d v = e / vtj
      const double q1 = isq1 * eQ1 / vtj;
      const double be2 = eBE2 / vtj, bc2 = eBC2 / vtj, be3 = eBE3 / vtj;
      const double bc3 = be2;
      const double dIB2dA = isf * be2 + isr * bc2, dIB2dY = -isf * be2;
      const double dIE2dA = Q_IS * (be2 - bc2) + isf * be2, dIE2dY = -Q_IS * be2 - isf * be2;
      const double dIB3dA = isf * be3 + isr * bc3, dIB3dY = -isr * bc3, dIB3dZ = -isf * be3;
      const double dIE3dA = Q_IS * (be3 - bc3) + isf * be3, dIE3dY = Q_IS * bc3, dIE3dZ = -Q_IS * be3 - isf * be3;
      const double dIC3dA = Q_IS * (be3 - bc3) - isr * bc3, dIC3dY = Q_IS * bc3 + isr * bc3, dIC3dZ = -Q_IS * be3;

      double Jm[6][6] = {
        { -1.0 / R3 - q1 - dIB2dA - dIB3dA, q1, -dIB2dY - dIB3dY, -dIB3dZ, 0.0, 0.0 },
        { q1, -q1 - 1.0 / RB, 0.0, 0.0, 0.0, 0.0 },
        { dIE2dA - dIC3dA, 0.0, dIE2dY - dIC3dY - di29dY, -dIC3dZ, 0.0, -di29dO * dO },
        { dIE3dA, 0.0, dIE3dY, dIE3dZ - g28, 0.0, 0.0 },
        { 0.0, 0.0, 0.0, 0.0, -gF - gR, gF * dO },
        { 0.0, 0.0, 0.0, -OPAMP_SCALE * kOp * OPAMP_A0 * dPdZ, OPAMP_SCALE * kOp * OPAMP_A0, OPAMP_SCALE * (1.0 + kOp) } };

      double dx[6];
      solveLinear<6>(Jm, F, dx);
      // step limiting. The limiter diodes (SPICE's pnjlim): a step that carries their voltage O - N far into
      // conduction is shortened, along the Newton direction, to the voltage of the current the linearised diode
      // predicts (else Newton walks back down the exponential by n VT per iteration). Then the junction nodes by 0.1 V
      // and the op amp's nodes N / I by 2 V, so they cross the diodes' gap and reach the rails in a few steps.
      const double dvd = dO * dx[5] - dx[4];
      const double vdNew = pnjlim(vd + dvd, vd, nVtD, vd + dvd > 0.0 ? vCritA : vCritB);
      if (dvd != 0.0) { const double sc = std::clamp((vdNew - vd) / dvd, 0.0, 1.0); for (double& d : dx) d *= sc; }
      double mx = 0.0;
      for (int k = 0; k < 6; ++k)
      {
        const double lim = k < 4 ? 0.1 : 2.0;
        dx[k] = std::clamp(dx[k], -lim, lim); mx = std::max(mx, std::fabs(dx[k]));
      }
      vA += dx[0]; vBn += dx[1]; vY += dx[2]; vZ += dx[3]; vN += dx[4]; vI += dx[5];
      if (mx < 1e-9) { converged = true; break; }
    }

    // capacitor and op amp histories
    vO = OPAMP_RAIL * std::tanh(vI / OPAMP_RAIL);
    const double vE = vO - dcOut;
    const double vG = vth + fbRth * ic29;
    i29 = ic29; v29 = vY - vG;
    const double v28n = vZ - vH;
    i28 = g28 * v28n - J28; v28 = v28n;
    const double vF = (fbA * vE + J30 + fbC * vG) / fbS;   // C30 (F -> G), from the F node equation
    const double v30n = vF - vG;
    i30 = g30 * v30n - J30; v30 = v30n;
    const double vCdn = vO - vN;
    iCd = gCd * vCdn - JCd; vCd = vCdn;
    vC31 += h31 * (iR + iR116);
    iR116 = iR;
    opPrev = OPAMP_A0 * (vP - vN) - vI;
    vIp = vI;
    // output coupling (C32 into the pot, ~1 Hz)
    dcOut += dcCoef * (vO - dcOut);

    const double gain = (wl * gainLp + wb * gainBp + wh * gainHp) * K35_INPUT_VOLTS;
    return vE / gain;
  }

} // end namespace dfl

#endif // dfl_MS20Filter_h
