// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Lowenfels

#include "dfl_ARP4075Filter.h"
using namespace dfl;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

ARP4075Filter::ARP4075Filter()
{
  setSampleRate(44100.0);
  updateRails();   // also the pot (the limiter's input resistance)
  setInputDrive(0.0);

  // passbands: LP's DC gain from the mixer pot (the limiter's slope into stage 1, then unity per stage); BP peaks at
  // 1/4 of it at the cutoff; HP's passband is it. At the circuit's supplies: a sagged rail (setRail) is quieter
  const double din = ALPHA * iIn / (2.0 * VT) * R35 / (R_MIX + R35) * kLoad;   // kLoad at RES 0 (updatePot)
  gainLp = INPUT_VOLTS * din * 0.5 * (R_B + R_B_FB) / K_SUM;
  gainBp = 0.25 * gainLp / dB2amp(BP_TRIM_DB);
  gainHp = gainLp / dB2amp(HP_TRIM_DB);
  reset();
}

ARP4075Filter::~ARP4075Filter()
{
}

//-------------------------------------------------------------------------------------------------
// parameter settings:

void ARP4075Filter::setSampleRate(double newSampleRate)
{
  if( newSampleRate > 0.0 )
    sampleRate = newSampleRate;
  T = 1.0 / sampleRate;
  gC = 2.0 * C_ST / T;
  g37 = 2.0 * C37 / T;
  h24 = T / (2.0 * C24);
  // C6 into the RES pot (100k) and the next stage: ~0.16 Hz
  c6Coef = 1.0 - std::exp(-T / (C6 * R_RES));
}

void ARP4075Filter::setInputDrive(double newDrive)
{
  drive = newDrive;
  driveFactor = dB2amp(newDrive);
  driveMakeup = std::pow(driveFactor, MAKEUP_EXP);
}

void ARP4075Filter::updatePot()
{
  resPot = std::clamp(-std::log(1.0 - std::clamp(resonance, 0.0, 1.0) * RES_SKEW_NORM) / 3.0, 0.0, 1.0);
  // R144 linear, wiper from the ground end
  wiper = resPot;
  rTh = resPot * (1.0 - resPot) * R_RES;
  // the limiter's bases: pin 1 sees R35 || R123, pin 2 R36 || (R142 + the wiper's Thevenin)
  const double r1 = R35 * R_MIX / (R35 + R_MIX), r2 = R36 * (R142 + rTh) / (R36 + R142 + rTh);
  kLoad = rD / (rD + r1 + r2);
}

void ARP4075Filter::updateRails()
{
  // both supplies sag by the same fraction. -14 V: the reference (Q16, half of it) follows, the LM3900s' swing about
  // it shrinks, and Q14's collector (saturating near ground) has that much less headroom above the stage emitters
  const double sag = 1.0 - RAIL_SAG * (1.0 - rail_), vneg = V_NEG * sag;
  vHi = 0.5 * vneg - SWING_HI_DROP;
  vLo = -(0.5 * vneg - SWING_LO_DROP);
  q14Head = VQ14_HEAD - 0.5 * (V_NEG - vneg);
  // +15 V: the limiter's tail through R24, and with it the pair's differential input resistance (re at half its tail,
  // in series with the pins' source resistances: the pot's loading)
  iIn = (V_POS * sag - VBE) / R24;
  rD = 2.0 * (BETA + 1.0) * VT / (0.5 * iIn);
  updatePot();
}

//-------------------------------------------------------------------------------------------------
// others:

void ARP4075Filter::reset()
{
  for (int k = 0; k < 4; ++k) { y[k] = 0.0; ic[k] = 0.0; }
  v37 = i37 = v24 = i24 = 0.0;
  dc6 = outDc = 0.0;
  for (double& r : rest) r = 0.0;

  // DC operating point (the limiter's base-resistor imbalance, the bias): capacitors open
  dcSolve = true;
  getSample(0.0);
  dcSolve = false;
  for (int k = 0; k < 4; ++k) ic[k] = 0.0;
  dc6 = y[3];
  const double i9 = 0.5 * (iIn - limiterRest()), i10 = 0.5 * (iIn + limiterRest());
  rest[0] = (R_B * i10 - R_B_FB * i9) / K_SUM;
  for (int k = 0; k < 4; ++k) { rest[k + 1] = y[k]; yPrev[k] = y[k]; }
  acCouplingState_ = 0.0;
}
