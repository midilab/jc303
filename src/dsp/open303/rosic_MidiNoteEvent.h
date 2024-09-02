#ifndef rosic_MidiNoteEvent_h
#define rosic_MidiNoteEvent_h

namespace rosic
{

  /**

  This is a class for representing MIDI note-events.

  */

  class MidiNoteEvent
  {

  public:

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Default constructor. */
    MidiNoteEvent();

    /** Constructor with initializations. */
    MidiNoteEvent(int initKey, int initVel, int initDetune = 0, int initPriority = 0 );

    /** Destructor. */
    ~MidiNoteEvent();

    //---------------------------------------------------------------------------------------------
    // parameter settings:

    /** Sets the key of the note (as MIDI note number between 0...127). */
    void setKey(int newKey);

    /** Sets the velocity of the note (between 0...127). */
    void setVelocity(int newVelocity);

    /** Sets the detuning of the note (in semitones). */
    void setDetune(double newDetune);

    /** Sets the priority of the note. */
    void setPriority(int newPriority);

    //---------------------------------------------------------------------------------------------
    // inquiry:

    /** Returns the key of the note (as MIDI note number between 0...127). */
    int getKey() const { return key; }

    /** Returns the velocity of the note (between 0...127). */
    int getVelocity() const { return vel; }

    /** Returns the detuning of the note (in semitones). */
    double getDetune() const { return detune; }

    /** Returns the priority of the note. */
    int getPriority() const { return priority; }

    //---------------------------------------------------------------------------------------------
    // overloaded operators:

    /** Note events are interpreted as equal if the have the same key. */
    bool operator==(const MidiNoteEvent& note2) const
    {
      if( note2.key == key )
        return true;
      else
        return false;
    }

  protected:

    int    key;       // key of the note in the range 0...127
    int    vel;       // velocity of the note in the range 0...127
    double detune;    // detuning in cents (for microtuning)
    int    priority;  // a priority value

  };

} // end namespace rosic

#endif // MidiNoteEvent_h
