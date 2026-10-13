// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Lowenfels

#include "dfl_MS20Filter.h"
using namespace dfl;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

MS20Filter::MS20Filter()
{
  updateDiodes();
  updateCutoffTable();
  setSampleRate(44100.0);
  reset();
}

MS20Filter::~MS20Filter()
{
}

//-------------------------------------------------------------------------------------------------
// parameter settings:

void MS20Filter::setSampleRate(double newSampleRate)
{
  if( newSampleRate > 0.0 )
    sampleRate = newSampleRate;
  T = 1.0 / sampleRate;
  g29 = 2.0 * C29 / T;
  g28 = 2.0 * C28 / T;
  g30 = 2.0 * C30 / T;
  h31 = T / (2.0 * C31);
  gCd = 2.0 * (2.0 * D_CJ) / T;
  dcCoef = 1.0 - std::exp(-2.0 * PI * 1.0 * T);   // C32 / C7 into the pot network: ~1 Hz
  updatePot();
}

void MS20Filter::setInputDrive(double newDrive)
{
  drive = newDrive;
  driveFactor = dB2amp(newDrive);
  driveMakeup = std::pow(driveFactor, K35_MAKEUP_EXP);
  updateSmallSignalGains();   // the Korg35 limiter normalisation follows the drive
}

void MS20Filter::setSharpness(double amount)
{
  sharpness_ = std::clamp(amount, 0.0, 1.0);
  vtj = VT * std::pow(2.0, 1.0 - 2.0 * sharpness_);
  updateCutoffTable();
}

void MS20Filter::updatePot()
{
  peakPot = std::clamp(-std::log(1.0 - std::clamp(resonance, 0.0, 1.0) * RES_SKEW_NORM) / 3.0, 0.0, 1.0);
  const double rTop = (1.0 - peakPot) * VR22 + 1.0;   // linear (B) pot, 10k
  const double rBot = peakPot * VR22 + 1.0;

  // E -(top || R127)- F -(R113 || C30)- G -(R111)- ground / BP injection; F -(bottom)- ground
  fbA = 1.0 / rTop + 1.0 / R127;
  fbB = 1.0 / rBot;
  fbC = 1.0 / R113 + g30;
  fbD = 1.0 / R111;
  fbS = fbA + fbB + fbC;
  fbRth = 1.0 / (fbC + fbD - fbC * fbC / fbS);
}

void MS20Filter::updateDiodes()
{
  const double scale = std::pow(10.0, CLIP_DECADES * (0.5 - clipThreshold_));
  diodeIb = D_IS * scale;
  diodeIa = diodeIb * std::pow(10.0, BITE_MAX_DECADES * (bite_ + satBias_));
  updateSmallSignalGains();
}

void MS20Filter::updateSmallSignalGains()
{
  // diode pair conductance at rest. LP passband: Z follows X at DC; x JFET follower x (1 + (R117 || diodes) / R116).
  // Bite / Clip / the saturation bias change the diodes' resting conductance; the normalisation follows only part of
  // it, less as the drive pushes the stage onto the knee (see K35_LIMITER_NORM_SLOPE)
  const double alpha = std::clamp(1.0 - K35_LIMITER_NORM_SLOPE * drive, 0.0, 1.0);
  const double gd = std::pow(2.0 * D_IS / (D_N * VT), 1.0 - alpha) * std::pow((diodeIa + diodeIb) / (D_N * VT), alpha);
  const double r117 = 1.0 / (1.0 / R117 + gd);
  gainLp = K35_PAD_LP * JFET_BUF_GAIN * (1.0 + r117 / R116);
  gainBp = gainLp / dB2amp(K35_BP_TRIM_DB);
  gainHp = gainLp * K35_PAD_HP / K35_PAD_LP / dB2amp(K35_HP_TRIM_DB);
}

//-------------------------------------------------------------------------------------------------
// Korg35 cutoff: DC operating point and small-signal corner

double MS20Filter::k35CornerAt(double vb, double* x)
{
  const double isq1 = Q_IS * (1.0 / (1.0 + Q_BF) + 1.0 / Q_BR);
  const double isf = Q_IS / Q_BF, isr = Q_IS / Q_BR;
  // x: vA, vBn, vY, vZ (in: start, out: operating point); X grounded, capacitors open
  x[1] = vb;
  double Jm[4][4];
  for (int it = 0; it < 200; ++it)
  {
    const double eQ1 = std::exp(std::clamp((x[0] - x[1]) / vtj, -80.0, 80.0));
    const double eBE2 = std::exp(std::clamp((x[0] - x[2]) / vtj, -80.0, 80.0));
    const double eBC2 = std::exp(std::clamp(x[0] / vtj, -80.0, 80.0));
    const double eBE3 = std::exp(std::clamp((x[0] - x[3]) / vtj, -80.0, 80.0));
    const double eBC3 = eBE2;
    const double iQ1 = isq1 * (eQ1 - 1.0);
    const double iB2 = isf * (eBE2 - 1.0) + isr * (eBC2 - 1.0);
    const double iE2 = Q_IS * (eBE2 - eBC2) + isf * (eBE2 - 1.0);
    const double iB3 = isf * (eBE3 - 1.0) + isr * (eBC3 - 1.0);
    const double iE3 = Q_IS * (eBE3 - eBC3) + isf * (eBE3 - 1.0);
    const double iC3 = Q_IS * (eBE3 - eBC3) - isr * (eBC3 - 1.0);
    double F[4] = { (VCC - x[0]) / R3 - iQ1 - iB2 - iB3, iQ1 - (x[1] - vb) / RB, iE2 - iC3, iE3 };

    const double q1 = isq1 * eQ1 / vtj;
    const double be2 = eBE2 / vtj, bc2 = eBC2 / vtj, be3 = eBE3 / vtj, bc3 = be2;
    const double dIB2dA = isf * be2 + isr * bc2, dIB2dY = -isf * be2;
    const double dIE2dA = Q_IS * (be2 - bc2) + isf * be2, dIE2dY = -Q_IS * be2 - isf * be2;
    const double dIB3dA = isf * be3 + isr * bc3, dIB3dY = -isr * bc3, dIB3dZ = -isf * be3;
    const double dIE3dA = Q_IS * (be3 - bc3) + isf * be3, dIE3dY = Q_IS * bc3, dIE3dZ = -Q_IS * be3 - isf * be3;
    const double dIC3dA = Q_IS * (be3 - bc3) - isr * bc3, dIC3dY = Q_IS * bc3 + isr * bc3, dIC3dZ = -Q_IS * be3;
    double J0[4][4] = {
      { -1.0 / R3 - q1 - dIB2dA - dIB3dA, q1, -dIB2dY - dIB3dY, -dIB3dZ },
      { q1, -q1 - 1.0 / RB, 0.0, 0.0 },
      { dIE2dA - dIC3dA, 0.0, dIE2dY - dIC3dY, -dIC3dZ },
      { dIE3dA, 0.0, dIE3dY, dIE3dZ } };
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) Jm[r][c] = J0[r][c];

    double A[4][4], b[4];
    for (int r = 0; r < 4; ++r) { for (int c = 0; c < 4; ++c) A[r][c] = J0[r][c]; b[r] = -F[r]; }
    for (int c = 0; c < 4; ++c)
    {
      int p = c;
      for (int r = c + 1; r < 4; ++r) if (std::fabs(A[r][c]) > std::fabs(A[p][c])) p = r;
      if (p != c) { for (int k = 0; k < 4; ++k) std::swap(A[c][k], A[p][k]); std::swap(b[c], b[p]); }
      for (int r = c + 1; r < 4; ++r)
      {
        const double m = A[r][c] / A[c][c];
        for (int k = c; k < 4; ++k) A[r][k] -= m * A[c][k];
        b[r] -= m * b[c];
      }
    }
    double dx[4], mx = 0.0;
    for (int r = 3; r >= 0; --r)
    {
      double s = b[r];
      for (int k = r + 1; k < 4; ++k) s -= A[r][k] * dx[k];
      dx[r] = std::clamp(s / A[r][r], -0.05, 0.05);
      mx = std::max(mx, std::fabs(dx[r]));
    }
    for (int k = 0; k < 4; ++k) x[k] += dx[k];
    if (mx < 1e-13) break;
  }

  // Schur complement onto (Y, Z): eliminate A and Bn (no capacitors there)
  const double a00 = Jm[0][0], a01 = Jm[0][1], a10 = Jm[1][0], a11 = Jm[1][1];
  const double det2 = a00 * a11 - a01 * a10;
  const double i00 = a11 / det2, i01 = -a01 / det2, i10 = -a10 / det2, i11 = a00 / det2;
  double G[2][2];
  for (int r = 0; r < 2; ++r)
    for (int c = 0; c < 2; ++c)
    {
      const double ur0 = Jm[2 + r][0], ur1 = Jm[2 + r][1];
      const double v0c = Jm[0][2 + c], v1c = Jm[1][2 + c];
      G[r][c] = Jm[2 + r][2 + c] - (ur0 * (i00 * v0c + i01 * v1c) + ur1 * (i10 * v0c + i11 * v1c));
    }
  const double detG = G[0][0] * G[1][1] - G[0][1] * G[1][0];
  return std::sqrt(std::max(detG, 1e-30) / (C29 * C28));
}

void MS20Filter::updateCutoffTable()
{
  // scan the control voltage, then invert to a uniform log-frequency grid
  constexpr int NS = 300;
  double lf[NS], vbs[NS];
  const double vbLo = -0.6, vbHi = 0.6;
  double x[4] = { 0.55, vbLo, 0.0, 0.0 };
  for (int i = 0; i < NS; ++i)
  {
    vbs[i] = vbLo + (vbHi - vbLo) * i / (NS - 1);
    lf[i] = std::log(k35CornerAt(vbs[i], x) / (2.0 * PI));   // warm start from the previous point
  }
  ctLogF0 = std::log(5.0);
  ctDLogF = (std::log(40000.0) - ctLogF0) / (CT_N - 1);
  int j = 0;
  for (int i = 0; i < CT_N; ++i)
  {
    const double target = ctLogF0 + ctDLogF * i;
    while (j < NS - 2 && lf[j + 1] < target) ++j;
    const double f = std::clamp((target - lf[j]) / (lf[j + 1] - lf[j]), -4.0, 4.0);
    ctVb[i] = vbs[j] + f * (vbs[j + 1] - vbs[j]);
  }
}

//-------------------------------------------------------------------------------------------------
// others:

void MS20Filter::reset()
{
  // operating point at the current cutoff, capacitors at rest
  double x[4] = { 0.55, 0.0, 0.0, 0.0 };
  k35CornerAt(k35ControlFor(cutoff), x);
  vA = x[0]; vBn = x[1]; vY = x[2]; vZ = x[3]; vZ0 = x[3];
  vN = vI = vO = vIp = 0.0;
  opPrev = 0.0;
  vCd = iCd = 0.0;
  v29 = vY; i29 = 0.0;   // G sits at 0 V at rest
  v28 = vZ; i28 = 0.0;
  v30 = 0.0; i30 = 0.0;
  vC31 = 0.0; iR116 = 0.0;

  dcOut = 0.0;
  acCouplingState_ = 0.0;
}
