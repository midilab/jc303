#include "rosic_BiquadFilter.h"
using namespace rosic;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

BiquadFilter::BiquadFilter()
{
  frequency  = 1000.0;
  gain       = 0.0;
  bandwidth  = 2.0*asinh(1.0/sqrt(2.0))/log(2.0);
  sampleRate = 44100.0;
  mode       = BYPASS;
  calcCoeffs();
  reset();    
}

//-------------------------------------------------------------------------------------------------
// parameter settings:

void BiquadFilter::setSampleRate(double newSampleRate)
{
  if( newSampleRate > 0.0 )
    sampleRate = newSampleRate;
  calcCoeffs();
}

void BiquadFilter::setMode(int newMode)
{
  mode = newMode; // 0:bypass, 1:Low Pass, 2:High Pass
  calcCoeffs();
}

void BiquadFilter::setFrequency(double newFrequency)
{
  frequency = newFrequency;
  calcCoeffs();
}

void BiquadFilter::setGain(double newGain)
{
  gain = newGain;
  calcCoeffs();
}

void BiquadFilter::setBandwidth(double newBandwidth)
{
  bandwidth = newBandwidth;
  calcCoeffs();
}

//-------------------------------------------------------------------------------------------------
//others:

void BiquadFilter::calcCoeffs()
{
  double w = 2*PI*frequency/sampleRate;
  double s, c;
  switch(mode)
  {
  case LOWPASS6: 
    {
      // formula from dspguide:
      double x = exp(-w);
      a1 = x;
      a2 = 0.0;
      b0 = 1.0-x;
      b1 = 0.0;
      b2 = 0.0;
    }
    break;
  case LOWPASS12: 
    {
      // formula from Robert Bristow Johnson's biquad cookbook:
      sinCos(w, &s, &c);
      double q     = dB2amp(gain);
      double alpha = s/(2.0*q);
      double scale = 1.0/(1.0+alpha);
      a1 = 2.0*c       * scale;
      a2 = (alpha-1.0) * scale;
      b1 = (1.0-c)     * scale;
      b0 = 0.5*b1;
      b2 = b0;
    }
    break;
  case HIGHPASS6: 
    {
      // formula from dspguide:
      double x = exp(-w);
      a1 = x;
      a2 = 0.0;
      b0 = 0.5*(1.0+x);
      b1 = -b0;
      b2 = 0.0;
    }
    break;
  case HIGHPASS12: 
    {
      // formula from Robert Bristow Johnson's biquad cookbook:
      sinCos(w, &s, &c);
      double q     = dB2amp(gain);
      double alpha = s/(2.0*q);
      double scale = 1.0/(1.0+alpha);
      a1 = 2.0*c       * scale;
      a2 = (alpha-1.0) * scale;
      b1 = -(1.0+c)    * scale;
      b0 = -0.5*b1;
      b2 = b0;
    }
    break;
  case BANDPASS: 
    {
      // formula from Robert Bristow Johnson's biquad cookbook:      
      sinCos(w, &s, &c);
      double alpha = s * sinh( 0.5*log(2.0) * bandwidth * w / s );
      double scale = 1.0/(1.0+alpha);
      a1 = 2.0*c       * scale;
      a2 = (alpha-1.0) * scale;
      b1 = 0.0;
      b0 = 0.5*s       * scale;
      b2 = -b0;
    }
    break;
  case BANDREJECT: 
    {
      // formula from Robert Bristow Johnson's biquad cookbook:
      sinCos(w, &s, &c);
      double alpha = s * sinh( 0.5*log(2.0) * bandwidth * w / s );
      double scale = 1.0/(1.0+alpha);
      a1 = 2.0*c       * scale;
      a2 = (alpha-1.0) * scale;
      b0 = 1.0         * scale;
      b1 = -2.0*c      * scale;
      b2 = 1.0         * scale;
    }
    break;
  case PEAK: 
    {
      // formula from Robert Bristow Johnson's biquad cookbook:
      sinCos(w, &s, &c);
      double alpha = s * sinh( 0.5*log(2.0) * bandwidth * w / s );
      double A     = dB2amp(gain);
      double scale = 1.0/(1.0+alpha/A);
      a1 = 2.0*c             * scale;
      a2 = ((alpha/A) - 1.0) * scale;
      b0 = (1.0+alpha*A)     * scale;
      b1 = -2.0*c            * scale;
      b2 = (1.0-alpha*A)     * scale;
    }
    break;
  case LOW_SHELF: 
    {
      // formula from Robert Bristow Johnson's biquad cookbook:
      sinCos(w, &s, &c);
      double A     = dB2amp(0.5*gain);
      double q     = 1.0 / (2.0*sinh( 0.5*log(2.0) * bandwidth ));
      double beta  = sqrt(A) / q;
      double scale = 1.0 / ( (A+1.0) + (A-1.0)*c + beta*s);
      a1 = 2.0 *     ( (A-1.0) + (A+1.0)*c          ) * scale;
      a2 = -         ( (A+1.0) + (A-1.0)*c - beta*s ) * scale;
      b0 =       A * ( (A+1.0) - (A-1.0)*c + beta*s ) * scale;
      b1 = 2.0 * A * ( (A-1.0) - (A+1.0)*c          ) * scale;
      b2 =       A * ( (A+1.0) - (A-1.0)*c - beta*s ) * scale;
    }
    break;




    // \todo: implement shelving and allpass modes

  default: // bypass
    {
      b0 = 1.0;
      b1 = 0.0;
      b2 = 0.0;
      a1 = 0.0;
      a2 = 0.0;
    }break;
  }
}

void BiquadFilter::reset()
{
  x1 = 0.0;
  x2 = 0.0;
  y1 = 0.0;
  y2 = 0.0;
}
