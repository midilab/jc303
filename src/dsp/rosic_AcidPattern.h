#ifndef rosic_AcidPattern_h
#define rosic_AcidPattern_h

// rosic-indcludes:
#include "rosic_RealFunctions.h"
#include "rosic_FunctionTemplates.h"

namespace rosic
{

  /**

  This is a class for representing note-events of acid-lines involving slides and accents.

  */

  class AcidNote
  {
  public:

    int  key;
    int  octave;
    bool accent;
    bool slide;
    bool gate;

    AcidNote()
    {
      key    = 0;
      octave = 0;
      accent = false;
      slide  = false;
      gate   = false;
    }

    bool isInDefaultState()
    { return key == 0 && octave == 0 && accent == false && slide == false && gate == false; }

  };

  /**

  This is a class for representing typical acid-lines involving slides and accents.

  */

  class AcidPattern
  {

  public:

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. */
    AcidPattern();   

    //---------------------------------------------------------------------------------------------
    // setup:

    /** Sets the length of one step (the time while gate is open) in units of one step (which 
    is one 16th note). */
    void setStepLength(double newStepLength) { stepLength = newStepLength; }

    /** Sets the key for one of the steps (between 0...12, where 0 and 12 is a C). */
    void setKey(int step, int newKey) { notes[step].key = newKey; }

    /** Sets the octave for one of the steps (0 is the root octave between C2...B2). */
    void setOctave(int step, int newOctave) { notes[step].octave = newOctave; }

    /** Sets the accent flag for one of the steps. */
    void setAccent(int step, bool shouldBeAccented) { notes[step].accent = shouldBeAccented; }

    /** Sets the slide flag for one of the steps. */
    void setSlide(int step, bool shouldHaveSlide) { notes[step].slide = shouldHaveSlide; }

    /** Sets the gate flag for one of the steps. */
    void setGate(int step, bool shouldBeOpen) { notes[step].gate = shouldBeOpen; }

    /** Clears all notes in the pattern. */
    void clear();

    /** Randomizes all notes in the pattern. \todo: restrict possible note-values to some scales*/
    void randomize();

    /*
    void setRandomSeed(int newSeed);
    void resetRandomSeed();
    void rendomizeGates();
    void randomizeNotes(); 
    void randomizeAccents();
    void randomizeSlides();
    void randomizeOctaves(int maxOctavesUp, int maxOctavesDown);
    */

    /** Circularly shifts the whole pattern by the given number of steps. */
    void circularShift(int numStepsToShift);

    //---------------------------------------------------------------------------------------------
    // inquiry:

    /** Returns the length of one step (the time while gate is open) in units of one step (which 
    is one 16th note). */
    double getStepLength() const { return stepLength; }

    /** Returns the key for one of the steps (between 0...12, where 0 and 12 is a C). */
    int getKey(int step) const { return notes[step].key; }

    /** Returns the octave for one of the steps (0 is the root octave between C2...B2). */
    int getOctave(int step) const { return notes[step].octave; }

    /** Returns the accent flag for one of the steps. */
    bool getAccent(int step) const { return notes[step].accent; }

    /** Returns the slide flag for one of the steps. */
    bool getSlide(int step) const { return notes[step].slide; }

    /** Returns the gate flag for one of the steps. */
    bool getGate(int step) const { return notes[step].gate; }

    /** Returns the maximum number of steps. */
    static int getMaxNumSteps() { return maxNumSteps; }

    /** Returns the current number of steps. */
    int getNumSteps() const { return numSteps; }

    /** Returns true if the pattern is empty, false otherwise. */
    bool isEmpty() const;

    /** Returns a pointer to the note at the given step. */
    AcidNote* getNote(int step) { return &notes[step]; }

    //=============================================================================================

  protected:

    static const int maxNumSteps = 16;
    AcidNote notes[maxNumSteps];

    int    numSteps;         // number of steps in the pattern
    double stepLength;       // step length in step units (16th notes)

  };

} // end namespace rosic

#endif // rosic_AcidPattern_h
