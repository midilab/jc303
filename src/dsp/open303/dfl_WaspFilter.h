// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Lowenfels

#ifndef dfl_WaspFilter_h
#define dfl_WaspFilter_h

// EDP Wasp state-variable filter, circuit-native.
//
// A 12 dB / octave SVF whose summing amplifier and two integrators are CD4069UB CMOS inverters run as op amps, with
// CA3080 OTAs as the integrators' voltage-controlled resistors, all on one supply:
//
//  * IC1 (inverter) sums the input (R1 / C1), LP (R2 || C2), its own output (R3) and the resonance network; its output
//    is HP. Each OTA takes the previous output through a divider (R5 / R6, biased at half the supply by R7 / R8) and
//    feeds an inverter integrator (C3 / C4 across IC3 / IC5): IC3's output is BP, IC5's LP.
//  * Resonance: BP returns to IC1's input through R14, the RES pot (50k) and R4, its far end shunted by C7, so the
//    pot fades the BP damping out above ~40 Hz; and through R13 / R15 and D1 / D2, a soft limiter on the damping.
//
// Revisions: the EDP original (+5 V, R13 100k) and the Doepfer A-124 (+12 V, R13 1M). Component values, the CD4069
// MOSFET model (square law with gate-dependent alpha / VT, eq. 33-34, Table 2) and the CA3080 model with its output
// stage at the rails (eq. 35-41, Tables 3-4) from Koper, Holters, Esqueda, Parker, "A Virtual Analog Model of the
// EDP Wasp VCF", DAFx 2022 (fitted to measurements at +12 V); the +5 V values cross-checked against J. Haible's EDP
// clone (2006). The inverters' low, level-dependent gain is the Wasp's dirt: the virtual grounds move, and at
// resonance the OTA outputs run into their rails.
//
// Each sample solves the eight circuit nodes (trapezoidal capacitors, Newton); the RES pot's wiper is eliminated as a
// resistive star. Checked against an ngspice model with the same device equations (tools/wasp-fidelity).
//
// The LP / BP / HP morph mixes the three outputs (one sign, so the crossfades have no notches); the circuit runs
// unchanged in every mode. Saturation Bias offsets OTA1's input divider (an asymmetric operating point for IC1),
// Saturation Rail sags the supply.

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

  class WaspFilter
  {

  public:

    /** Circuit revision; the value indexes the per-revision constants. */
    enum class Revision { EDP, A124 };

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. */
    explicit WaspFilter(Revision newRevision = Revision::EDP);

    /** Destructor. */
    ~WaspFilter();

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
    // Wasp specific:

    /** Selects the circuit revision (resets the state). */
    void setRevision(Revision newRevision);
    Revision getRevision() const { return revision; }

    /** The RES pot position, 0..1 (the resonance knob with Open303's skew undone; the pot is linear). */
    double getResPot() const { return resPot; }

    /** Saturation bias, 0..1: OTA1's + input divider (R7 / R8) moves from half the supply by up to BIAS_SHIFT of it.
        The DC loop then holds HP there, off IC1's switching point, so IC1 clips sooner on one side. 0 = the circuit. */
    void setSaturationBias(double b) { satBias_ = std::clamp(b, 0.0, 1.0); updateSupply(); }
    double getSaturationBias() const { return satBias_; }

    /** Supply rail, 0..1: 1 = the circuit's supply, lower sags it by up to RAIL_SAG of itself (less headroom, less
        inverter gain). */
    void setRail(double r) { rail_ = std::clamp(r, 0.0, 1.0); updateSupply(); }
    double getRail() const { return rail_; }

    /** Filter FM depth (0..1): Devilfish-style audio-rate cutoff modulation from the AC-coupled input. */
    void setFilterFmDepth(double amount) { filterFmDepth_ = std::clamp(amount, 0.0, 1.0); }
    double getFilterFmDepth() const { return filterFmDepth_; }

    /** Circuit node voltages (volts re ground), for the fidelity tools: 0 IC1 in, 1 HP, 2 IC3 in, 3 BP, 4 IC5 in,
        5 LP, 6 the D1 / D2 node, 7 the C7 node. */
    double getNode(int k) const { return v[std::clamp(k, 0, NN - 1)]; }
    double getSupply() const { return vdd; }
    /** OTA bias current for a cutoff (amps). */
    double getIabc(double fc) const { return otaIabc(fc); }
    /** Volts at R1 for a full-scale input at 0 dB drive. */
    double getInputVolts() const { return inputVolts; }
    /** Whether the last sample's Newton solve converged (for the fidelity tools). */
    bool isConverged() const { return converged; }

    //---------------------------------------------------------------------------------------------
    // circuit constants (public for the fidelity tools):

    static constexpr int NN = 8;
    static constexpr double VT = 0.025865;   // thermal voltage at SPICE's 27 C

    // Components (DAFx 2022 Table 1; the EDP's R13 and supply from its text)
    static constexpr double R1 = 27e3, R2 = 27e3, R3 = 27e3, R4 = 27e3, R7 = 27e3, R8 = 27e3, R11 = 27e3, R12 = 27e3;
    static constexpr double R5 = 47e3, R9 = 47e3, R6 = 1e3, R10 = 1e3, R14 = 1e3, R15 = 100e3;
    static constexpr double C1 = 0.22e-6, C7 = 0.22e-6, C2 = 100e-12, C3 = 330e-12, C4 = 330e-12;
    static constexpr double R_RES = 50e3;
    static constexpr double VDD_EDP = 5.0;
    // per revision (EDP, A-124): supply, R13
    static constexpr double VDD[2] = { VDD_EDP, 12.0 };
    static constexpr double R13[2] = { 100e3, 1e6 };

    // CD4069 MOSFETs (Table 2): alpha (A / V^2) and VT (V) as polynomials in vGS (PMOS: vGS < 0)
    static constexpr double NA0 = 0.0022, NA1 = -1.0471e-4, NA2 = 3.1252e-6;
    static constexpr double NT0 = 0.8655, NT1 = 0.2212, NT2 = 0.0149, N_LAMBDA = 1e-3;
    static constexpr double PA0 = -4.9942e-4, PA1 = -5.1393e-4, PA2 = -6.7622e-5, PA3 = -2.7258e-6;
    static constexpr double PT0 = 0.6239, PT1 = -0.1941, PT2 = 0.0135, P_LAMBDA = 0.06;
    static constexpr double ALPHA_MIN = 1e-6;
    // CMOS input / output protection diodes to the rails
    static constexpr double PROT_IS = 1e-14;

    // CA3080 (Tables 3-4): i = alpha Iabc tanh(beta v / 2 VT), with the output stage's currents near the rails
    static constexpr double OTA_ALPHA = 0.8635, OTA_BETA = 0.9408;
    static constexpr double PL1 = 1e-9, PL2 = 0.67938, PL3 = 1.27987e-7, PL4 = 101.18857, PL5 = 2.59819;
    static constexpr double PH1 = 91.7e-9, PH2 = -1.13642, PH3 = -72e-9, PH4 = -170.11701, PH5 = -5.4144;
    static constexpr double GAMMA_L = 30.14, GAMMA_H = 16.0255;
    static constexpr double DV_L = 0.6093, DV_H_DROP = 12.0 - 10.8105;   // from ground / below the supply
    static constexpr double IABC_MAX = 2e-3;

    // D1 / D2 (1N4148, fitted in the paper)
    static constexpr double D_IS = 2.52e-9, D_N = 1.752;

    // Newton: iterations per sample, the restart from the last sample, the largest node step (a fraction of the supply),
    // the line search (sufficient decrease, halvings, the squared residual (A^2) below which any step is taken)
    static constexpr int MAX_IT = 50, RESTART_IT = 30, LS_HALVINGS = 6;
    static constexpr double MAX_STEP = 0.2, LS_ALPHA = 1e-4, LS_FLOOR = 1e-24;

    // Character ranges
    static constexpr double BIAS_SHIFT = 0.15, RAIL_SAG = 0.3;

    // Input level: volts at R1 for a full-scale (+-1) signal at 0 dB drive, per revision (the model has R1 as the
    // circuit's input; R3 / R1 = 1, so these are also the volts of the equivalent unity-gain input):
    //  * EDP: the oscillators are 5 V CMOS logic; a full square is 5 V p-p, +-2.5 V after the input capacitor. The EDP
    //    Gnat (same designer, oscillators and filter topology; T. Stinchcombe's schematics, sheets 6 and 8) feeds it
    //    through 180k into the 33k summing node, x 0.18, so +-0.46 V at a unity-gain input. Clean (0.01 % THD in the
    //    passband), the dirt from ~1 V (+7 dB drive) up.
    //  * A-124: Eurorack audio is 10 V p-p (+-5 V, Doepfer's A-100 technical notes), into the Level pot (50k) and C1 / R1
    //    (DAFx 2022, Hin); at full Level (the model has no pot) that is +-5 V at R1, ~3 % THD in the passband on its
    //    +12 V rail: Doepfer's manual says to turn Level down if the output distorts.
    // The output is divided by driveFactor^MAKEUP_EXP (per revision): LP within ~+-2.5 dB from 0 to +16 dB at RES 0
    // (tools/wasp-fidelity/levels).
    static constexpr double INPUT_VOLTS[2] = { 0.46, 5.0 };
    static constexpr double MAKEUP_EXP[2] = { 0.92, 0.40 };
    // BP / HP output trims (dB, per revision): the pure modes sit near LP loudness on a saw at a typical resonant
    // setting (pot 0.5..0.8, cutoff 1 kHz, tools/wasp-fidelity/levels), as the MS-20's and IR3109's
    static constexpr double BP_TRIM_DB[2] = { 6.2, 4.0 }, HP_TRIM_DB[2] = { 6.9, 6.7 };

    //=============================================================================================

  protected:

    void updateSupply();
    void updatePot();

    /** Solves the circuit for one sample at input voltage vin (at R1) and OTA bias current ib. */
    INLINE void solve(double vin, double ib);

    /** Iabc for an integrator crossover at fc (prewarped for the trapezoidal integrators). */
    INLINE double otaIabc(double fc) const
    {
      const double w = std::min(2.0 / T * std::tan(PI * std::min(fc, 0.45 * sampleRate) * T), 0.49 * PI * 2.0 * sampleRate);
      return std::clamp(w * 2.0 * C3 * VT / (k1 * OTA_ALPHA * OTA_BETA), 1e-13, IABC_MAX);
    }

    /** Inverter output current into its output node for gate vi and output vo; di / dvi, di / dvo. */
    INLINE double inverter(double vi, double vo, double& dvi, double& dvo) const;

    /** CA3080 output current for a differential input vd and output voltage vo; di / dvd, di / dvo. */
    INLINE double ota(double vd, double vo, double ib, double& dvd, double& dvo) const;

    /** Protection diodes from a node to both rails: current out of the node, and its slope. */
    INLINE double prot(double vn, double& g) const;

    Revision revision;

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
    double vddNom = VDD_EDP, vdd = VDD_EDP;
    double r13 = R13[0];
    double vd1 = 0.5 * VDD_EDP, vd2 = 0.5 * VDD_EDP;   // OTA divider Thevenin voltages
    double dvH = VDD_EDP - DV_H_DROP;                   // OTA high-rail knee
    double k1 = 0.0, k2 = 0.0, gd1 = 0.0, gd2 = 0.0;    // OTA input divider gains, loads
    double gAB = 0.0, gAS = 0.0, gBS = 0.0;             // RES star eliminated: BP-C7 node, BP-IC1 in, C7 node-IC1 in
    double h1 = 0.0, gc2 = 0.0, gc3 = 0.0, gc4 = 0.0, gc7 = 0.0;
    double inputVolts = INPUT_VOLTS[0];
    double gainLp = 1.0, gainBp = 1.0, gainHp = 1.0;

    // state: node voltages, capacitor voltages and currents (trapezoidal)
    double v[NN] = {};
    double vPrev[NN] = {};               // the nodes one sample back (the predictor)
    double vC1 = 0.0, iC1 = 0.0;
    double vC2 = 0.0, iC2 = 0.0, vC3 = 0.0, iC3 = 0.0, vC4 = 0.0, iC4 = 0.0, vC7 = 0.0, iC7 = 0.0;
    double rest[3] = {};                 // LP / BP / HP at rest (the DC operating point)
    double outDc = 0.0, outDcCoef = 0.0; // output coupling capacitor (C6): DC tracker
    bool dcSolve = false;                // reset(): capacitors open
    bool converged = true;               // the last solve met its tolerance

    // Filter FM (Devilfish) state
    double acCouplingState_ = 0.0;
    static constexpr double acCouplingFreq_ = 20.0;
    static constexpr double filterFmScale_ = 0.4;

    // Open303's resonance skew normaliser, 1 - exp(-3)
    static constexpr double RES_SKEW_NORM = 0.950213;
  };

  //-----------------------------------------------------------------------------------------------
  // inlined functions:

  INLINE void WaspFilter::setCutoff(double newCutoff, bool /*updateCoefficients*/)
  {
    cutoff = std::clamp(newCutoff, 10.0, 20000.0);
  }

  INLINE void WaspFilter::setResonance(double newResonance, bool /*updateCoefficients*/)
  {
    resonance = newResonance;
    updatePot();
  }

  namespace wasp_detail
  {
    /** Square-law MOSFET with gate-dependent alpha / VT (magnitudes: g = |vGS|, d = |vDS|; a, t and their slopes
        da, dt in g). Returns |iD|; dIg, dId its slopes. */
    INLINE double mosfet(double a, double da, double t, double dt, double g, double d, double lam,
                         double& dIg, double& dId)
    {
      const double ov = g - t;
      if (ov <= 0.0) { dIg = dId = 0.0; return 0.0; }
      const double dov = 1.0 - dt, cl = 1.0 + lam * d;
      if (d < ov)
      {
        const double core = (ov - 0.5 * d) * d;
        dIg = (da * core + a * dov * d) * cl;
        dId = a * ((ov - d) * cl + core * lam);
        return a * core * cl;
      }
      const double core = 0.5 * ov * ov;
      dIg = (da * core + a * ov * dov) * cl;
      dId = a * core * lam;
      return a * core * cl;
    }

    /** exp with a linear continuation above x = 40 (keeps Newton finite). */
    INLINE double limExp(double x, double& d)
    {
      if (x > 40.0) { const double e = std::exp(40.0); d = e; return e * (1.0 + x - 40.0); }
      d = std::exp(x);
      return d;
    }
  }

  namespace wasp_detail
  {
    /** Solves J dx = -F with the Wasp's sparsity: HP (1) hangs on IC1's input (0) only, IC3's input (2) on HP and BP
        (3), LP (5) on IC1's input and IC5's input (4), which hangs on BP and LP. Those four go in closed form (each
        pivot is the node's own conductance plus its capacitor's, so not for the DC solve), then a 4 x 4 in IC1's
        input, BP, the D1 / D2 node and the C7 node. */
    INLINE void solveSparse(const double (&J)[8][8], const double (&F)[8], double (&dx)[8])
    {
      const double a1 = -F[1] / J[1][1], b1 = -J[1][0] / J[1][1];                     // x1 = a1 + b1 x0
      const double a2 = (-F[2] - J[2][1] * a1) / J[2][2], b2 = -J[2][1] * b1 / J[2][2];
      const double c2 = -J[2][3] / J[2][2];                                            // x2 = a2 + b2 x0 + c2 x3
      const double a5 = -F[5] / J[5][5], b5 = -J[5][0] / J[5][5], d5 = -J[5][4] / J[5][5];   // x5 = a5 + b5 x0 + d5 x4
      const double p4 = J[4][4] + J[4][5] * d5;
      const double a4 = (-F[4] - J[4][5] * a5) / p4, b4 = -J[4][5] * b5 / p4, c4 = -J[4][3] / p4;   // x4 = a4 + b4 x0 + c4 x3
      const double a5x = a5 + d5 * a4, b5x = b5 + d5 * b4, c5x = d5 * c4;             // x5 = a5x + b5x x0 + c5x x3

      double R[4][4] = {
        { J[0][0] + J[0][1] * b1 + J[0][5] * b5x, J[0][3] + J[0][5] * c5x, J[0][6], J[0][7] },
        { J[3][0] + J[3][2] * b2, J[3][3] + J[3][2] * c2, J[3][6], J[3][7] },
        { J[6][0], J[6][3], J[6][6], J[6][7] },
        { J[7][0], J[7][3], J[7][6], J[7][7] } };
      double G[4] = { F[0] + J[0][1] * a1 + J[0][5] * a5x, F[3] + J[3][2] * a2, F[6], F[7] };
      double y[4];
      solveLinear<4>(R, G, y);
      dx[0] = y[0]; dx[3] = y[1]; dx[6] = y[2]; dx[7] = y[3];
      dx[1] = a1 + b1 * y[0];
      dx[2] = a2 + b2 * y[0] + c2 * y[1];
      dx[4] = a4 + b4 * y[0] + c4 * y[1];
      dx[5] = a5x + b5x * y[0] + c5x * y[1];
    }
  }

  INLINE double WaspFilter::inverter(double vi, double vo, double& dvi, double& dvo) const
  {
    using namespace wasp_detail;
    // NMOS: vGS = vi, vDS = vo
    const double an = NA0 + vi * (NA1 + vi * NA2), dan = NA1 + 2.0 * NA2 * vi;
    const double tn = NT0 + vi * (NT1 + vi * NT2), dtn = NT1 + 2.0 * NT2 * vi;
    double ng, nd;
    const double in = mosfet(std::max(an, ALPHA_MIN), an > ALPHA_MIN ? dan : 0.0, tn, dtn, vi, vo, N_LAMBDA, ng, nd);
    // PMOS: vGS = s = vi - vdd < 0, the polynomials in s; magnitudes g = -s, d = vdd - vo
    const double s = vi - vdd;
    const double ap = PA0 + s * (PA1 + s * (PA2 + s * PA3)), dap = -(PA1 + s * (2.0 * PA2 + 3.0 * PA3 * s));
    const double tp = PT0 + s * (PT1 + s * PT2), dtp = -(PT1 + 2.0 * PT2 * s);
    double pg, pd;
    const double ip = mosfet(std::max(ap, ALPHA_MIN), ap > ALPHA_MIN ? dap : 0.0, tp, dtp, -s, vdd - vo, P_LAMBDA, pg, pd);
    dvi = -pg - ng;
    dvo = -pd - nd;
    return ip - in;
  }

  INLINE double WaspFilter::ota(double vd, double vo, double ib, double& dvd, double& dvo) const
  {
    const double x = OTA_BETA * vd / (2.0 * VT), t = std::tanh(x);
    const double fM = OTA_ALPHA * ib * t, dfM = OTA_ALPHA * ib * (1.0 - t * t) * OTA_BETA / (2.0 * VT);
    const double L = PL1 + PL2 * ib + PL3 * vd + PL4 * ib * ib + PL5 * ib * vd, dL = PL3 + PL5 * ib;
    const double H = PH1 + PH2 * ib + PH3 * vd + PH4 * ib * ib + PH5 * ib * vd, dH = PH3 + PH5 * ib;
    // the rail terms' weights, below 1e-8 away from the rails (skipped there)
    double wL = 0.0, dwL = 0.0, wH = 0.0, dwH = 0.0;
    if (vo < DV_L + 0.6)
    {
      const double tl = std::tanh(GAMMA_L * (vo - DV_L));
      wL = 0.5 * (1.0 - tl); dwL = -0.5 * GAMMA_L * (1.0 - tl * tl);
    }
    if (vo > dvH - 1.2)
    {
      const double th = std::tanh(GAMMA_H * (vo - dvH));
      wH = 0.5 * (1.0 + th); dwH = 0.5 * GAMMA_H * (1.0 - th * th);
    }
    dvd = dfM * (1.0 - wL - wH) + dL * wL + dH * wH;
    dvo = (L - fM) * dwL + (H - fM) * dwH;
    return fM + (L - fM) * wL + (H - fM) * wH;
  }

  INLINE double WaspFilter::prot(double vn, double& g) const
  {
    using namespace wasp_detail;
    // negligible (below 1e-20 A) more than 0.4 V inside the rails
    g = 0.0;
    double i = 0.0, e;
    if (vn > vdd - 0.4) { const double x = limExp((vn - vdd) / VT, e); i += PROT_IS * (x - 1.0); g += PROT_IS * e / VT; }
    if (vn < 0.4)       { const double x = limExp(-vn / VT, e);        i -= PROT_IS * (x - 1.0); g += PROT_IS * e / VT; }
    return i;
  }

  INLINE void WaspFilter::solve(double vin, double ib)
  {
    using namespace wasp_detail;
    constexpr double G2 = 1.0 / R2, G3 = 1.0 / R3, G15 = 1.0 / R15;
    const double G13 = 1.0 / r13;
    const double nVtD = D_N * VT;

    // capacitor companions (open at DC)
    const double gr1 = dcSolve ? 0.0 : 1.0 / (R1 + h1);
    const double e1 = vin - vC1 - h1 * iC1;
    const double c2 = dcSolve ? 0.0 : gc2, c3 = dcSolve ? 0.0 : gc3, c4 = dcSolve ? 0.0 : gc4, c7 = dcSolve ? 0.0 : gc7;
    const double J2 = dcSolve ? 0.0 : gc2 * vC2 + iC2, J3 = dcSolve ? 0.0 : gc3 * vC3 + iC3;
    const double J4 = dcSolve ? 0.0 : gc4 * vC4 + iC4, J7 = dcSolve ? 0.0 : gc7 * vC7 + iC7;

    // predictor: the nodes' linear extrapolation from the last two samples, kept inside the rails (past them the
    // junctions' exponentials throw Newton off)
    double vLast[NN];
    for (int k = 0; k < NN; ++k) vLast[k] = v[k];
    if (!dcSolve)
      for (int k = 0; k < NN; ++k) { v[k] = std::clamp(2.0 * v[k] - vPrev[k], 0.0, vdd); vPrev[k] = vLast[k]; }

    // the KCL residuals at the nodes x (amps) and their Jacobian
    auto residual = [&](const double (&x)[NN], double (&F)[NN], double (&J)[NN][NN])
    {
      const double vS = x[0], vHP = x[1], vN3 = x[2], vBP = x[3], vN5 = x[4], vLP = x[5], vX = x[6], vB = x[7];

      double i1i, i1o, i3i, i3o, i5i, i5o;
      const double inv1 = inverter(vS, vHP, i1i, i1o);
      const double inv3 = inverter(vN3, vBP, i3i, i3o);
      const double inv5 = inverter(vN5, vLP, i5i, i5o);
      double o1d, o1o, o2d, o2o;
      const double ota1 = ota(-k1 * (vHP - vd1), vN3, ib, o1d, o1o);
      const double ota2 = ota(-k2 * (vBP - vd2), vN5, ib, o2d, o2o);
      double gpS, gpHP, gpN3, gpBP, gpN5, gpLP;
      const double pS = prot(vS, gpS), pHP = prot(vHP, gpHP), pN3 = prot(vN3, gpN3);
      const double pBP = prot(vBP, gpBP), pN5 = prot(vN5, gpN5), pLP = prot(vLP, gpLP);
      // D1 / D2, X -> S
      const double ud = std::clamp((vX - vS) / nVtD, -80.0, 80.0);
      const double ep = std::exp(ud), em = 1.0 / ep;
      const double id = D_IS * (ep - em), gdd = D_IS * (ep + em) / nVtD;

      const double i2 = c2 * (vLP - vS) - J2, i3 = c3 * (vN3 - vBP) - J3, i4 = c4 * (vN5 - vLP) - J4;

      for (auto& row : J) for (double& j : row) j = 0.0;
      F[0] = (e1 - vS) * gr1 + (vLP - vS) * G2 + i2 + (vHP - vS) * G3 + id + (vBP - vS) * gAS + (vB - vS) * gBS - pS;
      J[0][0] = -gr1 - G2 - c2 - G3 - gdd - gAS - gBS - gpS;
      J[0][1] = G3; J[0][3] = gAS; J[0][5] = G2 + c2; J[0][6] = gdd; J[0][7] = gBS;

      F[1] = inv1 - (vHP - vS) * G3 - (vHP - vd1) * gd1 - pHP;
      J[1][0] = i1i + G3; J[1][1] = i1o - G3 - gd1 - gpHP;

      F[2] = ota1 - i3 - pN3;
      J[2][1] = -k1 * o1d; J[2][2] = o1o - c3 - gpN3; J[2][3] = c3;

      F[3] = inv3 + i3 - (vBP - vd2) * gd2 - (vBP - vX) * G13 - (vBP - vB) * gAB - (vBP - vS) * gAS - pBP;
      J[3][2] = i3i + c3; J[3][3] = i3o - c3 - gd2 - G13 - gAB - gAS - gpBP; J[3][6] = G13; J[3][7] = gAB; J[3][0] = gAS;

      F[4] = ota2 - i4 - pN5;
      J[4][3] = -k2 * o2d; J[4][4] = o2o - c4 - gpN5; J[4][5] = c4;

      F[5] = inv5 + i4 - i2 - (vLP - vS) * G2 - pLP;
      J[5][4] = i5i + c4; J[5][5] = i5o - c4 - c2 - G2 - gpLP; J[5][0] = c2 + G2;

      F[6] = (vBP - vX) * G13 - (vX - vB) * G15 - id;
      J[6][3] = G13; J[6][6] = -G13 - G15 - gdd; J[6][7] = G15; J[6][0] = gdd;

      F[7] = (vX - vB) * G15 + (vBP - vB) * gAB + (vS - vB) * gBS - (c7 * vB - J7);
      J[7][6] = G15; J[7][7] = -G15 - gAB - gBS - c7; J[7][3] = gAB; J[7][0] = gBS;

      double f2 = 0.0;
      for (int k = 0; k < NN; ++k) f2 += F[k] * F[k];
      return f2;
    };

    // Damped Newton: the Newton step, scaled down as a whole so no node moves more than maxStep (keeping its
    // direction, so ||F|| falls along it), is taken when it reduces ||F|| enough (Armijo), else halved up to
    // LS_HALVINGS times. Undamped, the iteration cycles between two points near the OTA and inverter rail knees at high
    // drive. If it has not converged by RESTART_IT and the last sample is the better point, it starts again from there.
    const int maxIt = dcSolve ? 200 : MAX_IT;
    const double maxStep = dcSolve ? 0.2 : MAX_STEP * vdd, tol = dcSolve ? 1e-10 : 1e-6;
    double F[NN], J[NN][NN], Ft[NN], Jt[NN][NN], vt[NN];
    double f2 = residual(v, F, J);
    auto accept = [&](const double (&x)[NN], double x2)
    {
      std::copy(&x[0], &x[0] + NN, &v[0]);
      std::copy(&Ft[0], &Ft[0] + NN, &F[0]);
      std::copy(&Jt[0][0], &Jt[0][0] + NN * NN, &J[0][0]);
      f2 = x2;
    };
    converged = false;
    for (int it = 0; it < maxIt; ++it)
    {
      if (it == RESTART_IT && !dcSolve)
      {
        const double fl2 = residual(vLast, Ft, Jt);
        if (fl2 < f2) accept(vLast, fl2);
      }
      double dx[NN];
      if (dcSolve) solveLinear<NN>(J, F, dx);
      else solveSparse(J, F, dx);
      double mx = 0.0;
      for (int k = 0; k < NN; ++k) mx = std::max(mx, std::fabs(dx[k]));
      if (mx > maxStep)
      {
        for (int k = 0; k < NN; ++k) dx[k] *= maxStep / mx;
        mx = maxStep;
      }
      if (mx < tol)   // quadratic convergence: the error left after a step of tol is ~tol^2
      {
        for (int k = 0; k < NN; ++k) v[k] += dx[k];
        converged = true;
        break;
      }
      double lambda = 1.0, ft2 = 0.0;
      for (int h = 0; ; ++h, lambda *= 0.5)
      {
        for (int k = 0; k < NN; ++k) vt[k] = v[k] + lambda * dx[k];
        ft2 = residual(vt, Ft, Jt);
        if (ft2 <= (1.0 - 2.0 * LS_ALPHA * lambda) * f2 || ft2 < LS_FLOOR || h == LS_HALVINGS) break;
      }
      accept(vt, ft2);
    }

    if (dcSolve) return;
    // capacitor states
    const double i1n = (e1 - v[0]) * gr1;
    vC1 += h1 * (i1n + iC1); iC1 = i1n;
    vC2 = v[5] - v[0]; iC2 = gc2 * vC2 - J2;
    vC3 = v[2] - v[3]; iC3 = gc3 * vC3 - J3;
    vC4 = v[4] - v[5]; iC4 = gc4 * vC4 - J4;
    vC7 = v[7];        iC7 = gc7 * vC7 - J7;
  }

  INLINE double WaspFilter::getSample(double in)
  {
    double fc = cutoff;
    if (filterFmDepth_ > 0.0)
    {
      double acAlpha = 2.0 * PI * acCouplingFreq_ / sampleRate;
      acCouplingState_ += acAlpha * (in - acCouplingState_);
      fc *= std::clamp(1.0 + filterFmDepth_ * filterFmScale_ * (in - acCouplingState_), 0.5, 1.5);
    }

    solve(in * driveFactor * inputVolts, otaIabc(fc));

    // the three outputs share IC1's inversion (each integrator's OTA divider inverts again), so LP -> BP -> HP keep one
    // sign; the output is turned back upright
    const double lp = (v[5] - rest[0]) / gainLp, bp = (v[3] - rest[1]) / gainBp, hp = (v[1] - rest[2]) / gainHp;
    const double wl = std::max(0.0, 1.0 - 2.0 * mode), wh = std::max(0.0, 2.0 * mode - 1.0), wb = 1.0 - wl - wh;
    double out = -(wl * lp + wb * bp + wh * hp);
    // C6: the clipping's DC (asymmetric, more so with Saturation Bias: up to ~0.2 of the output RMS at +16 dB, and it
    // moves with the cutoff) passes it almost whole, as in the circuit; Open303's post-filter highpass (24 Hz, before
    // the VCA) takes it out. A gated, env-swept sequence shows no onset thump from it (sub-15 Hz content the same at
    // Bias 0 and 1, and the same share as the TeeBee's).
    out -= outDc;
    outDc += outDcCoef * out;
    return out / driveMakeup;
  }

} // end namespace dfl

#endif // dfl_WaspFilter_h
