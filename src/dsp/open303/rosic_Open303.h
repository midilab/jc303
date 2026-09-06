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

    /** Sets the reverse-gate depth (0...1). At 0 the note plays normally. As it approaches 1, the
    amplitude, filter and accent contours are progressively flipped in time, so the note swells up
    from silence and the filter opens toward the note's end (then cuts on the next trigger) - it
    sounds like the note is played backwards. The swell duration follows the Decay setting (the main
    envelope's time constant), so it needs no separate time control. */
    void setReverseGate(double newReverseGate) { reverseGate = clip(newReverseGate, 0.0, 1.0); }

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

    /** Returns the reverse-gate depth (0...1). */
    double getReverseGate() const { return reverseGate; }

    /** Sets the host tempo (BPM) so the predicted reverse-swell length can be snapped to the musical
    16th-note grid. Pass 0 (or a non-positive value) when no tempo is available. */
    void setReverseTempo(double bpm) { reverseTempoBpm = (bpm > 0.0) ? bpm : 0.0; }

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

  protected:

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

    /** Recomputes the reverse-swell shape (reverseShape) from the current reverseLength and the
    amp-envelope decay, so the swell stays a faithful reversal as the predicted length changes. */
    void updateReverseShape();

    /** Predicts the current note's length (in samples) for the reverse swell: a short-biased low
    percentile of recent measured note lengths, optionally snapped to the host's 16th-note grid. */
    double predictReverseLength();

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
    double reverseGate;      // 0...1 depth for the reverse-gate effect (see setReverseGate)
    double reversePhase;     // samples elapsed since the note trigger (drives the reverse swell)
    double reverseLength;    // predicted note length in samples; the reverse swell peaks here
    double reverseMeasured;  // duration of the previous note/group, used to predict reverseLength
    double reverseLastOnPhase; // reversePhase at the most recent note-on within the current group
    double reverseStep;      // samples between consecutive note-ons in the current tied group
    double reverseShape;     // back-loadedness of the reverse swell, auto-set in updateReverseShape()
    double reverseShapeMain; // back-loadedness of the reversed filter/accent sweep, derived from
                             // the main (filter) envelope's decay in updateReverseShape()
    double reverseDen;       // e^reverseShape - 1, cached normalizer for the swell
    double reverseFreqFrom;  // pitch the reversed portamento glides from (Hz)
    double reverseLogRatio;  // ln(targetFreq / reverseFreqFrom); 0 for non-slide notes
    double reverseGlideBase; // swell value when the current glide started (re-anchor, jump-free slides)
    double reverseInstFreq;  // last instantaneous oscillator pitch (Hz), for glide continuity
    double reverseSwellNow;  // most recent swell value, read by slideToNote to re-anchor the glide
    double reverseHist[8];   // ring buffer of recent measured note lengths (samples), for prediction
    int    reverseHistPos;   // write index into reverseHist
    int    reverseHistCount; // number of valid entries in reverseHist (<= 8)
    double reverseTempoBpm;  // host tempo in BPM (0 = unknown); snaps prediction to the 16th grid
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

    // --- reverse-gate: synthesize a genuinely time-reversed contour -------------------------------
    // Playing a note backwards means the amplitude/filter grow as a *back-loaded* exponential: they
    // stay quiet for most of the note, then swell rapidly to a peak right at the end, where the
    // original attack transient becomes an abrupt cut. We drive that swell from a phase counter that
    // reaches 1 at 'reverseLength' samples after the trigger - which triggerNote() sets to the note's
    // (predicted) length - so the peak lands exactly at the note's end. reverseShape sets how
    // back-loaded the curve is; reverseDen == e^reverseShape - 1 normalizes it to 0..1. Computed
    // before the oscillator frequency because in reverse mode the swell also shapes the pitch glide.
    double mainEnvFwd = mainEnv.getSample();  // forward main envelope: decays 1 -> 0
    double rPos  = reversePhase / reverseLength;                  // 0..1 over the note, then clamped
    if( rPos > 1.0 ) rPos = 1.0;
    // The swell completes slightly early (at reverseHoldAt of the predicted length) and then HOLDS
    // at the peak until the cut. This concentrates energy at full level right before the end-cut
    // (punch), and protects against over-predicted note lengths starving the note of its peak.
    const double reverseHoldAt = 0.85;
    double rSw = rPos * (1.0/reverseHoldAt);
    if( rSw > 1.0 ) rSw = 1.0;
    // Amplitude swell: the exact time-reverse of the forward decay e^(-t/tau) over a note of
    // length L is e^(K*(r-1)) with K = L/tau - a back-loaded exponential with a *floor* of e^-K at
    // the head, NOT silence. Using this floored form (instead of the 0-normalized
    // (e^(K*r)-1)/(e^K-1)) preserves the forward note's RMS by mirror symmetry, so reverse mode is
    // as loud as forward, and guarantees every gated note is audible even when the predicted
    // reverseLength overshoots the actual note (no more "skipped" short notes).
    double swellAmp  = exp(reverseShape*(rSw - 1.0));
    // Filter/accent swell: same floored form, but shaped by the *main* (filter) envelope's decay
    // (reverseShapeMain), so the reversed cutoff sweep mirrors what the Decay knob does forwards
    // and opens fully into the peak.
    double swellMain = exp(reverseShapeMain*(rSw - 1.0));
    // filter-sweep floor: keep the reversed cutoff from starting fully closed. For a full-length note
    // this only lifts the quiet head (masked by the low amplitude there); for a note cut short by an
    // over-prediction it keeps the audible portion from sounding dull - self-proportional via the amp
    // gate. Costs a little sweep drama; set to 0 for the purest closed->open sweep.
    const double reverseFilterFloor = 0.15;
    if( swellMain < reverseFilterFloor ) swellMain = reverseFilterFloor;
    // 0-based normalized swell, kept as the pitch-glide driver (glide anchoring expects 0..1):
    double swell = (exp(reverseShape*rSw) - 1.0) / reverseDen;
    reversePhase += 1.0;
    reverseSwellNow = swell;   // slideToNote() reads this to re-anchor the pitch glide
    // gate the amplitude swell to the held note so it cuts at note-off / rests instead of holding:
    double revAmp = ampEnv.isNoteOn() ? swellAmp : 0.0;

    // calculate instantaneous oscillator frequency and set up the oscillator:
    double instFreq = pitchSlewLimiter.getSample(oscFreq);
    if( reverseGate > 0.0 )
    {
      // inverse portamento: the forward slew glides old->new at the note's silent head, so in reverse
      // mode it is inaudible. Instead glide from the current pitch to the target along the remaining
      // swell so it arrives during the audible tail - the reversed timing. The glide is re-anchored to
      // where the swell was when it started (reverseGlideBase), so mid-group slides never jump.
      double span     = 1.0 - reverseGlideBase;
      double gp       = (span > 1.0e-3) ? (swell - reverseGlideBase) / span : 1.0;
      if( gp < 0.0 ) gp = 0.0;
      if( gp > 1.0 ) gp = 1.0;
      double glideFreq = reverseFreqFrom * exp(gp * reverseLogRatio);
      instFreq = instFreq + reverseGate * (glideFreq - instFreq);
    }
    reverseInstFreq = instFreq;   // remember current pitch so a following slide can glide from here
    oscillator.setFrequency(instFreq*pitchWheelFactor);
    oscillator.calculateIncrement();

    // filter- & accent-modulation driver: crossfade forward (1->0) to the reversed main-env swell
    // (e^-Km -> 1) so the cutoff opens on the mirrored curve of the forward Decay sweep and the
    // accent emphasis lands on the peak, mirroring reversed audio.
    double mainEnvOut = mainEnvFwd + reverseGate * (swellMain - mainEnvFwd);

    double tmp1       = n1 * rc1.getSample(mainEnvOut);
    double tmp2       = 0.0;
    if( accentGain > 0.0 )
      tmp2 = mainEnvOut;
    tmp2 = n2 * rc2.getSample(tmp2);
    tmp1 = envScaler * ( tmp1 - envOffset );  // seems not to work yet
    tmp2 = accentGain*tmp2;
    double instCutoff = cutoff * pow(2.0, tmp1+tmp2);
    filter.setCutoff(instCutoff);

    // amplitude contour: crossfade the forward amp envelope with the reversed swell.
    double ampFwd    = ampEnv.getSample();
    double ampEnvOut = ampFwd + reverseGate * (revAmp - ampFwd);
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
    tmp  = notch.getSample(tmp);
    tmp *= ampEnvOut;                       // amplified
    tmp *= ampScaler;

    // find out whether we may switch ourselves off for the next call:
    idle = false;
    //idle = (sequencer.getSequencerMode() == AcidSequencer::OFF && ampEnv.endIsReached() 
    //        && fabs(tmp) < 0.000001); // ampEnvOut < 0.000001;

    return tmp;
  }

}

#endif 
