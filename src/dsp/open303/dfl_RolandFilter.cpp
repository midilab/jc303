// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Lowenfels

#include "dfl_RolandFilter.h"
using namespace dfl;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

RolandFilter::RolandFilter()
{
  setSampleRate(44100.0);
  updateCharacter();
  setInputDrive(drive);
  updateGains();
  reset();
}

RolandFilter::~RolandFilter()
{
}

//-------------------------------------------------------------------------------------------------
// parameter settings:

void RolandFilter::setSampleRate(double newSampleRate)
{
  if( newSampleRate > 0.0 )
    sampleRate = newSampleRate;
  T = 1.0 / sampleRate;
  gC = 2.0 * C_ST / T;
  // VCA input coupling: C59 1u into ~112k (~1.4 Hz)
  const double fDc = 1.0 / (2.0 * PI * 1e-6 * 112e3);
  vcaDcCoef = 1.0 - std::exp(-2.0 * PI * fDc * T);
}

void RolandFilter::setInputDrive(double newDrive)
{
  drive = newDrive;
  driveFactor = dB2amp(newDrive);
  driveMakeup = std::pow(driveFactor, MAKEUP_EXP);
}

void RolandFilter::updatePot()
{
  resPot = std::clamp(-std::log(1.0 - std::clamp(resonance, 0.0, 1.0) * RES_SKEW_NORM) / 3.0, 0.0, 1.0);
  // the RES CV sets the BA662's Iabc through TR1 (its Vbe a small offset of the CV's range)
  const double clipScale = std::pow(4.0, 1.0 - 2.0 * clipThreshold_);
  resIabc = RES_IABC_MAX / clipScale * std::max(0.0, resPot - RES_OFFSET) / (1.0 - RES_OFFSET);
}

void RolandFilter::updateCharacter()
{
  // Bite: stage mirror mismatch (alternating, unequal), renormalised to the matched stage's slope at its operating
  // point t0 = inOffT - h / (1 + h), h = m / 2 (the saturation bias offsets stage 1's pair, see setSaturationBias)
  static constexpr double pattern[4] = { 1.0, -0.7, 0.5, -0.85 };
  inOff[0] = satBias_;
  inOffT[0] = std::tanh(satBias_);
  for (int k = 0; k < 4; ++k)
  {
    mirror[k] = BITE_MIRROR * bite_ * pattern[k];
    const double h = 0.5 * mirror[k], t0 = inOffT[k] - h / (1.0 + h);
    gmNorm[k] = 1.0 / ((1.0 - t0 * t0) * (1.0 + h));
  }

  // Clip: the resonance OTA's input scale
  const double clipScale = std::pow(4.0, 1.0 - 2.0 * clipThreshold_);
  resKn = RES_KN * clipScale;
  resKp = RES_KP * clipScale;

  // Warmth: VCA drive
  vcaK = VCA_K * std::pow(VCA_DRIVE_MAX, warmth_);
  updatePot();
}

void RolandFilter::updateGains()
{
  // LP: Out4 = +(R_FB / R_IN) vin at DC (four inverting stages); BP peaks at 1/4 of the stage-1 input at the cutoff;
  // HP's passband is the stage-1 input
  const double g = R_FB / R_IN * INPUT_VOLTS;
  gainLp = g;
  gainBp = 0.25 * g / dB2amp(BP_TRIM_DB);
  gainHp = g / dB2amp(HP_TRIM_DB);
}

//-------------------------------------------------------------------------------------------------
// others:

void RolandFilter::reset()
{
  for (int k = 0; k < 4; ++k) { vc[k] = -VSH; ic[k] = 0.0; y[k] = 0.0; }
  vcaDc = 0.0;

  // DC operating point (Bite's offsets): capacitors open; then the VCA's coupling capacitor charged to it, as the
  // circuit sits at power-up
  const double gC0 = gC;
  gC = 0.0;
  for (int k = 0; k < 3; ++k)
    getSampleCircuit(0.0, cutoff);
  gC = gC0;
  for (int k = 0; k < 4; ++k) ic[k] = 0.0;
  vcaDc = preVca;
  acCouplingState_ = 0.0;
}
