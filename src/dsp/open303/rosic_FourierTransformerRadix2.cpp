#include "rosic_FourierTransformerRadix2.h"
#include "fft4g.c"
using namespace rosic;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

FourierTransformerRadix2::FourierTransformerRadix2()
{
  N                   = 0;
  logN                = 0;
  direction           = FORWARD;
  normalizationMode   = NORMALIZE_ON_INVERSE_TRAFO;
  normalizationFactor = 1.0;
  w                   = NULL;
  ip                  = NULL;
  tmpBuffer           = NULL;

  setBlockSize(256);
}

FourierTransformerRadix2::~FourierTransformerRadix2()
{
  // free dynamically allocated memory:
  if( w != NULL )
    delete[] w;
  if( ip != NULL )
    delete[] ip;
  if( tmpBuffer != NULL )
    delete[] tmpBuffer;
}

//-------------------------------------------------------------------------------------------------
// parameter settings:

void FourierTransformerRadix2::setBlockSize(int newBlockSize)
{
  // check new blocksize for validity:
  if( newBlockSize >= 2 && isPowerOfTwo(newBlockSize) )
  {
    // check, if the new blocksize is actually different from the old one in order to avoid 
    // unnecesarry re-allocations and re-computations:
    if( newBlockSize != N )
    {
      N    = newBlockSize;
      logN = (int) floor( log2((double) N + 0.5 ) );
      updateNormalizationFactor();

      if( w != NULL )
        delete[] w;
      w    = new double[2*N];

      if( ip != NULL )
        delete[] ip;
      ip    = new int[(int) ceil(4.0+sqrt((double)N))];
      ip[0] = 0; // indicate that re-initialization is necesarry

      if( tmpBuffer != NULL )
        delete[] tmpBuffer;
      tmpBuffer = new Complex[N];
    }
  }
  else if( !isPowerOfTwo(newBlockSize) || newBlockSize <= 1 )
    DEBUG_BREAK; // this class can only deal with blocksizes >= 2 that are a power of two
}

void FourierTransformerRadix2::setDirection(int newDirection)
{
  if( newDirection >= FORWARD && newDirection <= INVERSE )
  {
    // only when the new direction is actually different form the old one, we have to conjugate 
    // all the twiddle-factors, otherwise everything must stay as is:
    if( newDirection != direction )
    {
      direction = newDirection;
      updateNormalizationFactor();
    }
  }
  else
    DEBUG_BREAK; // passed int-parameter does not correspond to any meaningful enum-field
}

void FourierTransformerRadix2::setNormalizationMode(int newNormalizationMode)
{
  if( newNormalizationMode >= NORMALIZE_ON_FORWARD_TRAFO && 
      newNormalizationMode <= ORTHONORMAL_TRAFO )
  {
    normalizationMode = newNormalizationMode;
    updateNormalizationFactor();
  }
  else
    DEBUG_BREAK; // passed int-parameter does not correspond to any meaningful enum-field
}

void FourierTransformerRadix2::setRealSignalMode(bool willBeUsedForRealSignals)
{
  ip[0] = 0; // retriggers twiddle-factor computation
}

//-------------------------------------------------------------------------------------------------
// signal processing:

void FourierTransformerRadix2::transformComplexBufferInPlace(Complex *buffer)
{
  // retrieve the adresses of the real part of the first array entries in order to treat the 
  // Complex arrays as arrays of two successive double-numbers:
  double* d_buffer = &(buffer[0].re);

  // normalize the FFT-input, if required:
  if( normalizationFactor != 1.0 )
  {
    for(int n=0; n<2*N; n++)
      d_buffer[n] *= normalizationFactor;
  }

  // use Ooura's routine:
  int sign;
  if( direction == FORWARD )
    sign = -1;
  else
    sign = +1;
  cdft(2*N, sign, d_buffer, ip, w);
}

void FourierTransformerRadix2::transformComplexBuffer(Complex *inBuffer, Complex *outBuffer)
{
  // retrieve the adresses of the real part of the first array entries in order to treat the 
  // Complex arrays as arrays of two successive double-numbers:
  double* d_inBuffer  = &(inBuffer[0].re);
  double* d_outBuffer = &(outBuffer[0].re);

  // copy the input into the output for the in-place routine (thereby normalize, if necesarry):
  int n;
  if( normalizationFactor != 1.0 )
  {
    for(n=0; n<2*N; n++)
      d_outBuffer[n] = d_inBuffer[n] * normalizationFactor;
  }
  else
  {
    for(n=0; n<2*N; n++)
      d_outBuffer[n] = d_inBuffer[n];
  }

  // use Ooura's routine:
  int sign;
  if( direction == FORWARD )
    sign = -1;
  else
    sign = +1;
  cdft(2*N, sign, d_outBuffer, ip, w);
}

//-------------------------------------------------------------------------------------------------
// convenience functions for real signal:

void FourierTransformerRadix2::transformRealSignal(double *inSignal, Complex *outSpectrum)
{
  setDirection(FORWARD);

  // retrieve the adress of the real part of the first array entry of the output array in order to
  // treat the Complex array as array of two successive double-numbers:
  double* d_outBuffer = &(outSpectrum[0].re);

  // copy the input into the output for the in-place routine (thereby normalize, if necesarry):
  int n;
  if( normalizationFactor != 1.0 )
  {
    for(n=0; n<N; n++)
      d_outBuffer[n] = inSignal[n] * normalizationFactor;
  }
  else
  {
    for(n=0; n<N; n++)
      d_outBuffer[n] = inSignal[n];
  }

  // use Ooura's routine:
  rdft(N, 1, d_outBuffer, ip, w);

  // for some reason, this routine returns the second half of the spectrum (the complex conjugate 
  // values of the desired first half), so we need to take the complex conjugates:
  for(n=3; n<N; n+=2) // start at n=3 (imaginary part of the first bin after DC)
    d_outBuffer[n] = -d_outBuffer[n];
}

void FourierTransformerRadix2::transformRealSignal(double *signal, double *reAndIm)
{
  Complex* c_reAndIm = (Complex*) &(reAndIm[0]);
  transformRealSignal(signal, c_reAndIm);
}


void FourierTransformerRadix2::getRealSignalMagnitudesAndPhases(double *signal, 
                                                                double *magnitudes, double *phases)
{
  transformRealSignal(signal, tmpBuffer);

  // store the two purely real transform values at DC and Nyquist-frequency in the first fields of 
  // the magnitude- and phase- arrays respectively:
  magnitudes[0] = tmpBuffer[0].re;
  phases[0]     = tmpBuffer[0].im;

  // fill the rest of the array with the magnitudes and phases of the regular bins:
  double* dBuffer = &(tmpBuffer[0].re);
  double  re, im;
  int     k;
  for(k=1; k<N/2; k++)
  {
    re            = dBuffer[2*k];
    im            = dBuffer[2*k+1];
    magnitudes[k] = sqrt(re*re + im*im);
    if( re == 0.0 && im == 0.0 )
      phases[k] = 0.0;
    else
      phases[k] = atan2(im, re);
  }
}

void FourierTransformerRadix2::getRealSignalMagnitudes(double *signal, double *magnitudes)
{
  transformRealSignal(signal, tmpBuffer);
  magnitudes[0] = tmpBuffer[0].re;

  double* dBuffer = &(tmpBuffer[0].re);
  double  re, im;
  int     k;
  for(k=1; k<N/2; k++)
  {
    re            = dBuffer[2*k];
    im            = dBuffer[2*k+1];
    magnitudes[k] = sqrt(re*re + im*im);
  }
}

void FourierTransformerRadix2::transformSymmetricSpectrum(Complex *inSpectrum, double *outSignal)
{
  setDirection(INVERSE);

  // retrieve the adress of the real part of the first array entry of the output array in order to
  // treat the Complex array as array of two successive double-numbers:
  double* d_inBuffer = &(inSpectrum[0].re);

  // copy the input into the output for the in-place routine (thereby normalize, if necesarry):
  int n;
  if( normalizationFactor != 1.0 )
  {
    for(n=0; n<N; n++)
      outSignal[n] = 2.0 * d_inBuffer[n] * normalizationFactor;
  }
  else
  {
    for(n=0; n<N; n++)
      outSignal[n] = 2.0 * d_inBuffer[n];
  }

  // for some reason, the subsequent routine expects the second half of the spectrum (the complex 
  // conjugate values of the first half), so we need to take the complex conjugates:
  for(n=3; n<N; n+=2) // start at n=3 (imaginary part of the first bin after DC)
    outSignal[n] = -outSignal[n];

  // use Ooura's routine:
  rdft(N, -1, outSignal, ip, w);
}

void FourierTransformerRadix2::transformSymmetricSpectrum(double *reAndIm, double *signal)
{
  Complex* c_reAndIm = (Complex*) &(reAndIm[0]);
  transformSymmetricSpectrum(c_reAndIm, signal);
}

void FourierTransformerRadix2::getRealSignalFromMagnitudesAndPhases(double *magnitudes, 
                                                                    double *phases, 
                                                                    double *signal)
{
  tmpBuffer[0].re = magnitudes[0];
  tmpBuffer[0].im = phases[0];

  int k;
  double* dBuffer = &(tmpBuffer[0].re);
  double  s, c;
  for(k=1; k<N/2; k++)
  {
    sinCos(phases[k], &s, &c);
    dBuffer[2*k]   = magnitudes[k] * c;
    dBuffer[2*k+1] = magnitudes[k] * s;
  }

  transformSymmetricSpectrum(tmpBuffer, signal);
}

//-------------------------------------------------------------------------------------------------
// pre-calculations:

void FourierTransformerRadix2::updateNormalizationFactor()
{
  if( (normalizationMode == NORMALIZE_ON_FORWARD_TRAFO && direction == FORWARD) ||
      (normalizationMode == NORMALIZE_ON_INVERSE_TRAFO && direction == INVERSE)    )
  {
    normalizationFactor = 1.0 / (double) N;
  }
  else if( normalizationMode == ORTHONORMAL_TRAFO )
  {
    normalizationFactor = 1.0 / sqrt((double) N);
  }
  else
    normalizationFactor = 1.0;
}



