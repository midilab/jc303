#include "rosic_TeeBeeFilter.h"
using namespace rosic;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

TeeBeeFilter::TeeBeeFilter()
{
  cutoff              =  1000.0;
  drive               =     0.0;
  driveFactor         =     1.0;
  resonanceRaw        =     0.0;
  resonanceSkewed     =     0.0;
  g                   =     1.0;
  sampleRate          = 44100.0;
  twoPiOverSampleRate = 2.0*PI/sampleRate;

  feedbackHighpass.setMode(OnePoleFilter::HIGHPASS);
  feedbackHighpass.setCutoff(150.0);

  //setMode(LP_18);
  setMode(TB_303);
  calculateCoefficientsExact();
  reset();
}

TeeBeeFilter::~TeeBeeFilter()
{

}

//-------------------------------------------------------------------------------------------------
// parameter settings:

void TeeBeeFilter::setSampleRate(double newSampleRate)
{
  if( newSampleRate > 0.0 )
    sampleRate = newSampleRate;
  twoPiOverSampleRate = 2.0*PI/sampleRate;
  feedbackHighpass.setSampleRate(newSampleRate);
  calculateCoefficientsExact();
}

void TeeBeeFilter::setDrive(double newDrive)
{
  drive       = newDrive;
  driveFactor = dB2amp(drive);
}

void TeeBeeFilter::setMode(int newMode)
{
  if( newMode >= 0 && newMode < NUM_MODES )
  {
    mode = newMode;
    switch(mode)
    {
    case FLAT:      c0 =  1.0; c1 =  0.0; c2 =  0.0; c3 =  0.0; c4 =  0.0;  break;
    case LP_6:      c0 =  0.0; c1 =  1.0; c2 =  0.0; c3 =  0.0; c4 =  0.0;  break;
    case LP_12:     c0 =  0.0; c1 =  0.0; c2 =  1.0; c3 =  0.0; c4 =  0.0;  break;
    case LP_18:     c0 =  0.0; c1 =  0.0; c2 =  0.0; c3 =  1.0; c4 =  0.0;  break;
    case LP_24:     c0 =  0.0; c1 =  0.0; c2 =  0.0; c3 =  0.0; c4 =  1.0;  break;
    case HP_6:      c0 =  1.0; c1 = -1.0; c2 =  0.0; c3 =  0.0; c4 =  0.0;  break;
    case HP_12:     c0 =  1.0; c1 = -2.0; c2 =  1.0; c3 =  0.0; c4 =  0.0;  break;
    case HP_18:     c0 =  1.0; c1 = -3.0; c2 =  3.0; c3 = -1.0; c4 =  0.0;  break;
    case HP_24:     c0 =  1.0; c1 = -4.0; c2 =  6.0; c3 = -4.0; c4 =  1.0;  break;
    case BP_12_12:  c0 =  0.0; c1 =  0.0; c2 =  1.0; c3 = -2.0; c4 =  1.0;  break;
    case BP_6_18:   c0 =  0.0; c1 =  0.0; c2 =  0.0; c3 =  1.0; c4 = -1.0;  break;
    case BP_18_6:   c0 =  0.0; c1 =  1.0; c2 = -3.0; c3 =  3.0; c4 = -1.0;  break;
    case BP_6_12:   c0 =  0.0; c1 =  0.0; c2 =  1.0; c3 = -1.0; c4 =  0.0;  break;
    case BP_12_6:   c0 =  0.0; c1 =  1.0; c2 = -2.0; c3 =  1.0; c4 =  0.0;  break;
    case BP_6_6:    c0 =  0.0; c1 =  1.0; c2 = -1.0; c3 =  0.0; c4 =  0.0;  break;
    default:        c0 =  1.0; c1 =  0.0; c2 =  0.0; c3 =  0.0; c4 =  0.0;  // flat
    }
  }
  calculateCoefficientsApprox4();
}

//-------------------------------------------------------------------------------------------------
// others:

void TeeBeeFilter::reset()
{
  feedbackHighpass.reset();
  y1 = 0.0;
  y2 = 0.0;
  y3 = 0.0;
  y4 = 0.0;
}
