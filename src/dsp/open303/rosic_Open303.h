#ifndef rosic_Open303_h
#define rosic_Open303_h

#include <climits>
#include "rosic_MidiNoteEvent.h"
#include "rosic_BlendOscillator.h"
#include "rosic_BiquadFilter.h"
#include "rosic_TeeBeeFilter.h"
#include "dfl_DiodeLadderFilter.h"
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
  // Import dfl classes
  using dfl::DiodeLadderFilter;

  /** Modulation matrix sources. */
  enum ModSource
  {
    MOD_SRC_OFF = 0,
    MOD_SRC_LFO,          // bipolar
    MOD_SRC_ENV           // filter envelope, bipolar (envelope 0..1 mapped to -1..+1)
  };

  /** Modulation matrix destinations (also the values of the lfoDestination parameter). */
  enum ModDestination
  {
    MOD_DEST_CUTOFF = 0,
    MOD_DEST_VOLUME,
    MOD_DEST_PITCH,
    MOD_DEST_RESONANCE,
    MOD_DEST_OVERDRIVE,   // applied by the host (JC303) via getLfoOverdriveMod()
    MOD_DEST_FILTER_FM
  };

  static const int NUM_MOD_SLOTS = 4;

  /** Filter types available for selection. */
  enum FilterType
  {
    FILTER_TEEBEE = 0,      // Original TB-303 transistor ladder
    FILTER_DIODE_OCTAVE,    // Diode ladder with 1st pole one octave above (~18dB/oct)
    FILTER_DIODE,           // Diode ladder (4-pole, 24dB/oct)
    FILTER_DIODE_BP,        // Diode ladder, bandpass response (12/12 dB/oct)
    FILTER_DIODE_HP,        // Diode ladder, highpass response (24 dB/oct)
    NUM_FILTER_TYPES
  };

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
    void setResonance(double newResonance);

    /** Sets the filter type. */
    void setFilterType(FilterType newType);

    /** Sets the filter input drive in decibels. Pushes the signal into the
    diode ladder's saturating nonlinearity for overdrive character. The TeeBee
    filter is linear in TB_303 mode, so this only affects the diode models. */
    void setFilterDrive(double newDriveDb);

    /** Returns the current filter type. */
    FilterType getFilterType() const { return currentFilterType; }

    /** Sets the diode filter's passband (bass) compensation, 0..1. Boosts the
    low end to offset the thinning that the diode ladder exhibits at high
    resonance. Only affects the diode filter models. */
    void setPassbandCompensation(double newCompensation);

    /** Sets the Filter FM depth (0-1). Devilfish-style audio-rate cutoff modulation.
     *  Uses AC-coupled input to modulate filter cutoff frequency. */
    void setFilterFmDepth(double depth) { baseFilterFmDepth = depth; diodeFilter.setFilterFmDepth(depth); }

    /** Returns the Filter FM depth. */
    double getFilterFmDepth() const { return baseFilterFmDepth; }

    /** Current LFO offset for the overdrive dry/wet mix (+/-0.5 at full depth, 0 when the
        LFO is off or aimed elsewhere). Read once per block by the host. */
    double getLfoOverdriveMod() const { return lfoOverdriveMod; }

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

    /** Sets the LFO depth (0.0 to 1.0, unipolar). */
    void setLfoDepth(double depth) { lfoDepth = depth; }

    /** Sets the LFO destination (volume, cutoff). */
    void setLfoDestination(double dest) { lfoDestination = dest; }

    /** Configures matrix slot 1..3. Slot 0 is the LFO slot driven by setLfoDepth and
        setLfoDestination; slot 1 is hardwired to the envelope source (its source argument is
        ignored). The amount is bipolar (-1..+1) and scaled per destination. */
    void setModSlot(int slot, int source, int destination, double amount)
    {
      if( slot >= 1 && slot < NUM_MOD_SLOTS )
        modSlots[slot] = ModSlot{ source, destination, amount };
    }

    /** Enables/disables LFO processing (master on/off). */
    void setLfoOn(bool on) { lfoEnabled = on; }

    /** Enables/disables resetting the LFO phase on every (non-slid) note trigger. */
    void setLfoKeySync(bool on) { lfoKeySync = on; }

    /** Sets the LFO start phase used by key-sync (0.0 to 1.0 = 0..360 degrees). */
    void setLfoPhase(double phase) { lfoPhase = phase; }

    /** Sets the LFO contour (-1.0 to +1.0): lag/slew or sag/bulge depending on waveform. */
    void setLfoContour(double contour) { lfo.setContour(contour); }
    bool getLfoOn() const { return lfoEnabled; }

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
    DiodeLadderFilter         diodeFilter;
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

    struct ModSlot
    {
      int    source;
      int    destination;
      double amount;
    };

    /** Sums the effect of one matrix slot (value = source output) into the accumulators. */
    struct ModAccum
    {
      double cutoffOctaves = 0.0;
      double volumeFactor  = 1.0;
      double pitchSemis    = 0.0;
      double resonance     = 0.0;   // percentage points
      double filterFm      = 0.0;
      double overdrive     = 0.0;
      bool   pitchOn = false, resonanceOn = false, filterFmOn = false;

      void add(int destination, double value, double amount)
      {
        switch( destination )
        {
        case MOD_DEST_CUTOFF:    cutoffOctaves += value * amount * 2.0;  break;   // +/- 2 octaves
        case MOD_DEST_VOLUME:    volumeFactor  *= 1.0 - fabs(amount) + value * amount;  break;
        case MOD_DEST_PITCH:     pitchSemis    += value * amount * 12.0; pitchOn = true;  break;
        case MOD_DEST_RESONANCE: resonance     += value * amount * 50.0; resonanceOn = true;  break;
        case MOD_DEST_OVERDRIVE: overdrive     += value * amount * 0.5;  break;
        case MOD_DEST_FILTER_FM: filterFm      += value * amount * 0.5;  filterFmOn = true;  break;
        }
      }
    };

    /** Pushes a resonance percentage (0..100) to both filters. */
    void applyResonance(double percent);

    /** Triggers a note (called either directly in noteOn or in getSample when the sequencer is
    used). */
    void triggerNote(int noteNumber, bool hasAccent);

    /** Slides to a note (called either directly in noteOn or in getSample when the sequencer is
    used). */
    void slideToNote(int noteNumber, bool hasAccent);

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
    bool   idle;             // flag to indicate that we have currently nothing to do in getSample
    FilterType currentFilterType;  // currently selected filter type

    // LFO modulation depth
    double lfoDepth;    // LFO depth (0.0 to 1.0)
    ModSlot modSlots[NUM_MOD_SLOTS] = { {MOD_SRC_OFF, 0, 0.0}, {MOD_SRC_OFF, 0, 0.0},
                                        {MOD_SRC_OFF, 0, 0.0}, {MOD_SRC_OFF, 0, 0.0} };
    double lfoPhase = 0.0;      // LFO start phase for key-sync (0.0 to 1.0)
    bool lfoKeySync = false;    // reset LFO phase on note trigger
    double baseResonance = 0.0;       // knob value (percent), LFO offsets are applied on top
    double baseFilterFmDepth = 0.0;   // knob value, LFO offsets are applied on top
    double lfoOverdriveMod = 0.0;
    bool resonanceModActive = false;  // true while the LFO is moving resonance / FM, so that
    bool fmModActive = false;         // the knob value can be restored when it stops
    int lfoDestination;   // LFO destination (0=filter cutoff, 1=volume, 2=pitch)
    bool lfoEnabled = false;  // master LFO processing switch

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

          if( !slideToNextNote )
            triggerNote(key, note->accent);
          else
            slideToNote(key, note->accent);

          AcidNote* nextNote = sequencer.getNextScheduledNote();
          if( note->slide && nextNote->gate == true )
          {
            noteOffCountDown = INT_MAX;
            slideToNextNote  = true;
          }
          else
          {
            noteOffCountDown = sequencer.getStepLengthInSamples();
            slideToNextNote  = false;
          }
        }
      }
    }

    // filter envelope, shared by the cutoff path and the modulation matrix
    double mainEnvOut = mainEnv.getSample();
    double rc1Out     = n1 * rc1.getSample(mainEnvOut);
    double rc2Out     = n2 * rc2.getSample(accentGain > 0.0 ? mainEnvOut : 0.0);
    double envSource  = rc1Out + accentGain * rc2Out;

    // modulation matrix: slot 0 is the LFO slot (lfoDepth / lfoDestination), slot 1 the envelope
    // slot, 2..3 are free
    ModAccum mod;
    if( lfoEnabled )
    {
      bool   lfoSampled = false;
      double lfoValue   = 0.0;
      for(int i = 0; i < NUM_MOD_SLOTS; i++)
      {
        int    source      = modSlots[i].source;
        int    destination = modSlots[i].destination;
        double amount      = modSlots[i].amount;
        if( i == 0 )
        {
          source      = MOD_SRC_LFO;
          destination = (int) lfoDestination;
          amount      = lfoDepth;
        }
        else if( i == 1 )
          source      = MOD_SRC_ENV;
        if( amount == 0.0 || source == MOD_SRC_OFF )
          continue;

        double value = 2.0 * envSource - 1.0;
        if( source == MOD_SRC_LFO )
        {
          if( !lfoSampled )
          {
            lfoValue   = lfo.getSample() * 2.0 - 1.0;
            lfoSampled = true;
          }
          value = lfoValue;
        }
        mod.add(destination, value, amount);
      }
    }

    lfoOverdriveMod = mod.overdrive;
    if( mod.resonanceOn || resonanceModActive )
    {
      applyResonance(std::max(0.0, std::min(100.0, baseResonance + mod.resonance)));
      resonanceModActive = mod.resonanceOn;
    }
    if( mod.filterFmOn || fmModActive )
    {
      diodeFilter.setFilterFmDepth(baseFilterFmDepth + mod.filterFm);   // clamped by the filter
      fmModActive = mod.filterFmOn;
    }

    // calculate instantaneous oscillator frequency and set up the oscillator:
    // Apply pitch modulation AFTER slew limiter to prevent smoothing of audio-rate LFO
    double pitchModFactor = mod.pitchOn ? pow(2.0, mod.pitchSemis / 12.0) : 1.0;
    double instFreq = pitchSlewLimiter.getSample(oscFreq) * pitchModFactor;
    oscillator.setFrequency(instFreq*pitchWheelFactor);
    oscillator.calculateIncrement();

    // calculate instantaneous cutoff frequency from the nominal cutoff and all its modifiers and
    // set up the filter:
    double tmp1       = envScaler * ( rc1Out - envOffset );  // seems not to work yet
    double tmp2       = accentGain * rc2Out;
    double instCutoff = cutoff * pow(2.0, tmp1+tmp2+mod.cutoffOctaves);
    filter.setCutoff(instCutoff);
    diodeFilter.setCutoff(instCutoff);

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
      // Apply selected filter
      if(currentFilterType == FILTER_TEEBEE)
        tmp = filter.getSample(tmp);
      else
        tmp = diodeFilter.getSample(tmp);
      tmp  = antiAliasFilter.getSample(tmp);  // anti-aliasing filtered

    }

    // these filters may actually operate without oversampling (but only if we reset them in
    // triggerNote - avoid clicks)
    tmp  = allpass.getSample(tmp);
    tmp  = highpass2.getSample(tmp);
    tmp = notch.getSample(tmp);
    tmp *= ampEnvOut;                       // amplified
    tmp *= ampScaler;
    tmp *= mod.volumeFactor;                // matrix volume modulation

    // find out whether we may switch ourselves off for the next call:
    idle = false;
    //idle = (sequencer.getSequencerMode() == AcidSequencer::OFF && ampEnv.endIsReached()
    //        && fabs(tmp) < 0.000001); // ampEnvOut < 0.000001;

    return tmp;
  }

}

#endif
