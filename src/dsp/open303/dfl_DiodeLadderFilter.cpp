// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Lowenfels
// Public interface modeled on rosic::TeeBeeFilter (Open303, MIT, Copyright (c) 2009 Robin Schmidt).

#include "dfl_DiodeLadderFilter.h"
using namespace dfl;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

DiodeLadderFilter::DiodeLadderFilter()
{
  cutoff              =  1000.0;
  driveFactor         =     1.0;
  driveMakeup         =     1.0;
  satBiasKnob         = SAT_BIAS_DEFAULT;
  driveTrim[0] = driveTrim[1] = 1.0;
  satBias             =     0.0;
  satBiasShape        =     0.0;
  satBiasNorm         =     1.0;
  drive               =     0.0;
  passbandCompensation =    0.0;   // no bass comp
  resonance           =     0.0;
  sampleRate          = 44100.0;
  K                   =     0.0;

  // Filter FM (Devilfish mod)
  filterFmDepth       =     0.0;  // Default: off
  acCouplingState     =     0.0;  // AC coupling HPF state
  feedbackHpFreq      =   FEEDBACK_DEFAULT;

  // Initialize coefficients
  alpha = 0.0;
  G1 = G2 = G3 = G4 = 0.0;
  beta1 = beta2 = beta3 = beta4 = 0.0;
  delta1 = delta2 = delta3 = 0.0;
  gamma1 = gamma2 = gamma3 = 0.0;
  epsilon1 = epsilon2 = epsilon3 = 0.0;
  SG1 = SG2 = SG3 = SG4 = 0.0;
  GAMMA = 0.0;

  calculateCoefficients();
  reset();
}

DiodeLadderFilter::~DiodeLadderFilter()
{
}

//-------------------------------------------------------------------------------------------------
// parameter settings:

void DiodeLadderFilter::setSampleRate(double newSampleRate)
{
  if( newSampleRate > 0.0 )
    sampleRate = newSampleRate;
  calculateCoefficients();
}

void DiodeLadderFilter::setInputDrive(double newDrive)
{
  // Stable range: verified by stability_sweep up to +16 dB (impulse tails below -60 dB at every
  // cutoff / feedback-HP / resonance; the drive scales only the input, so the loop gain does not depend on it).
  drive = std::clamp(newDrive, -12.0, 16.0);
  driveFactor = dB2amp(drive > 0.0 ? drive * (1.0 + SAT_DEPTH_EXTRA) : drive);
  // Level makeup so the drive knob is not a volume control: full below 0 dB (the shaper is nearly
  // linear there), partial above (the shaper compresses, so the drive may add a little level with
  // the grit). Continuous at 0 dB. See DRIVE_MAKEUP_EXP_*.
  const double makeupExp = (driveFactor < 1.0) ? DRIVE_MAKEUP_EXP_CLEAN : DRIVE_MAKEUP_EXP_SAT;
  driveMakeup = 1.0 / pow(driveFactor, makeupExp);
  updateSatBias();
  calculateCoefficients();
}


void DiodeLadderFilter::setSaturationBias(double newBias)
{
  satBiasKnob = std::clamp(newBias, 0.0, 1.0);
  updateSatBias();
  calculateCoefficients();
}

void DiodeLadderFilter::updateSatBias()
{
  satBias = satBiasKnob * SAT_BIAS_MAX * std::clamp(drive / SAT_BIAS_TOP_DB, 0.0, 1.0);
  satBiasShape = shape(satBias);
  const double t = std::tanh(satBias);
  satBiasNorm = 1.0 / (1.0 - t * t);
}

//-------------------------------------------------------------------------------------------------
// others:

void DiodeLadderFilter::reset()
{
  z1 = 0.0;
  z2 = 0.0;
  z3 = 0.0;
  z4 = 0.0;
  loadState = 0.0;
  loadW = 0.0;
  outCouplingState = 0.0;
  acCouplingState = 0.0;  // Reset AC coupling filter
  for (double& st : netState) st = 0.0;
}
