#ifndef rosic_AnalogEnvelope_h
#define rosic_AnalogEnvelope_h

// rosic-indcludes:
#include "rosic_RealFunctions.h"

namespace rosic
{

  /**

  This is a class which generates an exponential envelope with adjustable start-, attack-, peak-, 
  hold-,  decay-, sustain-, release- and end-values. It is based on feeding a stairstep-like 
  input signal into a RC-filter unit. The filter input signal is switched to a new value 
  according to the time and level values, at the same time the filter is switched to it's new 
  time constant. This also implies, that the level-value will not really be reached (in theory) but 
  only approached asymptotically. So the time values are not really the time between the levels, 
  but rather time constants tau of the RC unit. The time constant tau is defined as the time until 
  the filter reaches 63.2% of the end value (for an incoming step-function). This time constant can 
  be scaled to re-define the ramp time to other values than 63.2%.

  */

  class AnalogEnvelope
  {

  public:

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. */
    AnalogEnvelope();  

    /** Destructor. */
    ~AnalogEnvelope(); 

    //---------------------------------------------------------------------------------------------
    // parameter settings:

    /** sets the sample-rate. */
    void setSampleRate(double newSampleRate);  

    /** Sets the point where the envelope starts (as raw value). */
    void setStartLevel(double newStart) { startLevel = newStart; }

    /** Sets the point where the envelope starts (in dB). */
    void setStartInDecibels(double newStart) { setStartLevel(dB2amp(newStart)); }

    /** Sets the point where the envelope starts (in semitones). */
    void setStartInSemitones(double newStart) { setStartLevel(pitchOffsetToFreqFactor(newStart)); }  

    /** Sets the highest point of the envelope (as raw value). */
    void setPeakLevel(double newPeak) { peakLevel = newPeak; }

    /** Sets the highest point of the envelope (in dB). */
    void setPeakInDecibels(double newPeak) { setPeakLevel(dB2amp(newPeak)); }

    /** Sets the highest point of the envelope (in semitones). */
    void setPeakInSemitones(double newPeak) { setPeakLevel(pitchOffsetToFreqFactor(newPeak)); }

    /** Sets the velocity dependence of the peak level as scaling factor of the peak by notes with 
    velocity == 127. Notes with velocity == 1 will use the reciprocal value and notes with 
    velocity == 64 will use the unmodified peak value. */
    void setPeakLevelByVel(double newPeakByVel) { peakByVel = newPeakByVel; }

    /** Sets the velocity dependence of the peak level in dB - notes with velocity == 127 will peak 
    this value louder, notes with velocity == 0 will peak this value more quiet and notes with 
    velocity == 64 will have an unmodified peak amplitude. */
    void setPeakByVelInDecibels(double newPeakByVel) { setPeakLevelByVel(dB2amp(newPeakByVel)); }

    /** Sets the velocity dependence of the peak level in semitones .... */
    void setPeakByVelInSemitones(double newPeakByVel) 
    { setPeakLevelByVel(pitchOffsetToFreqFactor(newPeakByVel)); }

    /** Sets the sustain level (as raw value). */
    void setSustainLevel(double newSustain) { sustainLevel = newSustain; }

    /** Sets the sustain level (in dB). */
    void setSustainInDecibels(double newSustain) { setSustainLevel(dB2amp(newSustain)); }

    /** Sets the sustain level (in semitones). */
    void setSustainInSemitones(double newSustain) 
    { setSustainLevel(pitchOffsetToFreqFactor(newSustain)); }

    /** Sets the end point of the envelope (as raw value). */
    void setEndLevel(double newEnd) { endLevel = newEnd; }

    /** Sets the end point of the envelope (in dB). */
    void setEndInDecibels(double newEnd) { setEndLevel(dB2amp(newEnd)); }

    /** Sets the end point of the envelope (in semitones). */
    void setEndInSemitones(double newEnd) { setEndLevel(pitchOffsetToFreqFactor(newEnd)); }

    /** Sets the length of the attack phase (in milliseconds). */
    void setAttack(double newAttackTime);    

    /** Sets the hold time (in milliseconds). */
    void setHold(double newHoldTime);      

    /** Sets the length of the decay phase (in milliseconds). */
    void setDecay(double newDecayTime);     
 
    /** Sets the length of the release phase (in milliseconds). */
    void setRelease(double newReleaseTime);  

    /** Scales the A,D,H and R times by adjusting the increment. It is 1 if not used - a timescale 
    of 2 means the envelope is twice as fast, 0.5 means half as fast -> useful for implementing a 
    key/velocity-tracking feature for the overall length for the envelope. */
    void setTimeScale(double newTimeScale); 

    /** Scales the time constants tau. Can be used to reach other values than 63.2% in the 
    specified time values */
    void setTauScale(double newTauScale);  

    /** Scales the peak-value of the envelope - useful for velocity response. */
    void setPeakScale(double newPeakScale); 

    /** Sets the internal state of the RC-filter. */
    void setInternalState(double newState) { previousOutput = newState; }

    //---------------------------------------------------------------------------------------------
    // inquiry:

    /** Returns the length of the attack phase (in milliseconds). */
    double getAttack() const { return attackTime; }

    /** Returns the length of the decay phase (in milliseconds). */
    double getDecay() const { return decayTime; }

    /** Returns the sustain level (as raw value). */
    double getSustain() const { return sustainLevel; }

    /** Returns the length of the release phase (in milliseconds). */
    double getRelease() const { return releaseTime; }

    /** Returns, when currently a note is on (the noteIsOn flag is set). */
    bool isNoteOn() const { return noteIsOn; }

    /** True, if output is below 40 dB. */
    bool endIsReached();  

    //---------------------------------------------------------------------------------------------
    // audio processing:

    /** Calculates one output sample at a time. */
    INLINE double getSample();    

    //---------------------------------------------------------------------------------------------
    // others:

    /** Causes the envelope to start with its attack-phase. When the parameter 
    'startFromCurrentValue' is true, the internal state will not be reset to startLevel, such that 
    the curve begins at the level, where the envelope currently is. */  
    void noteOn(bool startFromCurrentLevel = false, int newKey = 64, int newVel = 64);

    /** Causes the envelope to start with its release-phase. */
    void noteOff();  

    /** Resets the time variable. */
    void reset();   

  protected:

    /** Calculates our members that represent accumulated time values from attack, hold, etc. */
    void calculateAccumulatedTimes();

    // level and time parameters:
    double startLevel, peakLevel, sustainLevel, endLevel;  
    double attackTime, holdTime, decayTime, releaseTime;    // in seconds
    double peakByVel, peakByKey, timeScaleByVel, timeScaleByKey;

    // accumulated time values:
    double attPlusHld, attPlusHldPlusDec, attPlusHldPlusDecPlusRel;

    double time;       // time since the last call to to trigger() 
    double timeScale;  // scale the time constants in the filters according to
    double increment;  // increment for the time variable per sample 
    double tauScale;   // scale factor for the time constants of the filters
    double peakScale;  // scale factor for the peak-value

    double attackCoeff,  decayCoeff, releaseCoeff;   // filter coefficients
    double previousOutput;                           // previous output sample
    double sampleRate;                               // sample-rate
    bool   outputIsZero;                             // indicates if envelope has reached its end
    bool   noteIsOn;                                 // indicates if note is being held

  };

  //-----------------------------------------------------------------------------------------------
  // inlined functions:

  INLINE double AnalogEnvelope::getSample()
  {
    double out;

    // attack or hold phase:
    if(time <= attPlusHld)   // noteIsOn has not to be checked, because, time is advanced to the 
                             // beginning of the release phase in noteOff()
    {
      out   = previousOutput + attackCoeff * (peakScale*peakLevel - previousOutput);
      time += increment;
    }

    // decay phase:
    else if(time <= (attPlusHldPlusDec)) // noteIsOn has not to be checked
    {
      out   = previousOutput + decayCoeff * (sustainLevel - previousOutput);
      time += increment;
    }

    // sustain phase:
    else if(noteIsOn)
    {
      out = previousOutput + decayCoeff * (sustainLevel - previousOutput);
      // time is not incremented in sustain
    }

    // release phase:
    else
    {
      out   = previousOutput + releaseCoeff * (endLevel - previousOutput);
      time += increment;
    }

    // store output sample for next call:
    previousOutput = out; // + TINY;  // TINY is to avoid denorm problems

    return out;
  }

} // end namespace rosic

#endif // rosic_AnalogEnvelope_h
