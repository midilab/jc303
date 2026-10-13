// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Lowenfels

#include "dfl_WaspFilter.h"
using namespace dfl;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

WaspFilter::WaspFilter(Revision newRevision)
  : revision(newRevision)
{
  setSampleRate(44100.0);
  setRevision(newRevision);
}

WaspFilter::~WaspFilter()
{
}

//-------------------------------------------------------------------------------------------------
// parameter settings:

void WaspFilter::setSampleRate(double newSampleRate)
{
  if( newSampleRate > 0.0 )
    sampleRate = newSampleRate;
  T = 1.0 / sampleRate;
  h1 = T / (2.0 * C1);
  gc2 = 2.0 * C2 / T;
  gc3 = 2.0 * C3 / T;
  gc4 = 2.0 * C4 / T;
  gc7 = 2.0 * C7 / T;
  // output coupling: C6 10u into a 100k load (~0.16 Hz)
  outDcCoef = 1.0 - std::exp(-2.0 * PI * (1.0 / (2.0 * PI * 10e-6 * 100e3)) * T);
}

void WaspFilter::setInputDrive(double newDrive)
{
  drive = newDrive;
  driveFactor = dB2amp(newDrive);
  driveMakeup = std::pow(driveFactor, MAKEUP_EXP[(int)revision]);
}

void WaspFilter::setRevision(Revision newRevision)
{
  revision = newRevision;
  const int r = (int)revision;
  vddNom = VDD[r];
  r13 = R13[r];

  // OTA input dividers: the previous output through R5 (R9) and R6 (R10) into R7 || R8 (R11 || R12)
  gd1 = 1.0 / (R5 + R6 + R7 * R8 / (R7 + R8));
  gd2 = 1.0 / (R9 + R10 + R11 * R12 / (R11 + R12));
  k1 = R6 * gd1;
  k2 = R10 * gd2;
  // passbands: LP and HP at R2 / R1 = R3 / R1 of the input, BP's peak at the RES-0 Q (~1)
  gainLp = gainBp = gainHp = INPUT_VOLTS * R3 / R1;
  gainBp /= dB2amp(BP_TRIM_DB[r]);
  gainHp /= dB2amp(HP_TRIM_DB[r]);

  setInputDrive(drive);
  updateSupply();
  updatePot();
  reset();
}

void WaspFilter::updateSupply()
{
  vdd = vddNom * (1.0 - RAIL_SAG * (1.0 - rail_));
  vd1 = vdd * (0.5 + BIAS_SHIFT * satBias_);
  vd2 = vdd * R12 / (R11 + R12);
  dvH = vdd - DV_H_DROP;
}

void WaspFilter::updatePot()
{
  resPot = std::clamp(-std::log(1.0 - std::clamp(resonance, 0.0, 1.0) * RES_SKEW_NORM) / 3.0, 0.0, 1.0);
  // the wiper W: BP through R14 and rho of the pot, the C7 node through the rest, IC1's input through R4; eliminated
  // as a star (its mesh conductances)
  const double ga = 1.0 / (R14 + resPot * R_RES);
  const double gb = 1.0 / std::max((1.0 - resPot) * R_RES, 1.0);
  const double g4 = 1.0 / R4, gs = ga + gb + g4;
  gAB = ga * gb / gs;
  gAS = ga * g4 / gs;
  gBS = gb * g4 / gs;
}

//-------------------------------------------------------------------------------------------------
// others:

void WaspFilter::reset()
{
  // DC operating point: capacitors open; the gates start near the inverters' switching point
  for (int k = 0; k < NN; ++k) v[k] = 0.5 * vdd;
  v[0] = v[2] = v[4] = 0.45 * vdd;
  dcSolve = true;
  solve(0.0, otaIabc(cutoff));
  dcSolve = false;

  // the capacitors charged to it, carrying no current
  vC1 = -v[0]; iC1 = 0.0;
  vC2 = v[5] - v[0]; iC2 = 0.0;
  vC3 = v[2] - v[3]; iC3 = 0.0;
  vC4 = v[4] - v[5]; iC4 = 0.0;
  vC7 = v[7]; iC7 = 0.0;
  rest[0] = v[5]; rest[1] = v[3]; rest[2] = v[1];
  for (int k = 0; k < NN; ++k) vPrev[k] = v[k];
  outDc = 0.0;
  acCouplingState_ = 0.0;
}
