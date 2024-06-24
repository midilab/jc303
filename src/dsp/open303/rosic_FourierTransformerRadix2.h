#ifndef rosic_FourierTransformerRadix2_h
#define rosic_FourierTransformerRadix2_h

// standard includes:
#include <stdio.h>

// rosic-indcludes:
#include "rosic_Complex.h"
#include "rosic_RealFunctions.h"

namespace rosic
{

  /**

  This class performs a fast Fourier Transform on a block of complex numbers (which are of class
  "Complex"). The length of the block has to be a power of 2. It uses the FFT library by Takuya Ooura
  which seems to be rather efficient. It handles the conversion between doubles and Complex numbers
  by means of some hacky pointer trickery which relies on the fact that an object of class Complex 
  can be interpreted as two doubles stored in subsequent memory locations. This is true for the 
  compiler in MSVC 2005 but may or may not be true for other compilers. In case of problems, try the 
  class FourierTransfromerRadix2Clean which goes without such nasty hacks but is vastly inferior 
  efficiency-wise.

  */

  class FourierTransformerRadix2  
  {

  public:

    /** The direction of the transform. */
    enum directions
    {
      FORWARD,
      INVERSE
    };

    /** These are the possible normalization modes. */
    enum normalizationModes
    {
      NORMALIZE_ON_FORWARD_TRAFO, // divide by blockSize on forward FFT
      NORMALIZE_ON_INVERSE_TRAFO, // divide by blockSize on inverse FFT (default)
      ORTHONORMAL_TRAFO           // divide by sqrt(blockSize) on both transforms
    };

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. */
    FourierTransformerRadix2();  

    /** Destructor. */
    ~FourierTransformerRadix2();

    //---------------------------------------------------------------------------------------------
    // parameter settings:

    /** FFT-size, has to be a power of 2 and >= 2. */
    void setBlockSize(int newBlockSize);     

    /** Sets the direction of the transform (@see: directions). This will affect the sign of the 
    exponent (or equivalently: theimaginary part) in the twiddling factors and the normalization 
    constant. */
    void setDirection(int newDirection);

    /** When you switch bewteen usage of this object for real or complex signals, you will need to 
    call this switch-function in between which just triggers a re-computation of the twiddle 
    factors (which must be different for the two cases). */
    void setRealSignalMode(bool willBeUsedForRealSignals);

    /** Sets the mode for normalization of the output (@see: normalizationModes). */
    void setNormalizationMode(int newNormalizationMode);

    //---------------------------------------------------------------------------------------------
    // complex Fourier transforms:

    /** Transforms a buffer of complex numbers into its (forward or inverse) fourier transform. 
    The inBuffer will remain intact. Both, inBuffer and outBuffer must be of the size which was 
    specified when setting up the blockSize with setBlockSize(). */
    void transformComplexBuffer(Complex *inBuffer, Complex *outBuffer);   

    /** Does the same thing as transformComplexBuffer but performes in-place calculation 
    (overwrites the input buffer). */
    void transformComplexBufferInPlace(Complex *buffer);         

    //---------------------------------------------------------------------------------------------
    // some convenience functions for dealing with real signals:

    /** Transforms a real signal into its corresponding (conjugate symmetric) complex spectrum 
    using an algorithm which exploits the symmetries for more efficiency. When the input array is
    an array of doubles of length N, the output array will be an array of complex numbers (class 
    Complex) of length N/2 with the (purely real) transform value of bin N/2 stored in the 
    imaginary part of the first array entry (outSpectrum[0].im = real(N/2)). */
    void transformRealSignal(double *inSignal, Complex *outSpectrum);  

    /** Calculates real and imaginary part of the spectrum as interleaved double buffer: 
    buf[2]=re[1], buf[3]=im[1], buf[4]=re[2], buf[5]=im[2],... in general: buf[2*k]=re[k], 
    buf[2k+1]=im[k], k=1,..,(N/2)-1 where N is the FFT-size. The first two elements of the buffer 
    have a special meaning: buf[0] is the (purely real) DC and buf[1] is the (purely real) 
    coefficient for the Nyquist frequency. The other fields contain the real and imaginary parts of
    the positive frequencies only (interleaved) because the negative frequencies are redundant 
    (they are conjugate symmetric). */
    void transformRealSignal(double *signal, double *reAndIm);  

    /** Calculates spectral magnitudes and phases from a signal, where *signal should be of
    length N, where N is the block-size as chosen with setBlockSize() *magnitudes and *phases 
    should be of length N/2. */
    void getRealSignalMagnitudesAndPhases(double *signal, double *magnitudes, double *phases);

    /** Calculates the magnitudes only from a signal (useful for analyzer-stuff). */
    void getRealSignalMagnitudes(double *signal, double *magnitudes);

    /** Transforms a complex conjugate symmetric spectrum (i.e. a spectrum of a real signal) into
    the corresponding real signal. */
    void transformSymmetricSpectrum(Complex *inSpectrum, double *outSignal);

    /** Calculates a time signal from and interleaved buffer containing the real and imaginary 
    parts of the positive frequencies (the negative frequencies are assumed to be conjugate 
    symmetric). */
    void transformSymmetricSpectrum(double *reAndIm, double *signal);

    /** Calculates a real time signal from its magnitudes and phases, *magnitudes and *phases
    should be of length N/2, *signal is of length N where N is the block-size as chosen with 
    setBlockSize(). */
    void getRealSignalFromMagnitudesAndPhases(double *magnitudes, double *phases, double *signal);

    //---------------------------------------------------------------------------------------------
    // static functions

    /** Returns the physical frequency in Hz that corresponds to the given 'binIndex' for a given
    'fftSize' and 'sampleRate'. */
    static double binIndexToFrequency(int binIndex, int fftSize, double sampleRate) 
    { return binIndex*sampleRate/fftSize; }


    //=============================================================================================

  protected:

    /** Updates the normalizationFactor member variable acording to a new blockSize, direction or
    normalizationMode. */
    void updateNormalizationFactor();

    int    N;                    /**< the blocksize of the FFT. */
    int    logN;                 /**< Base 2 logarithm of the blocksize. */
    int    direction;            /**< The direction of the transform (@see: directions). */
    int    normalizationMode;    /**< The normalization mode (@see: normalizationModes. */
    double normalizationFactor;  /**< The normalization factor (can be 1, 1/N or 1/sqrt(N)). */

    // work-area stuff for Ooura's fft-routines:
    double *w;                   /**< Table of the twiddle-factors. */
    int    *ip;                  /**< Work area for bit-reversal (index pointer?). */

    // our own temporary storage area:
    Complex* tmpBuffer;

  };

} // end namespace rosic

#endif // rosic_FourierTransformerRadix2_h
