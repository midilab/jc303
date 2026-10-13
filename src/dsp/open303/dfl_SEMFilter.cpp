// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Lowenfels

#include "dfl_SEMFilter.h"
using namespace dfl;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

SEMFilter::SEMFilter()
{
  setSampleRate(44100.0);
  // passbands: HP and LP at R139 / R155 = R138 / R155 of the input; BP's peak at the RES-0 damping (Q ~1/3), lifted
  // by the trim
  gainLp = gainBp = gainHp = OUT_GAIN;
  gainBp /= dB2amp(BP_TRIM_DB);
  gainHp /= dB2amp(HP_TRIM_DB);
  updateSupply();
  updateBias();
  updatePot();
  reset();
}

SEMFilter::~SEMFilter()
{
}

//-------------------------------------------------------------------------------------------------
// parameter settings:

void SEMFilter::setSampleRate(double newSampleRate)
{
  if( newSampleRate > 0.0 )
    sampleRate = newSampleRate;
  T = 1.0 / sampleRate;
  h15 = T / (2.0 * C15);
  gc18 = 2.0 * C18 / T;
  gc20 = 2.0 * C20 / T;
  gc21 = 2.0 * C21 / T;
  gc22 = 2.0 * C22 / T;
}

void SEMFilter::setInputDrive(double newDrive)
{
  drive = newDrive;
  driveFactor = dB2amp(newDrive);
  driveMakeup = std::pow(driveFactor, MAKEUP_EXP);
}

void SEMFilter::updateSupply()
{
  vcc = VCC * (1.0 - RAIL_SAG * (1.0 - rail_));
  vSwing = vcc - OPAMP_SWING_DROP;
}

void SEMFilter::updateBias()
{
  off1 = 0.5 * BIAS_MAX_VT * satBias_;   // in 2 VT units
  off1T = std::tanh(off1);
}

void SEMFilter::updatePot()
{
  resPot = std::clamp(-std::log(1.0 - std::clamp(resonance, 0.0, 1.0) * RES_SKEW_NORM) / 3.0, 0.0, 1.0);
  // PS16: BP at the top, ground at the bottom; RES up moves the wiper down (less damping). The wiper W: BP through
  // the upper part, ground through the lower, the C22 node through R157; eliminated as a star (its mesh conductances)
  const double a = 1.0 - resPot;   // wiper position from the bottom
  const double gb = 1.0 / std::max((1.0 - a) * R_RES, 1.0);
  const double gg = 1.0 / std::max(a * R_RES, 1.0);
  const double gn = 1.0 / R157, gs = gb + gg + gn;
  gBN = gb * gn / gs;
  gNG = gg * gn / gs;
  gBG = gb * gg / gs;
}

//-------------------------------------------------------------------------------------------------
// others:

void SEMFilter::reset()
{
  // DC operating point: capacitors open, from the nodes at ground with the followers' gates a VGS below
  for (int k = 0; k < NN; ++k) v[k] = 0.0;
  v[1] = v[3] = -0.5;
  dcSolve = true;
  solve(0.0, otaIabc(cutoff));
  dcSolve = false;

  // the capacitors charged to it, carrying no current
  vC15 = -v[0]; iC15 = 0.0;
  vC18 = v[4] - v[0]; iC18 = 0.0;
  vC20 = v[1]; iC20 = 0.0;
  vC21 = v[3]; iC21 = 0.0;
  vC22 = v[7] - v[0]; iC22 = 0.0;
  vA12 = v[8];
  rest[0] = v[4]; rest[1] = v[2]; rest[2] = v[8];
  for (int k = 0; k < NN; ++k) vPrev[k] = v[k];
  acCouplingState_ = 0.0;
}
