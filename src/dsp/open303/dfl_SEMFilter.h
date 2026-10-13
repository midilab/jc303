// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Lowenfels

#ifndef dfl_SEMFilter_h
#define dfl_SEMFilter_h

// Oberheim SEM state-variable filter, circuit-native.
//
// A 12 dB / octave SVF (SEM-1A schematic, VCF section) on +-15 V:
//
//  * A12 (301A) sums the input (R155 220k, C15 0.15u), LP (R138 100k || C18 10p) and its own output (R139 100k); its
//    output is HP. Each CA3080 (A13 / A15) takes the previous output through a 100k / 220R divider into its + input
//    and drives a 300 pF capacitor (C20 / C21) to ground, buffered by a 2N4302 JFET source follower (Q39 / Q40, R147 /
//    R153 15k to -15 V): BP and LP.
//  * Resonance is a damper: BP across the RES pot (PS16, 50k to ground), the wiper through R157 33k and C22 0.33u into
//    A12's input, so turning RES up (the wiper toward ground) takes damping away; and BP through R159 47k / R158 10k,
//    D11 / D12 and R156 100k into the same node: the damping that grows with level, which holds the oscillation.
//
// Each sample solves eight nodes (A12's input, the two capacitor nodes and their followers, the diode string and the
// C22 node; trapezoidal capacitors, Newton); the RES pot's wiper is eliminated as a resistive star, A12's output is an
// explicit function of its input (open-loop gain with soft rails). Checked against an ngspice model with the same
// device equations (tools/sem-fidelity).
//
// The LP / BP / HP morph mixes the three outputs (one sign, so the crossfades have no notches); the circuit runs
// unchanged in every mode. Saturation Bias offsets OTA1's input (its trim, T9), Saturation Rail sags the +-15 V
// supplies.

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

  class SEMFilter
  {

  public:

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    SEMFilter();
    ~SEMFilter();

    //---------------------------------------------------------------------------------------------
    // parameter settings:

    void setSampleRate(double newSampleRate);
    INLINE void setCutoff(double newCutoff, bool updateCoefficients = true);
    INLINE void setResonance(double newResonance, bool updateCoefficients = true);
    void setInputDrive(double newDrive);
    /** Response mode, 0 = LP, 0.5 = BP, 1 = HP (mixes the outputs). Cheap, so it may change per sample. */
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
    // SEM specific:

    /** The RES pot position, 0..1 (the resonance knob with Open303's skew undone; the pot is linear). */
    double getResPot() const { return resPot; }

    /** Saturation bias, 0..1: an input offset of up to BIAS_MAX_VT thermal voltages on OTA1 (its offset trim, T9, off
        null), its resting current trimmed out as on the MS-20 / IR3109 models: the same slope at rest, unequal
        current limits once HP drives it. 0 = the circuit, trimmed. */
    void setSaturationBias(double b) { satBias_ = std::clamp(b, 0.0, 1.0); updateBias(); }
    double getSaturationBias() const { return satBias_; }

    /** Supply rail, 0..1: 1 = +-15 V, lower sags both rails by up to RAIL_SAG of themselves (A12's and the OTAs'
        headroom). */
    void setRail(double r) { rail_ = std::clamp(r, 0.0, 1.0); updateSupply(); }
    double getRail() const { return rail_; }

    /** Filter FM depth (0..1): Devilfish-style audio-rate cutoff modulation from the AC-coupled input. */
    void setFilterFmDepth(double amount) { filterFmDepth_ = std::clamp(amount, 0.0, 1.0); }
    double getFilterFmDepth() const { return filterFmDepth_; }

    /** Circuit node voltages (volts re ground), for the fidelity tools: 0 A12 in, 1 C20, 2 BP, 3 C21, 4 LP, 5 the
        R159 / R158 node, 6 the D11 / D12 - R156 node, 7 the C22 node; 8 = HP (A12's output). */
    double getNode(int k) const { return k == 8 ? hp : v[std::clamp(k, 0, NN - 1)]; }
    double getSupply() const { return vcc; }
    /** OTA bias current for a cutoff (amps). */
    double getIabc(double fc) const { return otaIabc(fc); }

    //---------------------------------------------------------------------------------------------
    // circuit constants (public for the fidelity tools):

    static constexpr int NN = 8;
    static constexpr double VT = 0.025865;   // thermal voltage at SPICE's 27 C

    // Components (SEM-1A schematic)
    static constexpr double R155 = 220e3, C15 = 0.15e-6;          // input (one mixer channel)
    static constexpr double R138 = 100e3, C18 = 10e-12, R139 = 100e3;
    static constexpr double R_DIV = 100e3, R_DIV_LO = 220.0;      // R142 / R144, R148 / R150
    static constexpr double C20 = 300e-12, C21 = 300e-12;
    static constexpr double R147 = 15e3, R153 = 15e3;             // follower loads to -15 V
    static constexpr double R157 = 33e3, C22 = 0.33e-6, R_RES = 50e3;
    static constexpr double R159 = 47e3, R158 = 10e3, R156 = 100e3;
    static constexpr double VCC = 15.0;

    // A12 (LM301A): open-loop gain, output swing below the rails (soft)
    static constexpr double OPAMP_GAIN = 1e5, OPAMP_SWING_DROP = 1.5;
    // CA3080: i = Iabc tanh(v / 2 VT); output compliance VCLAMP from either rail (clamp junctions)
    static constexpr double OTA_VCLAMP = 1.5, CLAMP_IS = 1e-14;
    static constexpr double IABC_MAX = 2e-3;
    // 2N4302 (a guess within its datasheet: IDSS 0.5..5 mA): square law, saturation (the drain is on +15 V)
    static constexpr double JFET_IDSS = 2e-3, JFET_VTO = -1.5;
    static constexpr double JFET_BETA = JFET_IDSS / (JFET_VTO * JFET_VTO);
    // D11 / D12 (1N4148, as the other models)
    static constexpr double D_IS = 2.52e-9, D_N = 1.752;

    // Character ranges
    static constexpr double BIAS_MAX_VT = 2.0, RAIL_SAG = 0.3;

    // Input level: volts at R155 for a full-scale (+-1) signal at 0 dB drive (the SEM's oscillators, 10 V p-p, mixer
    // at full). The output is divided by driveFactor^MAKEUP_EXP: the SEM is clean up to A12's rails, so full make-up
    // keeps LP within 0.1 dB from 0 to +16 dB at RES 0 (tools/sem-fidelity/levels).
    static constexpr double INPUT_VOLTS = 5.0;
    static constexpr double MAKEUP_EXP = 1.0;
    // The outputs' passband gain (LP and HP alike: R138 / R155 = R139 / R155), divided out to unity at 0 dB drive
    static constexpr double OUT_GAIN = INPUT_VOLTS * R139 / R155;
    // BP / HP output trims (dB): the pure modes sit near LP loudness on a saw at a typical resonant setting (pot
    // 0.5..0.8, cutoff 1 kHz, tools/sem-fidelity/levels), as the other circuit models'; large, as the SEM's damping
    // at low RES is heavy (Q ~1/3)
    static constexpr double BP_TRIM_DB = 9.0, HP_TRIM_DB = 8.8;

    //=============================================================================================

  protected:

    void updateSupply();
    void updateBias();
    void updatePot();

    /** Solves the circuit for one sample at input voltage vin (at R155) and OTA bias current ib. */
    INLINE void solve(double vin, double ib);

    /** Iabc for an integrator crossover at fc (prewarped for the trapezoidal integrators; the followers' gain below
        one is the circuit's). */
    INLINE double otaIabc(double fc) const
    {
      const double w = std::min(2.0 / T * std::tan(PI * std::min(fc, 0.45 * sampleRate) * T), 0.49 * PI * 2.0 * sampleRate);
      return std::clamp(w * C20 * 2.0 * VT / kDiv, 1e-13, IABC_MAX);
    }

    /** A12's output for its - input vn (+ input on ground); dHP / dvn. */
    INLINE double opamp(double vn, double& d) const
    {
      const double x = -OPAMP_GAIN * vn / vSwing, t = std::tanh(x);
      d = -OPAMP_GAIN * (1.0 - t * t);
      return vSwing * t;
    }

    /** CA3080 output current for a differential input vd (in thermal-voltage units already offset) into an output at
        vo; di / dvd, di / dvo. */
    INLINE double ota(double vd, double vo, double ib, double off, double offT, double& dvd, double& dvo) const;

    /** JFET source follower: drain current for gate vg and source vs; d / dvg (= -d / dvs). */
    INLINE double jfet(double vg, double vs, double& gm) const
    {
      const double u = vg - vs - JFET_VTO;
      if (u <= 0.0) { gm = 0.0; return 0.0; }
      gm = 2.0 * JFET_BETA * u;
      return JFET_BETA * u * u;
    }

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
    double satBias_ = 0.0, rail_ = 1.0;

    // derived
    double vcc = VCC, vSwing = VCC - OPAMP_SWING_DROP;
    double kDiv = R_DIV_LO / (R_DIV + R_DIV_LO), gDiv = 1.0 / (R_DIV + R_DIV_LO);
    double off1 = 0.0, off1T = 0.0;                   // OTA1's input offset (2 VT units) and its tanh
    double gBN = 0.0, gNG = 0.0, gBG = 0.0;           // RES star eliminated: BP-C22 node, C22 node-ground, BP-ground
    double h15 = 0.0, gc18 = 0.0, gc20 = 0.0, gc21 = 0.0, gc22 = 0.0;
    double gainLp = 1.0, gainBp = 1.0, gainHp = 1.0;

    // state: node voltages, A12's output, capacitor voltages and currents (trapezoidal)
    double v[NN] = {};
    double vPrev[NN] = {};               // the nodes one sample back (the predictor)
    double hp = 0.0;
    double vC15 = 0.0, iC15 = 0.0, vC18 = 0.0, iC18 = 0.0, vC20 = 0.0, iC20 = 0.0;
    double vC21 = 0.0, iC21 = 0.0, vC22 = 0.0, iC22 = 0.0;
    double rest[3] = {};                 // LP / BP / HP at rest (the DC operating point)
    bool dcSolve = false;                // reset(): capacitors open
    bool converged = true;               // the last sample's Newton converged (else no predictor)

    // Filter FM (Devilfish) state
    double acCouplingState_ = 0.0;
    static constexpr double acCouplingFreq_ = 20.0;
    static constexpr double filterFmScale_ = 0.4;

    // Open303's resonance skew normaliser, 1 - exp(-3)
    static constexpr double RES_SKEW_NORM = 0.950213;
  };

  //-----------------------------------------------------------------------------------------------
  // inlined functions:

  namespace sem_detail
  {
    /** exp with a linear continuation above x = 40 (keeps Newton finite). */
    INLINE double limExp(double x, double& d)
    {
      if (x > 40.0) { const double e = std::exp(40.0); d = e; return e * (1.0 + x - 40.0); }
      d = std::exp(x);
      return d;
    }
  }

  namespace sem_detail
  {
    /** Solves J dx = -F with the SEM's sparsity: C20 (1) hangs on A12's input (0) only, C21 (3) on BP (2), LP (4) on
        A12's input and C21. Those three go in closed form (pivots: a capacitor's conductance, the follower's), then a
        5 x 5 in A12's input, BP, the diode string's two nodes and the C22 node. Not for the DC solve. */
    INLINE void solveSparse(const double (&J)[8][8], const double (&F)[8], double (&dx)[8])
    {
      const double a1 = -F[1] / J[1][1], b1 = -J[1][0] / J[1][1];                     // x1 = a1 + b1 x0
      const double a3 = -F[3] / J[3][3], c3 = -J[3][2] / J[3][3];                     // x3 = a3 + c3 x2
      const double a4 = (-F[4] - J[4][3] * a3) / J[4][4], b4 = -J[4][0] / J[4][4];
      const double c4 = -J[4][3] * c3 / J[4][4];                                       // x4 = a4 + b4 x0 + c4 x2
      // reduced rows 0, 2, 5, 6, 7 in x0, x2, x5, x6, x7
      double R[5][5] = {
        { J[0][0] + J[0][4] * b4, J[0][4] * c4, 0.0, 0.0, J[0][7] },
        { J[2][1] * b1, J[2][2], J[2][5], 0.0, J[2][7] },
        { 0.0, J[5][2], J[5][5], J[5][6], 0.0 },
        { 0.0, 0.0, J[6][5], J[6][6], J[6][7] },
        { J[7][0], J[7][2], 0.0, J[7][6], J[7][7] } };
      double G[5] = { F[0] + J[0][4] * a4, F[2] + J[2][1] * a1, F[5], F[6], F[7] };
      double y[5];
      solveLinear<5>(R, G, y);
      dx[0] = y[0]; dx[2] = y[1]; dx[5] = y[2]; dx[6] = y[3]; dx[7] = y[4];
      dx[1] = a1 + b1 * y[0];
      dx[3] = a3 + c3 * y[1];
      dx[4] = a4 + b4 * y[0] + c4 * y[1];
    }
  }

  INLINE void SEMFilter::setCutoff(double newCutoff, bool /*updateCoefficients*/)
  {
    cutoff = std::clamp(newCutoff, 10.0, 20000.0);
  }

  INLINE void SEMFilter::setResonance(double newResonance, bool /*updateCoefficients*/)
  {
    resonance = newResonance;
    updatePot();
  }

  INLINE double SEMFilter::ota(double vd, double vo, double ib, double off, double offT, double& dvd, double& dvo) const
  {
    // tanh pair (an input offset off, its resting current offT trimmed out, the slope renormalised), then the output's
    // compliance: junctions OTA_VCLAMP inside either rail
    const double g = ib / (1.0 - offT * offT);
    const double t = std::tanh(vd / (2.0 * VT) + off);
    dvd = g * (1.0 - t * t) / (2.0 * VT);
    double i = g * (t - offT);
    dvo = 0.0;
    const double vHi = vcc - OTA_VCLAMP, vLo = -vcc + OTA_VCLAMP;
    double e;
    if (vo > vHi - 0.3)
    {
      const double x = sem_detail::limExp((vo - vHi) / VT, e);
      i -= CLAMP_IS * (x - 1.0); dvo -= CLAMP_IS * e / VT;
    }
    if (vo < vLo + 0.3)
    {
      const double x = sem_detail::limExp((vLo - vo) / VT, e);
      i += CLAMP_IS * (x - 1.0); dvo -= CLAMP_IS * e / VT;
    }
    return i;
  }

  INLINE void SEMFilter::solve(double vin, double ib)
  {
    constexpr double G138 = 1.0 / R138, G139 = 1.0 / R139, G147 = 1.0 / R147, G153 = 1.0 / R153;
    constexpr double G159 = 1.0 / R159, G158 = 1.0 / R158, G156 = 1.0 / R156;
    const double nVtD = D_N * VT;

    // capacitor companions (open at DC)
    const double gr = dcSolve ? 0.0 : 1.0 / (R155 + h15);
    const double e1 = vin - vC15 - h15 * iC15;
    const double c18 = dcSolve ? 0.0 : gc18, c20 = dcSolve ? 0.0 : gc20, c21 = dcSolve ? 0.0 : gc21, c22 = dcSolve ? 0.0 : gc22;
    const double J18 = dcSolve ? 0.0 : gc18 * vC18 + iC18, J20 = dcSolve ? 0.0 : gc20 * vC20 + iC20;
    const double J21 = dcSolve ? 0.0 : gc21 * vC21 + iC21, J22 = dcSolve ? 0.0 : gc22 * vC22 + iC22;

    const int maxIt = dcSolve ? 200 : 40;
    const double tol = dcSolve ? 1e-10 : 1e-6;
    // predictor: the nodes' linear extrapolation from the last two samples (not after a sample that did not converge)
    if (!dcSolve)
      for (int k = 0; k < NN; ++k) { const double p = converged ? 2.0 * v[k] - vPrev[k] : v[k]; vPrev[k] = v[k]; v[k] = p; }
    // the range each node can physically reach: A12's input within the input's reach, the rest within the rails
    const double vMax = vcc + 1.0, sMax = std::fabs(vin) + vcc + 1.0;
    converged = dcSolve;

    for (int it = 0; it < maxIt; ++it)
    {
      const double vS = v[0], vc1 = v[1], vBP = v[2], vc2 = v[3], vLP = v[4], vD = v[5], vM = v[6], vN = v[7];

      double dh;
      const double h = opamp(vS, dh);
      double o1d, o1o, o2d, o2o;
      const double i1 = ota(kDiv * h, vc1, ib, off1, off1T, o1d, o1o);
      const double i2 = ota(kDiv * vBP, vc2, ib, 0.0, 0.0, o2d, o2o);
      double gm1, gm2;
      const double id1 = jfet(vc1, vBP, gm1), id2 = jfet(vc2, vLP, gm2);
      double dp, dm;
      const double ep = sem_detail::limExp((vD - vM) / nVtD, dp), em = sem_detail::limExp((vM - vD) / nVtD, dm);
      const double idd = D_IS * (ep - em), gdd = D_IS * (dp + dm) / nVtD;

      const double i18 = c18 * (vLP - vS) - J18, i22 = c22 * (vN - vS) - J22;

      double F[NN], J[NN][NN] = {};
      F[0] = (e1 - vS) * gr + (vLP - vS) * G138 + i18 + (h - vS) * G139 + i22;
      J[0][0] = -gr - G138 - c18 + (dh - 1.0) * G139 - c22; J[0][4] = G138 + c18; J[0][7] = c22;

      F[1] = i1 - (c20 * vc1 - J20);
      J[1][0] = o1d * kDiv * dh; J[1][1] = o1o - c20;

      F[2] = id1 - (vBP + vcc) * G147 - vBP * gDiv - (vBP - vD) * G159 - (vBP - vN) * gBN - vBP * gBG;
      J[2][1] = gm1; J[2][2] = -gm1 - G147 - gDiv - G159 - gBN - gBG; J[2][5] = G159; J[2][7] = gBN;

      F[3] = i2 - (c21 * vc2 - J21);
      J[3][2] = o2d * kDiv; J[3][3] = o2o - c21;

      F[4] = id2 - (vLP + vcc) * G153 - (vLP - vS) * G138 - i18;
      J[4][3] = gm2; J[4][4] = -gm2 - G153 - G138 - c18; J[4][0] = G138 + c18;

      F[5] = (vBP - vD) * G159 - vD * G158 - idd;
      J[5][2] = G159; J[5][5] = -G159 - G158 - gdd; J[5][6] = gdd;

      F[6] = idd - (vM - vN) * G156;
      J[6][5] = gdd; J[6][6] = -gdd - G156; J[6][7] = G156;

      F[7] = (vM - vN) * G156 + (vBP - vN) * gBN - vN * gNG - i22;
      J[7][6] = G156; J[7][2] = gBN; J[7][7] = -G156 - gBN - gNG - c22; J[7][0] = c22;

      double dx[NN];
      if (dcSolve) solveLinear<NN>(J, F, dx);
      else sem_detail::solveSparse(J, F, dx);
      double mx = 0.0;
      for (int k = 0; k < NN; ++k)
      {
        // A12's input in units of its tanh argument: at most 2 from the linear region, back to it in one step from
        // saturation (where the slope gives Newton nothing to go on)
        const double xs = std::fabs(OPAMP_GAIN * v[0] / vSwing);
        const double lim = k == 0 ? std::max(2.0, xs) * vSwing / OPAMP_GAIN : (k == 5 || k == 6 ? 2.0 * nVtD + 0.5 : 2.0);
        const double d = std::clamp(dx[k], -lim, lim);
        v[k] = std::clamp(v[k] + d, -(k == 0 ? sMax : vMax), k == 0 ? sMax : vMax); mx = std::max(mx, std::fabs(d) * (k == 0 ? OPAMP_GAIN : 1.0));   // A12 in: as its output step
      }
      if (mx < tol) { converged = true; break; }   // quadratic convergence: the error left after a step of tol is ~tol^2
    }

    double dh;
    hp = opamp(v[0], dh);
    if (dcSolve) return;
    // capacitor states
    const double i15n = (e1 - v[0]) * gr;
    vC15 += h15 * (i15n + iC15); iC15 = i15n;
    vC18 = v[4] - v[0]; iC18 = gc18 * vC18 - J18;
    vC20 = v[1];        iC20 = gc20 * vC20 - J20;
    vC21 = v[3];        iC21 = gc21 * vC21 - J21;
    vC22 = v[7] - v[0]; iC22 = gc22 * vC22 - J22;
  }

  INLINE double SEMFilter::getSample(double in)
  {
    double fc = cutoff;
    if (filterFmDepth_ > 0.0)
    {
      double acAlpha = 2.0 * PI * acCouplingFreq_ / sampleRate;
      acCouplingState_ += acAlpha * (in - acCouplingState_);
      fc *= std::clamp(1.0 + filterFmDepth_ * filterFmScale_ * (in - acCouplingState_), 0.5, 1.5);
    }

    solve(in * driveFactor * INPUT_VOLTS, otaIabc(fc));

    // the three outputs share A12's inversion (the integrators do not invert), so LP -> BP -> HP keep one sign; the
    // output is turned back upright
    const double lp = (v[4] - rest[0]) / gainLp, bp = (v[2] - rest[1]) / gainBp, hpo = (hp - rest[2]) / gainHp;
    const double wl = std::max(0.0, 1.0 - 2.0 * mode), wh = std::max(0.0, 2.0 * mode - 1.0), wb = 1.0 - wl - wh;
    return -(wl * lp + wb * bp + wh * hpo) / driveMakeup;
  }

} // end namespace dfl

#endif // dfl_SEMFilter_h
