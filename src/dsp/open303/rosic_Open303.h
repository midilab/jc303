#ifndef rosic_Open303_h
#define rosic_Open303_h

#include <climits>
#include "rosic_MidiNoteEvent.h"
#include "dfl_BlendOscillator.h"
#include "dfl_DualSubOscillator.h"
#include "rosic_BiquadFilter.h"
#include "rosic_TeeBeeFilter.h"
#include "dfl_DiodeLadderFilter.h"
#include "dfl_MS20Filter.h"
#include "dfl_RolandFilter.h"
#include "dfl_WaspFilter.h"
#include "dfl_SEMFilter.h"
#include "dfl_ARP4075Filter.h"
#include "dfl_FilterType.h"
#include "dfl_ResonanceMap.h"
#include "dfl_TB303OutputNetwork.h"
#include "rosic_AnalogEnvelope.h"
#include "rosic_DecayEnvelope.h"
#include "rosic_LeakyIntegrator.h"
#include "rosic_EllipticQuarterBandFilter.h"
#include "rosic_AcidSequencer.h"
#include "dfl_LFO.h"
#include "dfl_DynamicSmoothingFilter.h"

#include <list>

using namespace std; // for the noteList

namespace rosic
{
  // Import dfl classes
  using dfl::DiodeLadderFilter;

  enum SquareModel
  {
    SQUARE_OPEN303 = 0,     // Open303's pre-rendered wavetable square (fixed duty, tanh edge drive)
    SQUARE_CIRCUIT,         // pitch-tracked pulse width and waveshaper sag, from a model of the TB-303 circuit
    NUM_SQUARE_MODELS
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

    /** Sets the sub oscillator gain (0.0 to 1.0). */
    void setSubOscGain(double newGain) { subOscGain = newGain; }

    /** Sets the sub oscillator blend (0.0 = -2 octaves, 1.0 = -1 octave). */
    void setSubOscBlend(double newBlend) { subOscBlend = newBlend; }

    /** Sets the master tuning frequency for note A4 (usually 440 Hz). */
    void setTuning(double newTuning) { tuning = newTuning; }

    /** Sets the filter's nominal cutoff frequency (in Hz). */
    void setCutoff(double newCutoff);

    /** Sets the resonance amount for the filter. */
    void setResonance(double newResonance);

    /** Sets the filter type. While playing, the output fades out over a few ms, the filter chains reset and the new
    model fades in, so the switch does not click. */
    void setFilterType(dfl::FilterType newType);

    /** Sets the filter input drive in decibels. Pushes the signal into the
    diode ladder's saturating nonlinearity for overdrive character. The TeeBee
    filter is linear in TB_303 mode, so this only affects the diode models. */
    void setFilterDrive(double newDriveDb);

    /** Returns the current filter type. */
    dfl::FilterType getFilterType() const { return pendingFilterType; }

    /** Sets the diode filter's bass comp amount, 0..1. */
    void setPassbandCompensation(double newCompensation);

    /** Sets the diode filter's response mode, 0..1: 0 = lowpass, 0.5 = bandpass, 1 = highpass. The LFO may modulate
    it (destination 3), so the knob value is kept as the base. */
    void setFilterMode(double newMode) { modeBase = newMode; }

    /** Sets the Filter FM depth (0-1). Devilfish-style audio-rate cutoff modulation.
     *  Uses AC-coupled input to modulate filter cutoff frequency. */
    void setFilterFmDepth(double depth)
    {
      diodeFilter.setFilterFmDepth(depth);
      ms20Filter.setFilterFmDepth(depth);
      junoFilter.setFilterFmDepth(depth);
      waspFilter.setFilterFmDepth(depth);
      semFilter.setFilterFmDepth(depth);
      hornetFilter.setFilterFmDepth(depth);
      arpFilter.setFilterFmDepth(depth);
    }

    /** Sets the diode models' unit trim (loop-gain scale, 1 = nominal unit). */
    void setResonanceTrim(double trim) { diodeFilter.setLoopGainTrim(trim); }

    /** Diode filter quality: Hi-Fi (the default) solves every diode pair's tanh per sample (DiodeLadderFilter::setHiFi). */
    void setFilterHiFi(bool on) { diodeFilter.setHiFi(on); }

    /** Diode ladder tension, -1..1 (Hi-Fi only; 0 = the circuit's diodes, DiodeLadderFilter::setTension). */
    void setFilterTension(double t) { diodeFilter.setTension(t); }
    void setFilterTensionEnvelope(bool on) { diodeFilter.setTensionEnvelope(on); }   // comparison switch (temporary)

    // The character controls (DiodeLadderFilter::setBite / setClipThreshold / setSharpness / setWarmth) stay in the filter
    // for the fidelity tools but are not exposed: Clip is Saturation in disguise (the model is homogeneous), Sharpness
    // acts much like Res Trim, Warmth and Bite only once Saturation drives the pairs (Bite overlapping Saturation Bias).

    /** Sets the diode filter's saturation asymmetry (0-1, default 0.5, ramps in with drive above 0 dB; its swell of the
     *  loop gain is the driven filter's push, and near full resonance its crunch: the loop locks onto its own frequency
     *  under the note, see DiodeLadderFilter SAT_BIAS_MAX). Higher = more swell and crunch. In Hi-Fi the range's top
     *  half is rescaled to end at a cap that follows the tension (SAT_BIAS_CAP_*: past it the note chokes). The
     *  plugin's Saturation Bias parameter. The MS-20 offsets its input stage, the Wasp OTA1's divider, the ARP 4075 its limiter pair; the IR3109 and SEM ignore it (see
     *  updateCircuitCharacter). */
    void setFilterBias(double bias) { diodeFilter.setSaturationBias(bias); circuitBias = bias; updateCircuitCharacter(); }

    /** Sets the diode filter's rail, 0..1 (Hi-Fi; 1 = unlimited, the full crunch; lower = cleaner, see
     *  DiodeLadderFilter::setRail). The MS-20 and IR3109 models take it as their Clip, the Wasp as its supply (see updateCircuitCharacter). */
    void setFilterRail(double rail) { diodeFilter.setRail(rail); circuitRail = rail; updateCircuitCharacter(); }

    /** Returns the Filter FM depth. */
    double getFilterFmDepth() const { return diodeFilter.getFilterFmDepth(); }

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

    /** Sets the drive (in dB) for the tanh-shaper that forms the 303-square waveform (the Open303 square
    model; the circuit model's pulse is fully saturated at any drive). */
    void setTanhShaperDrive(double newDrive)
    { waveTable2.setTanhShaperDriveFor303Square(newDrive); }

    /** Switches the TB-303 waveshaper envelope on the square (slow rise in the bottom octaves, sloped top, level
    change with pitch; see dfl::BlendOscillator::ENV_TABLE). Off by default; the pitch-tracked pulse width applies
    either way. */
    void setCircuitPulseShape(bool on) { oscillator.setCircuitPulseShape(on); }

    /** Selects the square: Open303's wavetable (the default) or the circuit model. */
    void setSquareModel(SquareModel newModel) { oscillator.setClassicSquare(newModel == SQUARE_OPEN303); }

    /** Scales the waveshaper envelope: 0 = flat top, 1 = the circuit (default), 2 = exaggerated. */
    void setPulseSag(double newSag) { oscillator.setPulseSag(newSag); }

    /** Maps a 0..1 square-drive knob to the pulse sag, in the TeeBee drive's direction (up = cleaner square):
    2 at 0, linear to the circuit (1) at the default 0.25, then ((1-k)/0.75)^1.5 to flat at 1. The power spreads
    the spectral change evenly; most of it happens between a slight sag and flat. */
    static double sagForSquareDrive(double knob)
    {
      if( knob <= 0.25 )
        return 2.0 - 4.0*knob;
      const double u = (1.0 - knob) / 0.75;
      return u*std::sqrt(u);
    }

    /** Sets the offset (raw value) for the tanh-shaper that forms the 303-square waveform. */
    void setTanhShaperOffset(double newOffset)
    { waveTable2.setTanhShaperOffsetFor303Square(newOffset); }

    /** Sets the cutoff frequency for the highpass before the TeeBee filter (the diode models use the circuit's input coupling). */
    void setPreFilterHighpass(double newCutoff) { highpass1.setCutoff(newCutoff); }

    /** Sets the cutoff frequency for the highpass inside the feedback loop of the main filter. */
    void setFeedbackHighpass(double newCutoff)
    {
      feedbackHighpass = newCutoff;
      filter.setFeedbackHighpassCutoff(newCutoff);
      // The knob is referenced to the TeeBee's 150 Hz default, where the diode sits at its default Q21 beta (300).
      // From there the corner runs exponentially to DIODE_FEEDBACK_MAX at the knob's start (350 Hz) and to
      // DIODE_FEEDBACK_MIN at its end (100 Hz).
      const double def = DiodeLadderFilter::FEEDBACK_DEFAULT;
      double diodeCutoff = newCutoff >= 150.0
        ? def * pow(DIODE_FEEDBACK_MAX / def, log(newCutoff / 150.0) / log(350.0 / 150.0))
        : def * pow(DIODE_FEEDBACK_MIN / def, log(150.0 / newCutoff) / log(150.0 / 100.0));
      diodeFilter.setFeedbackHighpass(diodeCutoff);
      outputNetwork.setCouplingPole(diodeFilter.getCouplingPoleHz());
    }

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
      // The resonance pot scales the accent capacitor's discharge lag (see
      // setResonance): tau = accentAttack * (1 + 2*resonance), so 1x..3x.
      rc2.setTimeConstant(accentAttack * (1.0 + resonanceSkewed * 2.0));
      updateAccentSweepGain();
    }

    /** Sets the accent sweep (in milliseconds): the time constant of the accent capacitor (rc2), the Devil Fish
    "Sweep Speed". The stock 15 ms charges and drains within a step, so accents stand alone; slower settings
    keep charge between accents, so a run of accents climbs higher each time. */
    void setAccentSweep(double newSweep) { setAccentAttack(newSweep); }

    /** Sets the filter envelope's decay time for accented notes (in milliseconds).
    Devil Fish provides range of 30...3000 ms for this parameter. On the normal 303, this
    parameter was fixed to 200 ms.  */
    void setAccentDecay(double newAccentDecay) { accentDecay = newAccentDecay; updateAccentSweepGain(); }

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

    /** Enables/disables LFO processing (master on/off). */
    void setLfoOn(bool on) { lfoEnabled = on; }

    /** Enables/disables resetting the LFO phase on every (non-slid) note trigger. */
    void setLfoKeySync(bool on) { lfoKeySync = on; }

    /** Sets the LFO start phase used by key-sync (0.0 to 1.0 = 0..360 degrees). */
    void setLfoPhase(double phase) { lfoPhase = phase; }
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

    /** An effect inserted after the filter and output chain, before the VCA, so it sees a steady level
    while the amp envelope shapes its output. It runs at the sample rate. */
    class PreVcaInsert
    {
    public:
      virtual ~PreVcaInsert() = default;
      virtual void process(double* buffer, int numSamples) = 0;
    };

    /** Sets the pre-VCA insert; nullptr bypasses it. */
    void setPreVcaInsert(PreVcaInsert* newInsert) { preVcaInsert = newInsert; }

    /** Renders numSamples output samples through the pre-VCA insert, in chunks of maxBlockSize. */
    void renderBlock(double* out, int numSamples);

    static const int maxBlockSize = 64;

    //-----------------------------------------------------------------------------------------------
    // event handling:

    /** Accepts note-on events (note offs are also handled here as note ons with velocity zero). */
    void noteOn(int noteNumber, int velocity, double detune);

    /** Flags the next note-on (via noteOn) as muted and/or a hammer (instant-pitch legato when
    a note is already held). Cleared once that note-on has been handled. */
    void setNextNoteModifiers(bool muted, bool hammer, bool reversed = false, double gateSamples = 0.0);

    /** Turns all possibly running notes off. */
    void allNotesOff();

    /** Sets the pitchbend value in semitones. */
    void setPitchBend(double newPitchBend);

    //-----------------------------------------------------------------------------------------------
    // embedded objects:

    MipMappedWaveTable        waveTable1, waveTable2, waveTable3;
    dfl::BlendOscillator      oscillator;
    dfl::DualSubOscillator    subOscillator;
    TeeBeeFilter              filter;
    DiodeLadderFilter         diodeFilter;
    dfl::MS20Filter           ms20Filter;
    dfl::RolandFilter         junoFilter;
    dfl::WaspFilter           waspFilter { dfl::WaspFilter::Revision::EDP };
    dfl::WaspFilter           hornetFilter { dfl::WaspFilter::Revision::A124 };
    dfl::ARP4075Filter        arpFilter;
    dfl::SEMFilter            semFilter;
    dfl::TB303OutputNetwork   outputNetwork;   // the diode models' output path, instead of allpass / highpass2 / notch
    AnalogEnvelope            ampEnv;
    DecayEnvelope             mainEnv;
    LeakyIntegrator           pitchSlewLimiter;
    DynamicSmoothingFilter    cutoffSmoother;   // anti-zipper smoothing of the cutoff parameter
    //LeakyIntegrator           ampDeClicker;
    BiquadFilter              ampDeClicker;
    LeakyIntegrator           rc1, rc2;
    OnePoleFilter             highpass1, highpass2, allpass;
    OnePoleFilter             diodeInputCoupling;   // the diode models' VCF input coupling, instead of highpass1
    BiquadFilter              notch;
    EllipticQuarterBandFilter antiAliasFilter;
    AcidSequencer             sequencer;
    dfl::LFO                  lfo;

  protected:

    /** Triggers a note (called either directly in noteOn or in getSample when the sequencer is
    used). */
    void triggerNote(int noteNumber, bool hasAccent);

    /** Clears the state of both filter models' chains (filter, input / output networks, anti-alias). */
    void resetFilters();

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

    /** Recomputes the reverse-swell shape (reverseShape) from the current reverseLength and the
    amp-envelope decay, so the swell stays a faithful reversal as the predicted length changes. */
    void updateReverseShape();

    /** Predicts the current note's length (in samples) for the reverse swell: a short-biased low
    percentile of recent measured note lengths, optionally snapped to the host's 16th-note grid. */
    double predictReverseLength();

    /** Extends the reverse swell across a legato (slide/hammer) note-on so a tied group swells as
    one reversed note. */
    void extendReverseGroup();

    void calculateEnvModScalerAndOffset();

    /** Updates the normalizer n1 according to the time-constant of rc1 and the decay-time of the
    main envelope generator. */
    void updateNormalizer1();

    /** Scales rc2 so a lone accent peaks as high as with the stock 15 ms capacitor, whatever the sweep: a slow
    sweep then builds up across consecutive accents instead of just making every accent weaker. */
    void updateAccentSweepGain();

    /** Updates the normalizer n2 according to the time-constant of rc2 and the decay-time of the
    main envelope generator. */
    void updateNormalizer2();

    /** Renders one sample up to the VCA and returns the VCA gains to apply with applyVca. */
    INLINE double renderPreVca(double& ampGain, double& volumeGain);

    INLINE double applyVca(double in, double ampGain, double volumeGain) const;

    static const int oversampling = 4;

    PreVcaInsert* preVcaInsert = nullptr;
    double blockPreVca[maxBlockSize];
    double blockAmp[maxBlockSize];
    double blockVolume[maxBlockSize];

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
    double resonanceSkewed;  // skewed resonance (0-1), scales the accent capacitor lag
    double normalDecay;      // decay time for the filter envelope on non-accented notes
    double accentDecay;      // decay time for the filter envelope on accented notes
    double normalAmpRelease; // amp-env release time for non-accented notes
    double accentAmpRelease; // amp-env release time for accented notes
    double accentGain;       // between 0.0...1.0 - to scale the 3rd amp-envelope on accents
    double accentSweepGain = 1.0; // rc2 peak makeup for the accent sweep, 1 at the stock 15 ms
    double subOscGain;       // sub oscillator gain (0.0 to 1.0)
    double subOscBlend;      // sub oscillator blend (0.0 = -2 octaves, 1.0 = -1 octave)
    double pitchWheelFactor; // scale factor for oscillator frequency from pitch-wheel
    double n1, n2;           // normalizers for the RCs that are driven by the MEG
    int    currentNote;      // note which is currently played (-1 if none)
    int    currentVel;       // velocity of currently played note
    int    noteOffCountDown; // a countdown variable till next note-off in sequencer mode
    bool   slideToNextNote;  // indicate that we need to slide to the next note in sequencer mode
    bool   hammerToNextNote; // indicate that we need to hammer (legato w/ instant pitch) to the next note
    bool   idle;             // flag to indicate that we have currently nothing to do in getSample
    bool   nextNoteMuted;    // set by setNextNoteModifiers, consumed by the next noteOn
    bool   nextNoteHammer;   // likewise: legato note-on snaps pitch instead of gliding
    double nextNoteGate;     // gate length in samples of the next note-on (0 = unknown)
    bool   nextNoteReverse;  // likewise: a triggered (non-legato) note plays time-reversed
    bool   currentNoteReverse; // reversed flag of the note (or legato group) currently sounding
    double reverseNoteMorph; // smoothed 0..1 per-note reverse amount (click-free switching)
    bool   currentNoteMuted; // flag indicating the current note is muted (shorter gate, darker, quieter)

    // TT-303 mute parameters
    double muteGateFactor;   // gate length multiplier for muted notes (0.4-0.6)
    double muteLevelFactor;  // VCA level factor for muted notes (0.4-0.7)
    double muteCutoffFactor; // filter cutoff factor for muted notes (0.6-0.9)
    double muteEnvFactor;    // envelope mod factor for muted notes (0.6-0.9)
    double muteMorph;        // smoothed 0..1 muted amount for click-free mute transitions
    double muteMorphCoeff;   // one-pole coefficient for ~2ms mute smoothing
    dfl::FilterType currentFilterType;  // filter type currently rendered
    dfl::FilterType pendingFilterType;  // selected filter type; currentFilterType follows after the switch fade-out
    double switchGain = 1.0;            // filter switch fade, 0..1
    double switchStep = 1.0;            // per-sample fade increment (5 ms)
    double feedbackHighpass = 150.0;   // Filter Feedback knob in TeeBee Hz
    // Level per filter model, after the filter chain, so the models sit at equal loudness at resonance 0 (saw, mean
    // over C2 / C3 / C4 and cutoff 400 / 1000 / 2500 Hz, env mod 60; tools/diode-fidelity/model_level). Level matching
    // between models lives here rather than in the filters, which stay at their own (circuit) level.
    static constexpr double FILTER_LEVEL[dfl::NUM_FILTER_TYPES] = { 1.0, 2.401, 1.349, 1.274, 1.106, 1.259, 1.265, 1.288 };   // TeeBee, Spicy (+7.6 dB), MissBehave (+2.6 dB), JuneBug (+2.1 dB), Wasabi (+0.9 dB), Hornet (+2.0 dB), SemiSweet (+2.0 dB), Oddity (+2.2 dB)
    // The diode models' corner at the knob's end: the coupling pole C27/C25 into Q21 at 46 Hz, Q21 beta about 500 (the
    // 2SC1583's datasheet minimum). Higher betas make the network's ~7 Hz mode motorboat at high resonance (SPICE:
    // beta 800 at full pot), so the knob stops here. The pole is 578.1 rad/s x corner / 115.
    static constexpr double DIODE_FEEDBACK_MIN = 46.0 * 2.0 * 3.14159265358979 * 115.0 / 578.1;
    // ... and at the knob's start: coupling pole 214 Hz (Q21 beta about 70).
    static constexpr double DIODE_FEEDBACK_MAX = 350.0 * 115.0 / 150.0;

    // LFO modulation depth
    double lfoDepth;    // LFO depth (0.0 to 1.0)
    double lfoPhase = 0.0;      // LFO start phase for key-sync (0.0 to 1.0)
    bool lfoKeySync = false;    // reset LFO phase on note trigger
    int lfoDestination;   // LFO destination (0=filter cutoff, 1=volume, 2=pitch, 3=pulse width, 4=filter mode)
    // Saturation Bias / Rail / drive as last set, for the MS-20, IR3109 and Wasp models (updateCircuitCharacter)
    double circuitBias = 0.5, circuitRail = 1.0, circuitDriveDb = 0.0;
    void updateCircuitCharacter();

    double modeBase = 0.0;    // diode filter mode knob value (0 = LP, 0.5 = BP, 1 = HP); the LFO modulates it
    bool lfoEnabled = false;  // master LFO processing switch

    list<MidiNoteEvent> noteList;

  };

  //-------------------------------------------------------------------------------------------------
  // inlined functions:

  INLINE double Open303::getSample()
  {
    // 'idle' short-circuits output before the first note is ever triggered (it starts true and is
    // cleared on the first trigger). Note it is never set back to true afterwards, so this is only a
    // pre-first-note guard; re-enabling end-of-note detection here to save CPU on silence is a
    // possible future optimization but needs click-free retrigger testing.
    if( idle )
      return 0.0;

    double ampGain, volumeGain;
    double tmp = renderPreVca(ampGain, volumeGain);
    return applyVca(tmp, ampGain, volumeGain);
  }

  INLINE double Open303::renderPreVca(double& ampGain, double& volumeGain)
  {
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
    double pwmDepth = 0.0, pwmLfo = 0.0;
    double modeLfoAdd = 0.0;    // LFO contribution to the filter mode

    if (lfoEnabled && lfoDepth > 0.0)
    {
      // Get LFO output (0.0 to +1.0 unipolar, convert to bipolar for modulation)
      double lfoValue = lfo.getSample() * 2.0 - 1.0;  // Convert unipolar to bipolar

      switch (lfoDestination) {
        case 0:
            // Apply LFO filter modulation (convert to bipolar, in octaves)
            lfoFilterMod = lfoValue * lfoDepth * 2.0;  // +/- 2 octaves max
            break;
        case 1:
            // Apply LFO volume modulation - tremolo (convert to linear amplitude multiplier)
            volumeModFactor = 1.0 - lfoDepth + (lfoValue * lfoDepth);
            break;
        case 2:
        {
            // Apply LFO pitch modulation (in semitones, converted to frequency multiplier, +/- 12 semitones max)
            double semitones = lfoValue * lfoDepth * 12.0;
            pitchModFactor = pow(2.0, semitones / 12.0);
            break;
        }
        case 3:
            // Pulse width (circuit square only), unipolar towards narrow; applied with the pitch-tracked width below
            pwmDepth = lfoDepth;
            pwmLfo   = 0.5 * (lfoValue + 1.0);
            break;
        case 4:
            // Apply LFO to the diode filter mode: +/- 0.5 around the knob at depth 1 (LP -> BP or BP -> HP)
            modeLfoAdd = 0.5 * lfoValue * lfoDepth;
            break;
      }
    }

    diodeFilter.setMode(modeBase + modeLfoAdd);   // LFO-modulated when destination 3
    ms20Filter.setMode(modeBase + modeLfoAdd);
    junoFilter.setMode(modeBase + modeLfoAdd);
    waspFilter.setMode(modeBase + modeLfoAdd);
    semFilter.setMode(modeBase + modeLfoAdd);
    hornetFilter.setMode(modeBase + modeLfoAdd);
    arpFilter.setMode(modeBase + modeLfoAdd);

    // --- reverse-gate: synthesize a genuinely time-reversed contour -------------------------------
    // Playing a note backwards means the amplitude/filter grow as a *back-loaded* exponential: they
    // stay quiet for most of the note, then swell rapidly to a peak right at the end, where the
    // original attack transient becomes an abrupt cut. We drive that swell from a phase counter that
    // reaches 1 at 'reverseLength' samples after the trigger - which triggerNote() sets to the note's
    // (predicted) length - so the peak lands exactly at the note's end. reverseShape sets how
    // back-loaded the curve is; reverseDen == e^reverseShape - 1 normalizes it to 0..1. Computed
    // before the oscillator frequency because in reverse mode the swell also shapes the pitch glide.
    double mainEnvFwd = mainEnv.getSample();  // forward main envelope: decays 1 -> 0
    // effective reverse depth: the global toggle, or a note flagged reversed in the sequencer
    // (smoothed so the contour crossfade doesn't click); identical to reverseGate when no step is flagged
    const double reverseNoteTarget = currentNoteReverse ? 1.0 : 0.0;
    reverseNoteMorph = reverseNoteTarget + muteMorphCoeff * (reverseNoteMorph - reverseNoteTarget);
    const double revDepth = reverseGate > reverseNoteMorph ? reverseGate : reverseNoteMorph;
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

    // calculate instantaneous oscillator frequency and set up the oscillator. The portamento slew
    // runs in the log-frequency domain (glide log(freq), then exponentiate back to Hz) so the slide
    // is an exponential in pitch/cents rather than in Hz - this matches how a real 303 slews its pitch CV
    // (volts/octave) and keeps octave slides from lingering in the low end.
    // Apply pitch modulation AFTER slew limiter to prevent smoothing of audio-rate LFO.
    double slewFreq = exp( pitchSlewLimiter.getSample( log(oscFreq) ) );
    if( revDepth > 0.0 )
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
      slewFreq = slewFreq + revDepth * (glideFreq - slewFreq);
    }
    reverseInstFreq = slewFreq;   // remember current pitch so a following slide can glide from here
    double instFreq = slewFreq * pitchModFactor;
    oscillator.setFrequency(instFreq*pitchWheelFactor);
    oscillator.calculateIncrement();
    subOscillator.setFrequency(instFreq*pitchWheelFactor);
    subOscillator.calculateIncrement();

    // TB-303 pulse width tracks pitch: the square comes from the saw through the Q8 waveshaper, whose
    // RC-coupled bias makes the duty depend on frequency: 58% at C1, 50% near 65 Hz, 41.7% at C4, 36% at C5.
    // Cubic fit in octaves above C1 to an ngspice model of the stage (service-manual values), within
    // 1.1 points from 10 Hz to 2 kHz; it agrees with ~50% at 85 Hz measured on a real unit. The input is
    // clamped to that range so the cubic cannot run away under pitch bend or LFO.
    {
      double x  = clip(log2(instFreq*pitchWheelFactor / 32.703), -1.71, 5.93);   // octaves above C1, 10 Hz..2 kHz
      double pw = clip(58.164 + x*(-7.543 + x*(0.812 - x*0.071)), 25.0, 75.0);
      // PWM, unipolar towards narrow: depth first pulls centers above 50% down to 50% (a width and its complement
      // sound alike, so sweeping through 50% would fold the motion over), then the LFO sweeps towards 10%
      if( pwmDepth > 0.0 )
      {
        const double centre = pw - pwmDepth*std::max(pw - 50.0, 0.0);
        pw = clip(centre - pwmDepth*pwmLfo*(centre - 10.0), 10.0, 75.0);
      }
      oscillator.setPulseWidth(pw);
    }

    // calculate instantaneous cutoff frequency from the nominal cutoff and all its modifiers and
    // set up the filter:
    // filter- & accent-modulation driver: crossfade forward (1->0) to the reversed main-env swell
    // (e^-Km -> 1) so the cutoff opens on the mirrored curve of the forward Decay sweep and the
    // accent emphasis lands on the peak, mirroring reversed audio.
    double mainEnvOut = mainEnvFwd + revDepth * (swellMain - mainEnvFwd);
    double tmp1       = n1 * rc1.getSample(mainEnvOut);
    double tmp2       = 0.0;
    if( accentGain > 0.0 )
      tmp2 = mainEnvOut;
    tmp2 = n2 * rc2.getSample(tmp2) * accentSweepGain;
    tmp1 = envScaler * ( tmp1 - envOffset );  // main env-mod scaling (Schmidt's measured mapping)
    tmp2 = accentGain*tmp2;

    // Smoothly morph the mute reductions (0 = open, 1 = fully muted) so that
    // level/cutoff/env changes ramp over ~2ms instead of stepping in one sample.
    // A hard switch on a held (legato hammer/slide) note produces an audible click.
    double muteTarget = currentNoteMuted ? 1.0 : 0.0;
    muteMorph = muteTarget + muteMorphCoeff * (muteMorph - muteTarget);

    // Apply mute envelope reduction (morphed)
    tmp1 *= 1.0 + muteMorph * (muteEnvFactor - 1.0);

    // Anti-zipper: dynamically smooth the (block-rate) cutoff parameter before it drives the filter
    double smoothedCutoff = cutoffSmoother.getSample(cutoff);
    double instCutoff = smoothedCutoff * (1.0 + muteMorph * (muteCutoffFactor - 1.0));  // mute: darker tone
    instCutoff *= pow(2.0, tmp1+tmp2+lfoFilterMod);
    filter.setCutoff(instCutoff);
    diodeFilter.setCutoff(instCutoff);
    ms20Filter.setCutoff(instCutoff);
    junoFilter.setCutoff(instCutoff);
    waspFilter.setCutoff(instCutoff);
    semFilter.setCutoff(instCutoff);
    hornetFilter.setCutoff(instCutoff);
    arpFilter.setCutoff(instCutoff);

    // amplitude contour: crossfade the forward amp envelope with the reversed swell.
    double ampFwd    = ampEnv.getSample();
    double ampEnvOut = ampFwd + revDepth * (revAmp - ampFwd);
    //ampEnvOut += 0.45*filterEnvOut + accentGain*6.8*filterEnvOut;
    if( ampEnv.isNoteOn() )
      ampEnvOut += 0.45*mainEnvOut + accentGain*4.0*mainEnvOut;
    ampEnvOut = ampDeClicker.getSample(ampEnvOut);

    // oversampled calculations:
    double tmp;
    for(int i=1; i<=oversampling; i++)
    {
      tmp  = -oscillator.getSample();         // the raw oscillator signal
      if( subOscGain > 0.0 )
        tmp += subOscGain * (-subOscillator.getSample(subOscBlend));
      if(currentFilterType == dfl::FILTER_TEEBEE)
      {
        tmp = highpass1.getSample(tmp);       // pre-filter highpass
        tmp = filter.getSample(tmp);
      }
      else if(currentFilterType == dfl::FILTER_MS20)
      {
        tmp = highpass1.getSample(tmp);       // pre-filter highpass
        tmp = ms20Filter.getSample(tmp);
      }
      else if(currentFilterType == dfl::FILTER_JUNO106)
      {
        tmp = highpass1.getSample(tmp);       // pre-filter highpass
        tmp = junoFilter.getSample(tmp);
      }
      else if(currentFilterType == dfl::FILTER_WASP || currentFilterType == dfl::FILTER_WASP_A124)
      {
        tmp = highpass1.getSample(tmp);       // pre-filter highpass
        tmp = (currentFilterType == dfl::FILTER_WASP ? waspFilter : hornetFilter).getSample(tmp);
      }
      else if(currentFilterType == dfl::FILTER_SEM)
      {
        tmp = highpass1.getSample(tmp);       // pre-filter highpass
        tmp = semFilter.getSample(tmp);
      }
      else if(currentFilterType == dfl::FILTER_ARP4075)
      {
        tmp = highpass1.getSample(tmp);       // pre-filter highpass
        tmp = arpFilter.getSample(tmp);
      }
      else
      {
        tmp = diodeInputCoupling.getSample(tmp);
        tmp = diodeFilter.getSample(tmp);
      }
      tmp  = antiAliasFilter.getSample(tmp);  // anti-aliasing filtered

    }

    // these filters may actually operate without oversampling (but only if we reset them in
    // triggerNote - avoid clicks)
    if(currentFilterType == dfl::FILTER_DIODE)
      tmp = outputNetwork.getSample(tmp);
    else if(currentFilterType != dfl::FILTER_TEEBEE)
      tmp = highpass2.getSample(tmp);         // MS-20 / IR3109 / Wasp / SEM / ARP: the post-filter highpass only (allpass / notch are the 303's)
    else
    {
      tmp  = allpass.getSample(tmp);
      tmp  = highpass2.getSample(tmp);
      tmp = notch.getSample(tmp);
    }
    tmp *= FILTER_LEVEL[currentFilterType]; // level match between filter models
    if(pendingFilterType != currentFilterType)
    {
      switchGain -= switchStep;
      if(switchGain <= 0.0)
      {
        switchGain = 0.0;
        currentFilterType = pendingFilterType;
        resetFilters();
      }
    }
    else if(switchGain < 1.0)
      switchGain = fmin(switchGain + switchStep, 1.0);
    tmp *= switchGain;                      // filter switch fade
    ampGain    = ampEnvOut;
    volumeGain = volumeModFactor * (1.0 + muteMorph * (muteLevelFactor - 1.0));  // LFO volume mod and mute level reduction (morphed)

    return tmp;
  }

  INLINE double Open303::applyVca(double in, double ampGain, double volumeGain) const
  {
    in *= ampGain;                          // amplified
    in *= ampScaler;
    in *= volumeGain;                       // LFO volume modulation
    return in;
  }

}

#endif
