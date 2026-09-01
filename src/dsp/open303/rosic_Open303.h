#ifndef rosic_Open303_h
#define rosic_Open303_h

#include <climits>
#include "rosic_MidiNoteEvent.h"
#include "rosic_BlendOscillator.h"
#include "rosic_BiquadFilter.h"
#include "rosic_TeeBeeFilter.h"
#include "rosic_AnalogEnvelope.h"
#include "rosic_DecayEnvelope.h"
#include "rosic_LeakyIntegrator.h"
#include "rosic_EllipticQuarterBandFilter.h"
#include "rosic_AcidSequencer.h"
#include "dfl_LFO.h"

#include <list>

using namespace std; // for the noteList

namespace rosic
{

  /**

  This is a monophonic bass-synth that aims to emulate the sound of the famous Roland TB 303 and
  goes a bit beyond.

  */

  class Open303
  {

  public:

    //-----------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. */
    Open303();

    /** Destructor. */
    ~Open303();

    //-----------------------------------------------------------------------------------------------
    // parameter settings:

    /** Sets the sample-rate (in Hz). */
    void setSampleRate(double newSampleRate);

    /** Sets up the waveform continuously between saw and square - the input should be in the range
    0...1 where 0 means pure saw and 1 means pure square. */
    void setWaveform(double newWaveform) { oscillator.setBlendFactor(newWaveform); }

    /** Sets the master tuning frequency for note A4 (usually 440 Hz). */
    void setTuning(double newTuning) { tuning = newTuning; }

    /** Sets the filter's nominal cutoff frequency (in Hz). */
    void setCutoff(double newCutoff);

    /** Sets the resonance amount for the filter. */
    void setResonance(double newResonance) { filter.setResonance(newResonance); }

    /** Sets the modulation depth of the filter's cutoff frequency by the filter-envelope generator
    (in percent). */
    void setEnvMod(double newEnvMod);

    /** Sets the main envelope's decay time for non-accented notes (in milliseconds).
    Devil Fish provides range of 30...3000 ms for this parameter. On the normal 303, this
    parameter had a range of 200...2000 ms.  */
    void setDecay(double newDecay) { normalDecay = newDecay; }

    /** Sets the accent (in percent).  */
    void setAccent(double newAccent);

    /** Sets the master volume level (in dB). */
    void setVolume(double newVolume);

    //  from here: parameter settings which were not available to the user in the 303:

    /** Sets the amplitudes envelope's sustain level in decibels. Devil Fish uses the second half
    of the range of the (amplitude) decay pot for this and lets the user adjust it between 0
    and 100% of the full volume. In the normal 303, this parameter was fixed to zero. */
    void setAmpSustain(double newAmpSustain) { ampEnv.setSustainInDecibels(newAmpSustain); }

    /** Sets the drive (in dB) for the tanh-shaper for 303-square waveform - internal parameter, to
    be scrapped eventually. */
    void setTanhShaperDrive(double newDrive)
    { waveTable2.setTanhShaperDriveFor303Square(newDrive); }

    /** Sets the offset (as raw value for the tanh-shaper for 303-square waveform - internal
    parameter, to be scrapped eventually. */
    void setTanhShaperOffset(double newOffset)
    { waveTable2.setTanhShaperOffsetFor303Square(newOffset); }

    /** Sets the cutoff frequency for the highpass before the main filter. */
    void setPreFilterHighpass(double newCutoff) { highpass1.setCutoff(newCutoff); }

    /** Sets the cutoff frequency for the highpass inside the feedback loop of the main filter. */
    void setFeedbackHighpass(double newCutoff) { filter.setFeedbackHighpassCutoff(newCutoff); }

    /** Sets the cutoff frequency for the highpass after the main filter. */
    void setPostFilterHighpass(double newCutoff) { highpass2.setCutoff(newCutoff); }

    /** Sets the phase shift of tanh-shaped square wave with respect to the saw-wave (in degrees)
    - this is important when the two are mixed. */
    void setSquarePhaseShift(double newShift) { waveTable2.set303SquarePhaseShift(newShift); }

    /** Sets the slide-time (in ms). The TB-303 had a slide time of 60 ms. */
    void setSlideTime(double newSlideTime);

    /** Sets the filter envelope's attack time for non-accented notes (in milliseconds).
    Devil Fish provides range of 0.3...30 ms for this parameter. */
    void setNormalAttack(double newNormalAttack)
    {
      normalAttack = newNormalAttack;
      rc1.setTimeConstant(normalAttack);
    }

    /** Sets the filter envelope's attack time for accented notes (in milliseconds). In the
    Devil Fish, accented notes have a fixed attack time of 3 ms.  */
    void setAccentAttack(double newAccentAttack)
    {
      accentAttack = newAccentAttack;
      rc2.setTimeConstant(accentAttack);
    }

    /** Sets the filter envelope's decay time for accented notes (in milliseconds).
    Devil Fish provides range of 30...3000 ms for this parameter. On the normal 303, this
    parameter was fixed to 200 ms.  */
    void setAccentDecay(double newAccentDecay) { accentDecay = newAccentDecay; }

    /** Sets the amplitudes envelope's decay time (in milliseconds). Devil Fish provides range of
    16...3000 ms for this parameter. On the normal 303, this parameter was fixed to
    approximately 3-4 seconds.  */
    void setAmpDecay(double newAmpDecay) { ampEnv.setDecay(newAmpDecay); }

    /** Sets the amplitudes envelope's release time (in milliseconds). On the normal 303, this
    parameter was fixed to .....  */
    void setAmpRelease(double newAmpRelease)
    {
      normalAmpRelease = newAmpRelease;
      ampEnv.setRelease(newAmpRelease);
    }

    // LFO parameter settings:

    /** Sets the LFO waveform (0=Triangle, 1=Saw Up, 2=Saw Down, 3=Square, 4=Random, 5=Pink Noise). */
    void setLfoWaveform(int waveform) { lfo.setWaveform(waveform); }

    /** Sets the LFO rate in Hz (0.1 to 1000.0). */
    void setLfoRate(double rate) { lfo.setRate(rate); }

    /** Sets the LFO depth (-1.0 to +1.0). */
    void setLfoDepth(double depth) { lfoDepth = depth; }

    /** Sets the LFO destination (volume, cutoff). */
    void setLfoDestination(double dest) { lfoDestination = dest; }

    //-----------------------------------------------------------------------------------------------
    // inquiry:

    /** Returns the waveform as a continuous value between 0...1 where 0 means pure saw and 1 means
    pure square. */
    double getWaveform() const { return oscillator.getBlendFactor(); }

    /** Sets the master tuning frequency for note A4 (usually 440 Hz). */
    double getTuning() const { return tuning; }

    /** Returns the filter's nominal cutoff frequency (in Hz). */
    double getCutoff() const { return cutoff; }

    /** Returns the filter's resonance amount (in percent) */
    double getResonance() const { return filter.getResonance(); }

    /** Returns the modulation depth of the filter's cutoff frequency by the filter-envelope
    generator (in percent). */
    double getEnvMod() const { return envMod; }

    /** Returns the filter envelope's decay time for non-accented notes (in milliseconds). */
    double getDecay() const { return normalDecay; }

    /** Returns the accent (in percent). */
    double getAccent() const { return 100.0 * accent; }

    /** Returns the master volume level (in dB). */
    double getVolume() const { return level; }

    //  from here: parameters which were not available to the user in the 303:

    /** Returns the amplitudes envelope's sustain level (in dB). */
    double getAmpSustain() const { return amp2dB(ampEnv.getSustain()); }

    /** Returns the drive (in dB) for the tanh-shaper for 303-square waveform - internal parameter,
    to be scrapped eventually. */
    double getTanhShaperDrive() const
    { return waveTable2.getTanhShaperDriveFor303Square(); }

    /** Returns the offset (as raw value for the tanh-shaper for 303-square waveform - internal
    parameter, to be scrapped eventually. */
    double getTanhShaperOffset() const
    { return waveTable2.getTanhShaperOffsetFor303Square(); }

    /** Returns the cutoff frequency for the highpass before the main filter. */
    double getPreFilterHighpass() const { return highpass1.getCutoff(); }

    /** Retruns the cutoff frequency for the highpass inside the feedback loop of the main
    filter. */
    double getFeedbackHighpass() const { return filter.getFeedbackHighpassCutoff(); }

    /** Returns the cutoff frequency for the highpass after the main filter. */
    double getPostFilterHighpass() const { return highpass2.getCutoff(); }

    /** Returns the phase shift of tanh-shaped square wave with respect to the saw-wave (in degrees)
    - this is important when the two are mixed. */
    double getSquarePhaseShift() const { return waveTable2.get303SquarePhaseShift(); }

    /** Returns the slide-time (in ms). */
    double getSlideTime() const { return slideTime; }

    /** Returns the filter envelope's attack time for non-accented notes (in milliseconds). */
    double getNormalAttack() const { return normalAttack; }

    /** Returns the filter envelope's attack time for non-accented notes (in milliseconds). */
    double getAccentAttack() const { return accentAttack; }

    /** Returns the filter envelope's decay time for non-accented notes (in milliseconds). */
    double getAccentDecay() const { return accentDecay; }

    /** Returns the amplitudes envelope's decay time (in milliseconds). */
    double getAmpDecay() const { return ampEnv.getDecay(); }

    /** Returns the amplitudes envelope's release time (in milliseconds). */
    double getAmpRelease() const { return normalAmpRelease; }

    //-----------------------------------------------------------------------------------------------
    // audio processing:

    /** Calculates onse output sample at a time. */
    INLINE double getSample();

    //-----------------------------------------------------------------------------------------------
    // event handling:

    /** Accepts note-on events (note offs are also handled here as note ons with velocity zero). */
    void noteOn(int noteNumber, int velocity, double detune);

    /** Turns all possibly running notes off. */
    void allNotesOff();

    /** Sets the pitchbend value in semitones. */
    void setPitchBend(double newPitchBend);

    //-----------------------------------------------------------------------------------------------
    // embedded objects:

    MipMappedWaveTable        waveTable1, waveTable2;
    BlendOscillator           oscillator;
    TeeBeeFilter              filter;
    AnalogEnvelope            ampEnv;
    DecayEnvelope             mainEnv;
    LeakyIntegrator           pitchSlewLimiter;
    //LeakyIntegrator           ampDeClicker;
    BiquadFilter              ampDeClicker;
    LeakyIntegrator           rc1, rc2;
    OnePoleFilter             highpass1, highpass2, allpass;
    BiquadFilter              notch;
    EllipticQuarterBandFilter antiAliasFilter;
    AcidSequencer             sequencer;
    dfl::LFO                  lfo;

  protected:

    /** Triggers a note (called either directly in noteOn or in getSample when the sequencer is
    used). */
    void triggerNote(int noteNumber, bool hasAccent);

    /** Slides to a note (called either directly in noteOn or in getSample when the sequencer is
    used). */
    void slideToNote(int noteNumber, bool hasAccent);

    /** Hammers to a note - legato with instant pitch change, no envelope retrigger
    (TT-303 extension, called in getSample when the sequencer is used). */
    void hammerToNote(int noteNumber, bool hasAccent);

    /** Releases a note (called either directly in noteOn or in getSample when the sequencer is
    used). */
    void releaseNote(int noteNumber);

    /** Sets the decay-time of the main envelope and updates the normalizers n1, n2 accordingly. */
    void setMainEnvDecay(double newDecay);

    void calculateEnvModScalerAndOffset();

    /** Updates the normalizer n1 according to the time-constant of rc1 and the decay-time of the
    main envelope generator. */
    void updateNormalizer1();

    /** Updates the normalizer n2 according to the time-constant of rc2 and the decay-time of the
    main envelope generator. */
    void updateNormalizer2();

    static const int oversampling = 4;

    double tuning;           // master tunung for A4 in Hz
    double ampScaler;        // final volume as raw factor
    double oscFreq;          // frequecy of the oscillator (without pitchbend)
    double sampleRate;       // the (non-oversampled) sample rate
    double level;            // master volume level (in dB)
    double levelByVel;       // velocity dependence of the level (in dB)
    double accent;           // scales all "byVel" parameters
    double slideTime;        // the time to slide from one note to another (in ms)
    double cutoff;           // nominal cutoff frequency of the filter
    double envMod;           // strength of the envelope modulation in percent
    double envUpFraction;    // fraction of the envelope that goes upward
    double envOffset;        // offset for the normalized envelope ('bipolarity' parameter)
    double envScaler;        // scale-factor for the normalized envelope (derived from envMod)
    double normalAttack;     // attack time for the filter envelope on non-accented notes
    double accentAttack;     // attack time for the filter envelope on accented notes
    double normalDecay;      // decay time for the filter envelope on non-accented notes
    double accentDecay;      // decay time for the filter envelope on accented notes
    double normalAmpRelease; // amp-env release time for non-accented notes
    double accentAmpRelease; // amp-env release time for accented notes
    double accentGain;       // between 0.0...1.0 - to scale the 3rd amp-envelope on accents
    double pitchWheelFactor; // scale factor for oscillator frequency from pitch-wheel
    double n1, n2;           // normalizers for the RCs that are driven by the MEG
    int    currentNote;      // note which is currently played (-1 if none)
    int    currentVel;       // velocity of currently played note
    int    noteOffCountDown; // a countdown variable till next note-off in sequencer mode
    bool   slideToNextNote;  // indicate that we need to slide to the next note in sequencer mode
    bool   hammerToNextNote; // indicate that we need to hammer (legato w/ instant pitch) to the next note
    bool   idle;             // flag to indicate that we have currently nothing to do in getSample
    bool   currentNoteMuted; // flag indicating the current note is muted (shorter gate, darker, quieter)

    // TT-303 mute parameters
    double muteGateFactor;   // gate length multiplier for muted notes (0.4-0.6)
    double muteLevelFactor;  // VCA level factor for muted notes (0.4-0.7)
    double muteCutoffFactor; // filter cutoff factor for muted notes (0.6-0.9)
    double muteEnvFactor;    // envelope mod factor for muted notes (0.6-0.9)
    double muteMorph;        // smoothed 0..1 muted amount for click-free mute transitions
    double muteMorphCoeff;   // one-pole coefficient for ~2ms mute smoothing

    // LFO modulation depth
    double lfoDepth;    // LFO depth (-1.0 to +1.0)
    int lfoDestination;   // LFO destination (0=filter cutoff, 1=volume, 2=pitch)

    list<MidiNoteEvent> noteList;

  };

  //-------------------------------------------------------------------------------------------------
  // inlined functions:

  INLINE double Open303::getSample()
  {
    //if( sequencer.getSequencerMode() == AcidSequencer::OFF && ampEnv.endIsReached() )
    //  return 0.0;
    if( idle )
      return 0.0;

    // check the sequencer if we have some note to trigger:
    if( sequencer.getSequencerMode() != AcidSequencer::OFF )
    {
      noteOffCountDown--;
      if( noteOffCountDown == 0 || sequencer.isRunning() == false )
        releaseNote(currentNote);

      AcidNote *note = sequencer.getNote();
      if( note != NULL )
      {
        if( note->gate == true && currentNote != -1)
        {
          int key = note->key + 12*note->octave + currentNote;
          key = clip(key, 0, 127);

          // Determine accent: mute overrides accent (muted notes are never accented)
          bool hasAccent = note->accent && !note->mute;

          // Handle note triggering based on previous step's legato state
          if( !slideToNextNote && !hammerToNextNote )
            triggerNote(key, hasAccent);
          else if( hammerToNextNote )
            hammerToNote(key, hasAccent);  // Instant pitch, no envelope retrigger
          else
            slideToNote(key, hasAccent);   // Glide pitch, no envelope retrigger

          // Track if this note is muted (for cutoff/level reduction in audio processing)
          currentNoteMuted = note->mute;

          // Determine legato behavior for next step transition
          AcidNote* nextNote = sequencer.getNextScheduledNote();
          bool nextHasGate = nextNote->gate == true;

          // slide and hammer both create legato (continuous gate, no env retrigger)
          // slide = smooth pitch glide, hammer = instant pitch change
          if( (note->slide || note->hammer) && nextHasGate )
          {
            noteOffCountDown = INT_MAX;  // Keep gate open
            slideToNextNote  = note->slide && !note->hammer;  // slide takes precedence only if no hammer
            hammerToNextNote = note->hammer;
          }
          else
          {
            // Calculate gate length - muted notes have shorter gate
            int gateLength = sequencer.getStepLengthInSamples();
            if( note->mute )
              gateLength = (int)(gateLength * muteGateFactor);
            noteOffCountDown = gateLength;
            slideToNextNote  = false;
            hammerToNextNote = false;
          }
        }
      }
    }

    // LFO modulation - only process if lfoDepth is greater than zero
    double lfoFilterMod = 0.0;
    double volumeModFactor = 1.0;
    double pitchModFactor = 1.0;

    if (lfoDepth > 0.0)
    {
      // Get LFO output (0.0 to +1.0 unipolar, convert to bipolar for modulation)
      double lfoValue = lfo.getSample() * 2.0 - 1.0;  // Convert unipolar to bipolar

      switch (lfoDestination) {
        case 0:
            // Apply LFO filter modulation (convert to bipolar, in octaves)
            // linToLin(lfoDepth, 0.0, 1.0, -1.0, 1.0) == lfoDepth * 2 - 1
            lfoFilterMod = lfoValue * (lfoDepth * 2 - 1) * 2.0;  // +/- 2 octaves max
            break;
        case 1:
            // Apply LFO volume modulation - tremolo (convert to linear amplitude multiplier)
            volumeModFactor = 1.0 - lfoDepth + (lfoValue * lfoDepth);
            break;
        case 2:
            // Apply LFO pitch modulation (in semitones, converted to frequency multiplier -12 to +12 semitones)
            // linToLin(lfoDepth, 0.0, 1.0, -12.0, 12.0) == lfoDepth * 24 - 12
            double semitones = lfoValue * (lfoDepth * 24 - 12);
            pitchModFactor = pow(2.0, semitones / 12.0);
            break;
      }
    }

    // calculate instantaneous oscillator frequency and set up the oscillator:
    // Apply pitch modulation AFTER slew limiter to prevent smoothing of audio-rate LFO
    double instFreq = pitchSlewLimiter.getSample(oscFreq) * pitchModFactor;
    oscillator.setFrequency(instFreq*pitchWheelFactor);
    oscillator.calculateIncrement();

    // calculate instantaneous cutoff frequency from the nominal cutoff and all its modifiers and
    // set up the filter:
    double mainEnvOut = mainEnv.getSample();
    double tmp1       = n1 * rc1.getSample(mainEnvOut);
    double tmp2       = 0.0;
    if( accentGain > 0.0 )
      tmp2 = mainEnvOut;
    tmp2 = n2 * rc2.getSample(tmp2);
    tmp1 = envScaler * ( tmp1 - envOffset );  // seems not to work yet
    tmp2 = accentGain*tmp2;

    // Smoothly morph the mute reductions (0 = open, 1 = fully muted) so that
    // level/cutoff/env changes ramp over ~2ms instead of stepping in one sample.
    // A hard switch on a held (legato hammer/slide) note produces an audible click.
    double muteTarget = currentNoteMuted ? 1.0 : 0.0;
    muteMorph = muteTarget + muteMorphCoeff * (muteMorph - muteTarget);

    // Apply mute envelope reduction (morphed)
    tmp1 *= 1.0 + muteMorph * (muteEnvFactor - 1.0);

    // Apply mute cutoff reduction (morphed, darker tone)
    double instCutoff = cutoff;
    instCutoff *= 1.0 + muteMorph * (muteCutoffFactor - 1.0);
    instCutoff *= pow(2.0, tmp1+tmp2+lfoFilterMod);
    filter.setCutoff(instCutoff);

    double ampEnvOut = ampEnv.getSample();
    //ampEnvOut += 0.45*filterEnvOut + accentGain*6.8*filterEnvOut;
    if( ampEnv.isNoteOn() )
      ampEnvOut += 0.45*mainEnvOut + accentGain*4.0*mainEnvOut;
    ampEnvOut = ampDeClicker.getSample(ampEnvOut);

    // oversampled calculations:
    double tmp;
    for(int i=1; i<=oversampling; i++)
    {
      tmp  = -oscillator.getSample();         // the raw oscillator signal
      tmp  = highpass1.getSample(tmp);        // pre-filter highpass
      tmp  = filter.getSample(tmp);           // now it's filtered
      tmp  = antiAliasFilter.getSample(tmp);  // anti-aliasing filtered

    }

    // these filters may actually operate without oversampling (but only if we reset them in
    // triggerNote - avoid clicks)
    tmp  = allpass.getSample(tmp);
    tmp  = highpass2.getSample(tmp);
    tmp = notch.getSample(tmp);
    tmp *= ampEnvOut;                       // amplified
    tmp *= ampScaler;
    tmp *= volumeModFactor;                 // LFO volume modulation

    // Apply mute level reduction (morphed, quieter tone)
    tmp *= 1.0 + muteMorph * (muteLevelFactor - 1.0);

    // find out whether we may switch ourselves off for the next call:
    idle = false;
    //idle = (sequencer.getSequencerMode() == AcidSequencer::OFF && ampEnv.endIsReached()
    //        && fabs(tmp) < 0.000001); // ampEnvOut < 0.000001;

    return tmp;
  }

}

#endif
