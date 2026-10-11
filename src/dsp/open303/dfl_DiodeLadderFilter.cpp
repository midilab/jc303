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
  updateCharacter();
}

void DiodeLadderFilter::updateCharacter()
{
  clipS = std::pow(2.0, 2.0 * clip_ - 1.0);
  o12 = BITE_Q12 * bite_;
  o21 = BITE_Q21 * bite_;
  q12Rho = SHARP_RHO_MAX * std::max(0.0, 2.0 * sharp_ - 1.0);
  q12C = 1.0 + q12Rho;
  q21W = std::pow(WARM_MAX, warmth_);
  // Q21's operating offset: X0 + K tanh(X0) = h o21 (Newton; monotonic)
  {
    const double target = headroomBias * o21;
    double x = target / (1.0 + Q21_SELF_CENTRE);
    for (int i = 0; i < 50; ++i)
    {
      const double t = std::tanh(x), dx = (x + Q21_SELF_CENTRE * t - target) / (1.0 + Q21_SELF_CENTRE * (1.0 - t * t));
      x -= dx;
      if (std::fabs(dx) < 1e-15) break;
    }
    oq21 = x;
  }
  q21T0 = hiFi ? tanhRational(oq21) : shape(oq21);   // the quality's own tanh
  // Q12's rest point (with Saturation Bias, without the offset); the degenerated gain renormalised to the
  // undegenerated pair's rest slope (sech^2 of the bias)
  double dt;
  const double XB = q12C * satBias;
  const double tb0 = hiFi ? tanhRational(satBias) : shape(satBias);
  q12TB = (q12Rho > 0.0) ? degenT(XB, q12Rho, dt) : (hiFi ? tanhRational(XB) : shape(XB));
  if (q12Rho <= 0.0) dt = 1.0 - q12TB * q12TB;
  q12Nrm = (1.0 - tb0 * tb0) / (q12C * dt);
  q12Plain = (bite_ == 0.0 && clip_ == 0.5 && q12Rho == 0.0);
  // the rungs' rest current: at DC every rung carries Q12's output (its feedback is AC-coupled), T(x) = un_dc, so each
  // rung's tanh sits at hs un_dc (only Bite's offset makes un_dc non-zero)
  double dq;
  restRung = (o12 != 0.0) ? headroomBias / clipS * q12(0.0, dq, hiFi) : 0.0;
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
  for (int k = 0; k < 4; ++k) yPrev[k] = yPrev2[k] = shadowZ[k] = 0.0;
  shadowLoadState = 0.0;
  for (double& e : envE) e = 0.0;
  acCouplingState = 0.0;  // Reset AC coupling filter
  for (double& st : netState) st = 0.0;

  // Bite's Q12 offset pushes a DC current through the ladder: settle onto that DC operating point (as the circuit sits
  // at power-up, and as SPICE starts) instead of stepping into it. The DC solution does not depend on the cutoff or the
  // coupling poles, nor on the loop (the feedback is AC-coupled: it carries no DC), so run silence through a fast ladder
  // and a fast coupling network with the loop open (closed, near the threshold it would ring instead of settling), then
  // restore them.
  if (o12 != 0.0)
  {
    const double cut0 = cutoff;
    cutoff = 0.2 * sampleRate;
    calculateCoefficients();
    for (double& a : netA) a = 0.9;
    K = 0.0;
    for (int n = 0; n < 2000; ++n)
    {
      getSample(0.0);
      // the Q21 load lowpass and the coupling network's first section are slow (the coupling pole, not the cutoff):
      // pin them to their DC value, the ladder top's
      loadState = loadW = z4;
      netState[0] = z4;
    }
    for (int i = 1; i < NET_N; ++i) netState[i] = 0.0;   // after section 0's highpass: no DC
    cutoff = cut0;
    calculateCoefficients();
    acCouplingState = 0.0;
    for (double& e : envE) e = 0.0;
  }
}
