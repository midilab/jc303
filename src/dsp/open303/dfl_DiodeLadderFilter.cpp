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
  passbandCompensation =    0.0;
  resonance           =     0.0;
  sampleRate          = 44100.0;
  octaveMode          =    true;  // default to TB-303 style
  responseMode        = RESPONSE_LP;
  K                   =     0.0;

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
  calculateCoefficients();
}

void DiodeLadderFilter::setInputDrive(double newDrive)
{
  drive = newDrive;
  driveFactor = dB2amp(newDrive);
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
}
