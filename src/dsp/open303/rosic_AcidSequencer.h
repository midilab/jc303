#ifndef rosic_AcidSequencer_h
#define rosic_AcidSequencer_h

// rosic-indcludes:
#include "rosic_AcidPattern.h"

namespace rosic
{

  /**

  This is a sequencer for typical acid-lines involving slides and accents.

  \todo: make the permissibility-thing work correctly

  */

  class AcidSequencer
  {

  public:

    enum sequencerModes
    {
      OFF = 0,
      KEY_SYNC,
      HOST_SYNC,

      NUM_SEQUENCER_MODES
    };

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. */
    AcidSequencer();   

    //---------------------------------------------------------------------------------------------
    // setup:

    /** Sets the sample-rate. */
    void setSampleRate(double newSampleRate);

    /** Sets the tempo in BPM. */
    void setTempo(double newTempoInBpm) { bpm = newTempoInBpm; }

    /** Sets the key in one of the patterns for one of the steps (between 0...11, 0 is C). */
    void setKey(int pattern, int step, int newKey);

    /** Sets the octave for one of the steps (0 is the root octave between C2...B2). */
    void setOctave(int pattern, int step, int newOctave);

    /** Sets the accent flag for one of the steps. */
    void setAccent(int pattern, int step, bool shouldBeAccented);

    /** Sets the slide flag for one of the steps. */
    void setSlide(int pattern, int step, bool shouldHaveSlide);

    /** Sets the gate flag for one of the steps. */
    void setGate(int pattern, int step, bool shouldBeOpen);

    /** Selects one of the modes for the sequencer @see sequencerModes. */
    void setMode(int newMode);

    /** Sets the length of one step (the time while gate is open) in units of one step (which 
    is one 16th note). */
    void setStepLength(double newStepLength) 
    { patterns[activePattern].setStepLength(newStepLength); }

    /** Circularly shifts the active pattern by the given number of steps. */
    void circularShift(int numSteps) { patterns[activePattern].circularShift(numSteps); }

    /** Marks a key (note value from 0...12, where 0 and 12 is a C) as permissible or not. 
    Whenever the pattern currently played requires a key that is not permissible, the sequencer
    will play the closest key among the permissible ones (it will select the lower when two 
    permissible keys are at equal distance). */
    void setKeyPermissible(int key, bool shouldBePermissible);

    /** Toggles the permissibility of a key on/off. */
    void toggleKeyPermissibility(int key);

    //---------------------------------------------------------------------------------------------
    // inquiry:

    /** Returns the number of patterns. */
    int getNumPatterns() const { return numPatterns; }

    /** Returns a pointer to the pattern with given index - NULL if index is out of range. */
    AcidPattern* getPattern(int index);

    /** Returns true when the sequencer is running, false otherwise. */
    bool isRunning() const { return running; }

    /** Returns once true, when the mode was changed due to a call to setMode. Thereafter, it will
    always return false until a new call to setMode happens - whereafter it will again return true
    once, ...and so on. The idea is that an outlying class may have to become aware of such changes
    in order to turn off running notes (trigger all-notes-off or something). */
    bool modeWasChanged();

    /** Returns the length of one step (the time while gate is open) in units of one step (which 
    is one 16th note). */
    double getStepLength() const { return patterns[activePattern].getStepLength(); }

    /** Returns the length of one step (the time while gate is open) in samples. */
    int getStepLengthInSamples() const 
    { return roundToInt(sampleRate*getStepLength()*beatsToSeconds(0.25, bpm)); }

    /** Returns the selected sequencer mode @see sequencerModes. */
    int getSequencerMode() const { return sequencerMode; }

    /** Returns, if the given key is among the permissible ones. */
    bool isKeyPermissible(int key);

    //---------------------------------------------------------------------------------------------
    // audio processing:

    /** Returns a pointer to the note that occurs at this sample if any, NULL otherwise. */
    INLINE AcidNote* getNote();

    /** Returns the next note that will be scheduled - after getNote() has returned a non-NULL 
    pointer, this will be the next non-NULL note that will be returned. So, if an event has 
    occurred at some time instant, you may investigate the next upcoming event beforehand by 
    calling this function. */
    INLINE AcidNote* getNextScheduledNote() 
    { 
      AcidNote* note = patterns[activePattern].getNote(step);
      note->key      = getClosestPermissibleKey(note->key); 
      return note;
    }

    /** Returns the key among the permissible ones which is closest to the given key - if two keys 
    are at the same distance, it returns the lower of them. If the passed key is itself 
    permissible, it will be returned unchanged. */
    INLINE int getClosestPermissibleKey(int key);

    //---------------------------------------------------------------------------------------------
    // event handling:

    /** Lets the sequencer start playing. */
    void start();

    /** Lets the sequencer stop playing. */
    void stop();

    //---------------------------------------------------------------------------------------------
    // others:

    //=============================================================================================

  protected:

    static const int numPatterns = 16;
    AcidPattern patterns[numPatterns];

    int    activePattern;      // the currently selected pattern
    bool   running;            // flag to indicate that sequencer is running
    bool   modeChanged;        // flag that is set to true in setMode and to false in modeChanged
    double sampleRate;         // the sample-rate
    double bpm;                // the tempo in bpm
    int    countDown;          // a sample-countdown - counts down for the next step to occur
    int    step;               // the current step
    int    sequencerMode;      // the selected mode for the sequencer
    double driftError;         // to keep track and compensate for accumulating timing error
    bool   keyPermissible[13]; // array of flags to indicate if a particular key is permissible

  };

  //-----------------------------------------------------------------------------------------------
  // from here: definitions of the functions to be inlined, i.e. all functions which are supposed 
  // to be called at audio-rate (they can't be put into the .cpp file):

  INLINE AcidNote* AcidSequencer::getNote()
  {
    if( running == false )
      return NULL;

    if( countDown > 0 )
    {
      countDown--;
      return NULL;
    }
    else
    {
      double secondsToNextStep = beatsToSeconds(0.25, bpm);
      double samplesToNextStep = secondsToNextStep * sampleRate;
      countDown                = roundToInt(samplesToNextStep);

      // keep track of accumulating error due to rounding and compensate when the accumulated error
      // exceeds half a sample:
      driftError += countDown - samplesToNextStep;
      if( driftError < -0.5 ) // negative errors indicate that we are too early
      {
        driftError += 1.0;
        countDown  += 1;
      }
      else if( driftError >= 0.5 )
      {
        driftError -= 1.0;
        countDown  -= 1;
      }

      AcidNote* note = patterns[activePattern].getNote(step);
      note->key      = getClosestPermissibleKey(note->key);
      step           = (step+1) % patterns[activePattern].getNumSteps();
      return note; 
    }
  }

  INLINE int AcidSequencer::getClosestPermissibleKey(int key)
  {
    if( key >= 0 && key <= 12 )
    {
      if( keyPermissible[key] )
        return key;
      else
      {
        // find the closest lower permissible key:
        int kLo = key-1;
        while( kLo >= 0 )
        {
          if( keyPermissible[kLo] )
            break;
          kLo--;
        }

        // find the closest higher permissible key:
        int kHi = key+1;
        while( kHi < 12 )
        {
          if( keyPermissible[kHi] )
            break;
          kHi++;
        }

        // select the closest (subject to the constraint that it must be between 0 and 12):
        if(      (kHi-key) <  (kLo-key) && kHi <= 12 )
          return kHi;
        else if( (kLo-key) <  (kHi-key) && kLo >= 0  )
          return kLo;
        else if( (kHi-key) == (kLo-key) && kLo >= 0  )
          return kLo;
        else return -1; // none of the keys is permissible
      }
    }
    else
      return 0;
  }

} // end namespace rosic

#endif // rosic_AcidSequencer_h
