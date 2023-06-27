#ifndef rosic_MipMappedWaveTable_h
#define rosic_MipMappedWaveTable_h

// rosic-indcludes:
#include "rosic_FunctionTemplates.h"
#include "rosic_FourierTransformerRadix2.h"

namespace rosic
{

  /**

  This is a class for generating and storing a single-cycle-waveform in a lookup-table and 
  retrieving values form it at arbitrary positions by means of interpolation.

  */

  class MipMappedWaveTable
  {

    // Oscillator and SuperOscillator classes need access to certain protected member-variables 
    // (namely the tableLength and related quantities), so we declare them as friend-classes:
    friend class Oscillator;
    friend class BlendOscillator;
    friend class SuperOscillator;
    // \ todo: get rid of this by providing get-functions

  public:

    enum waveforms
    {
      SILENCE = 0,
      SINE, 
      TRIANGLE,
      SQUARE,
      SAW,
      SQUARE303,
      SAW303
    };

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. */
    MipMappedWaveTable();          

    /** Destructor. */
    ~MipMappedWaveTable();         

    //---------------------------------------------------------------------------------------------
    // parmeter-settings:

    /** Selects a waveform from the set of built-in wavforms. The object generates the 
    prototype-waveform by some algorithmic rules and renders various bandlimited version of it via 
    FFT/iFFT. */
    void setWaveform(int newWaveform);

    /** Overloaded function to set the waveform form outside this class. This function expects a 
    pointer to the prototype-waveform to be handed over along with the length of this waveform. It 
    copies the values into the internal buffers and renders various bandlimited version via 
    FFT/iFFT.
    \todo: Interpolation for the case that lengthInSamples does not match the length of the 
    internal table-length. */
    void setWaveform(double* newWaveform, int lengthInSamples);

    /** Sets the time symmetry between the first and second half-wave (as value between 0...1) - 
    for a square wave, this is also known as pulse-width. Currently only implemented for square and 
    saw waveforms. */
    void setSymmetry(double newSymmetry);

    // internal 'back-panel' parameters:

    /** Sets the drive (in dB) for the tanh-shaper for 303-square waveform - internal parameter, to 
    be scrapped eventually. */
    void setTanhShaperDriveFor303Square(double newDrive)
    { tanhShaperFactor = dB2amp(newDrive); fillWithSquare303(); }

    /** Sets the offset (as raw value for the tanh-shaper for 303-square waveform - internal 
    parameter, to be scrapped eventually. */
    void setTanhShaperOffsetFor303Square(double newOffset)
    { tanhShaperOffset = newOffset; fillWithSquare303(); }

    /** Sets the phase shift of tanh-shaped square wave with respect to the saw-wave (in degrees)
    - this is important when the two are mixed. */
    void set303SquarePhaseShift(double newShift)
    { squarePhaseShift = newShift; fillWithSquare303(); }

    //---------------------------------------------------------------------------------------------
    // inquiry:

    /** Returns the drive (in dB) for the tanh-shaper for 303-square waveform - internal parameter, to 
    be scrapped eventually. */
    double getTanhShaperDriveFor303Square() const { return amp2dB(tanhShaperFactor); }

    /** Returns the offset (as raw value for the tanh-shaper for 303-square waveform - internal 
    parameter, to be scrapped eventually. */
    double getTanhShaperOffsetFor303Square() const { return tanhShaperOffset; }

    /** Returns the phase shift of tanh-shaped square wave with respect to the saw-wave (in degrees)
    - this is important when the two are mixed. */
    double get303SquarePhaseShift() const { return squarePhaseShift; }

    //---------------------------------------------------------------------------------------------
    // audio processing:

    /** Returns the value at position 'integerPart+fractionalPart' of table 'tableIndex' with 
    linear interpolation - this function may be preferred over 
    getValueLinear(double phaseIndex, int tableIndex) when you want to calculate the integer and 
    fractional part of the phase-index yourself. */
    INLINE double getValueLinear(int integerPart, double fractionalPart, int tableIndex);

    /** Returns the value at position 'phaseIndex' of table 'tableIndex' with linear 
    interpolation - this function computes the integer and fractional part of the phaseIndex
    internally. */
    INLINE double getValueLinear(double phaseIndex, int tableIndex);

  protected:

    // functions to fill table with the built-in waveforms (these functions are
    // called from setWaveform(int newWaveform):
    void fillWithSine();
    void fillWithTriangle();
    void fillWithSquare();
    void fillWithSaw();
    void fillWithSquare303();
    void fillWithSaw303();
    void fillWithPeak();
    void fillWithMoogSaw();

    void initPrototypeTable();
      // fills the "prototypeTable"-variable with all zeros

    void initTableSet();
      // fills the "tableSet"-variable with all zeros

    void removeDC();
      // removes dc-component from the waveform in the prototype-table

    void normalize();
      // normalizes the amplitude of the prototype-table to 1.0

    void reverseTime();
      // time-reverses the prototype-table

    /** Renders the prototype waveform and generates the mip-map from that. */
    void renderWaveform();

    void generateMipMap();
      // generates a multisample from the prototype table, where each of the
      // successive tables contains one half of the spectrum of the previous one

    static const int tableLength = 2048;
      // Length of the lookup-table. The actual length of the allocated memory is 4 samples longer, 
      // to store additional samples for the interpolator (which are the same values as at the 
      // beginning of the buffer) */


    double symmetry; // symmetry between 1st and 2nd half-wave

    static const int numTables = 12;
      // The Oscillator class uses a one table-per octave multisampling to avoid aliasing. With a 
      // table-size of 8192 and a sample-sample rate of  44100, the 12th table will have a 
      // fundamental frequency (the frequency where the increment is 1) of 11025 which is good for 
      // the highest frequency. 

    int    waveform;   // index of the currently chosen native waveform
    double sampleRate; // the sampleRate

    double prototypeTable[tableLength];
      // this is the prototype-table with full bandwidth. one additional sample (same as 
      // prototypeTable[0]) for linear interpolation without need for table wraparound at the last 
      // sample (-> saves one if-statement each audio-cycle) ...and a three further addtional 
      // samples for more elaborate interpolations like cubic (not implemented yet, also:
      // the fillWith...()-functions don't support these samples yet). */

    double tableSet[numTables][tableLength+4];
      // The multisample for anti-aliased waveform generation. The 4 additional values are equal 
      // to the first 4 values in the table for easier interpolation. The first index is for the 
      // table-number - index 0 accesses the first version which has full bandwidth, index 1 
      // accesses the second version which is bandlimited to Nyquist/2, 2->Nyquist/4, 
      // 3->Nyquist/8, etc. */

    // embedded objects:
    FourierTransformerRadix2 fourierTransformer;

    // internal parameters:
    double tanhShaperFactor, tanhShaperOffset, squarePhaseShift;

  };

  //-----------------------------------------------------------------------------------------------
  // inlined functions:
    
  INLINE double MipMappedWaveTable::getValueLinear(int integerPart, double fractionalPart, int tableIndex)
  {
    // ensure, that the table index is in the valid range:
    if( tableIndex<=0 )
      tableIndex = 0;
    else if ( tableIndex>numTables )
      tableIndex = 11;

    return   (1.0-fractionalPart) * tableSet[tableIndex][integerPart] 
           +      fractionalPart  * tableSet[tableIndex][integerPart+1];
  }

  INLINE double MipMappedWaveTable::getValueLinear(double phaseIndex, int tableIndex)
  {
    /*
    // ensure, that the table index is in the valid range:
    if( tableIndex<=0 )
      tableIndex = 0;
    else if ( tableIndex>numTables )
      tableIndex = 11;
      */

    // calculate integer and fractional part of the phaseIndex:
    int    intIndex = floorInt(phaseIndex);
    double frac     = phaseIndex  - (double) intIndex;
    return getValueLinear(intIndex, frac, tableIndex);

    // lookup value in the table with linear interpolation and return it:
    //return (1.0-frac)*tableSet[tableIndex][intIndex] + frac*tableSet[tableIndex][intIndex+1];
  }

} // end namespace rosic

#endif // rosic_MipMappedWaveTable_h
