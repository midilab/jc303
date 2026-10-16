#include "rosic_LeakyIntegrator.h"
using namespace rosic;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

LeakyIntegrator::LeakyIntegrator()
{
  sampleRate  = 44100.0f; 
  tau         = 10.0f;    
  y1          = 0.0;

  calculateCoefficient();
}

LeakyIntegrator::~LeakyIntegrator()
{

}

//-------------------------------------------------------------------------------------------------
// parameter settings:

void LeakyIntegrator::setSampleRate(double newSampleRate)
{
  if( newSampleRate > 0.0 )
  {
    sampleRate = newSampleRate;
    calculateCoefficient();
  }
}

void LeakyIntegrator::setTimeConstant(double newTimeConstant)
{
  if( newTimeConstant >= 0.0 && newTimeConstant != tau )
  {
    tau = newTimeConstant; 
    calculateCoefficient();
  }
}

//-------------------------------------------------------------------------------------------------
// inquiry:

double LeakyIntegrator::getNormalizer(double tau1, double tau2, double fs)
{
  double td = 0.001*tau1;
  double ta = 0.001*tau2;

  // catch some special cases:
  if( ta == 0.0 && td == 0.0 )
    return 1.0;
  else if( ta == 0.0 )
  {
    return 1.0 / (1.0-exp(-1.0/(fs*td)));
  }
  else if( td == 0.0 )
  {
    return 1.0 / (1.0-exp(-1.0/(fs*ta)));
  }

  // compute the filter coefficients:
  double x  = exp( -1.0 / (fs*td)  );
  double bd = 1-x;
  double ad = -x;
  x         = exp( -1.0 / (fs*ta)  );
  double ba = 1-x;
  double aa = -x;

  // compute the location and height of the peak:
  double xp;
  if( ta == td )
  {
    double tp  = ta;
    double np  = fs*tp;
    xp         = (np+1.0)*ba*ba*pow(aa, np);
  }
  else
  {
    double tp  = log(ta/td) / ( (1.0/td) - (1.0/ta) );
    double np  = fs*tp;
    double s   = 1.0 / (aa-ad);
    double b01 = s * aa*ba*bd;
    double b02 = s * ad*ba*bd;
    double a01 = s * (ad-aa)*aa;
    double a02 = s * (ad-aa)*ad;
    xp         = b01*pow(a01, np) - b02*pow(a02, np);
  }

  // return the normalizer as reciprocal of the peak height:
  return 1.0/xp;
}

//-------------------------------------------------------------------------------------------------
// others:

void LeakyIntegrator::reset()
{
  y1 = 0;
}

void LeakyIntegrator::calculateCoefficient()
{
  if( tau > 0.0 )
    coeff = exp( -1.0 / (sampleRate*0.001*tau)  );
  else
    coeff = 0.0;
}

