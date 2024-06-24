#include "rosic_DecayEnvelope.h"
using namespace rosic;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

DecayEnvelope::DecayEnvelope()
{  
  c            = 1.0;    
  y            = 1.0;
  yInit        = 1.0;
  tau          = 200.0;  
  fs           = 44100.0;
  normalizeSum = false;
  calculateCoefficient();
}

DecayEnvelope::~DecayEnvelope()
{

}

//-------------------------------------------------------------------------------------------------
// parameter settings:

void DecayEnvelope::setSampleRate(double newSampleRate)
{
  if( newSampleRate > 0.0 )
  {
    fs = newSampleRate;
    calculateCoefficient();
  }
}

void DecayEnvelope::setDecayTimeConstant(double newTimeConstant)
{
  if( newTimeConstant > 0.001 ) // at least 0.001 ms decay
  {
    tau = newTimeConstant;
    calculateCoefficient();
  }
}

void DecayEnvelope::setNormalizeSum(bool shouldNormalizeSum)
{
  normalizeSum = shouldNormalizeSum;
  calculateCoefficient();
}

//-------------------------------------------------------------------------------------------------
// others:

void DecayEnvelope::trigger()
{
  y = yInit;
}

bool DecayEnvelope::endIsReached(double threshold)
{
  if( y < threshold )
    return true;
  else
    return false;
}

//-------------------------------------------------------------------------------------------------
// internal functions:

void DecayEnvelope::calculateCoefficient()
{
  c     = exp( -1.0 / (0.001*tau*fs) );
  if( normalizeSum == true )
    yInit = (1.0-c)/c;
  else  
    yInit = 1.0/c;
}

