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
     *  plugin's Saturation Bias parameter. The MS-20 and IR3109 models offset their input stage, the Wasp OTA1's divider (see
     *  updateCircuitCharacter). */
    void setFilterBias(double bias) { diodeFilter.setSaturationBias(bias); circuitBias = bias; updateCircuitCharacter(); }

    /** Sets the diode filter's rail, 0..1 (Hi-Fi; 1 = unlimited, the full crunch; lower = cleaner, see
     *  DiodeLadderFilter::setRail). The MS-20 and IR3109 models take it as their Clip, the Wasp as its supply (see updateCircuitCharacter). */
    void setFilterRail(double rail) { diodeFilter.setRail(rail); circuitRail = rail; updateCircuitCharacter(); }

    /** Oddity (ARP 4075) only: the 2k2 tail mod (ARP4075Filter::setTailMod). */
    void setFilterArpMod(bool on) { arpFilter.setTailMod(on); }

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

    /** Releases a note (called either directly in noteOn or in getSample when the sequencer is
    used). */
    void releaseNote(int noteNumber);

    /** Sets the decay-time of the main envelope and updates the normalizers n1, n2 accordingly. */
    void setMainEnvDecay(double newDecay);

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
    bool   idle;             // flag to indicate that we have currently nothing to do in getSample
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

    // calculate instantaneous oscillator frequency and set up the oscillator. The portamento slew
    // runs in the log-frequency domain (glide log(freq), then exponentiate back to Hz) so the slide
    // is an exponential in pitch/cents rather than in Hz - this matches how a real 303 slews its pitch CV
    // (volts/octave) and keeps octave slides from lingering in the low end.
    // Apply pitch modulation AFTER slew limiter to prevent smoothing of audio-rate LFO.
    double instFreq = exp( pitchSlewLimiter.getSample( log(oscFreq) ) ) * pitchModFactor;
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
    double mainEnvOut = mainEnv.getSample();
    double tmp1       = n1 * rc1.getSample(mainEnvOut);
    double tmp2       = 0.0;
    if( accentGain > 0.0 )
      tmp2 = mainEnvOut;
    tmp2 = n2 * rc2.getSample(tmp2) * accentSweepGain;
    tmp1 = envScaler * ( tmp1 - envOffset );  // main env-mod scaling (Schmidt's measured mapping)
    tmp2 = accentGain*tmp2;
    // Anti-zipper: dynamically smooth the (block-rate) cutoff parameter before it drives the filter
    double smoothedCutoff = cutoffSmoother.getSample(cutoff);
    double instCutoff = smoothedCutoff * pow(2.0, tmp1+tmp2+lfoFilterMod);
    filter.setCutoff(instCutoff);
    diodeFilter.setCutoff(instCutoff);
    ms20Filter.setCutoff(instCutoff);
    junoFilter.setCutoff(instCutoff);
    waspFilter.setCutoff(instCutoff);
    semFilter.setCutoff(instCutoff);
    hornetFilter.setCutoff(instCutoff);
    arpFilter.setCutoff(instCutoff);

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
    tmp *= ampEnvOut;                       // amplified
    tmp *= ampScaler;
    tmp *= volumeModFactor;                 // LFO volume modulation

    return tmp;
  }

}

#endif
