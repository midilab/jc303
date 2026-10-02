// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Loewenfels
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
  drive               =     0.0;
  passbandCompensation =    0.0;   // HP/BP morph: pure HP/BP by default
  resonance           =     0.0;
  sampleRate          = 44100.0;
  octaveMode          =    true;  // default to TB-303 style
  responseMode        = RESPONSE_LP;
  K                   =     0.0;
  Kcomp               =     0.0;
  plainTrimLP         =     1.0;
  plainTrimBPHP       =     1.0;

  // Filter FM (Devilfish mod)
  filterFmDepth       =     0.0;  // Default: off
  acCouplingState     =     0.0;  // AC coupling HPF state
  feedbackHpFreq      =   115.0;  // Feedback-path HPF corner (fit to the real circuit)
  feedbackHpAlpha     =     0.0;
  feedbackHpGain      =     1.0;
  feedbackHpState     =     0.0;
  updateFeedbackHp();

  // Initialize coefficients
  alpha = 0.0;
  alpha2 = 0.0;
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
  updateFeedbackHp();
  calculateCoefficients();
}

void DiodeLadderFilter::setInputDrive(double newDrive)
{
  // Stable range: the zero-delay loop solve assumes unit input gain, so very high drive
  // (>= 12 dB) can tip the near-critical octave loop into oscillation at high cutoff.
  drive = std::clamp(newDrive, -12.0, 9.0);
  driveFactor = dB2amp(drive);
  // Small-signal makeup so the drive knob is (roughly) level-neutral instead of
  // doubling as a volume boost. Full 1/driveFactor makeup over-corrects at real
  // signal levels (the oscillator hits the filter near unity, deep in the tanh's
  // compressing region), turning drive into a level cut. A partial exponent is
  // the best static compromise across resonance settings - see DRIVE_MAKEUP_EXP.
  driveMakeup = 1.0 / pow(driveFactor, DRIVE_MAKEUP_EXP);
  calculateCoefficients();  // octave resonance ceiling is scaled by driveMakeup
}

//-------------------------------------------------------------------------------------------------
// others:

void DiodeLadderFilter::reset()
{
  z1 = 0.0;
  z2 = 0.0;
  z3 = 0.0;
  z4 = 0.0;
  acCouplingState = 0.0;  // Reset AC coupling filter
  feedbackHpState = 0.0;
}
