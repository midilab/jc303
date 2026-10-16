#include "rosic_Open303.h"
using namespace rosic;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

Open303::Open303()
{
  tuning           =   440.0;
  TuningTable::fillEqualTemperament(pitchBanks[0], 440.0);
  pitchBanks[1] = pitchBanks[0];
  ampScaler        =     1.0;
  oscFreq          =   440.0;
  sampleRate       = 44100.0;
  level            =   -12.0;
  levelByVel       =    12.0;
  accent           =     0.0;
  reverseGate      =     0.0;
  reversePhase     =     0.0;
  reverseLength    = 44100.0;  // 1s placeholder until the first note sets it (see triggerNote)
  reverseMeasured  =     0.0;
  reverseHistPos   =       0;
  reverseHistCount =       0;
  reverseTempoBpm  =     0.0;
  for(int i=0; i<8; i++) reverseHist[i] = 0.0;
  reverseLastOnPhase =   0.0;
  reverseStep      =     0.0;
  reverseShape     =     3.5;
  reverseShapeMain =     3.5;
  reverseDen       = exp(reverseShapeMain) - 1.0;
  reverseFreqFrom  =   440.0;
  reverseLogRatio  =     0.0;
  reverseGlideBase =     0.0;
  reverseInstFreq  =   440.0;
  reverseSwellNow  =     0.0;
  slideTime        =    60.0;
  cutoff           =  1000.0;
  cutoffKnobModValue = cutoffToKnobPosition(cutoff);
  envUpFraction    =     2.0/3.0;
  normalAttack     =     3.0;
  accentAttack     =    15.0;
  resonanceSkewed  =     0.0;
  normalDecay      =  1000.0;
  accentDecay      =   200.0;
  normalAmpRelease =     1.0;
  accentAmpRelease =    50.0;
  accentGain       =     0.0;
  pitchWheelFactor =     1.0;
  subOscGain       =     0.0;
  subOscBlend      =     1.0;  // -1 octave
  currentNote      =    -1;
  currentVel       =     0;
  noteOffCountDown =     0;
  slideToNextNote  = false;
  hammerToNextNote = false;
  idle             = true;
  currentFilterType = dfl::FILTER_TEEBEE;
  pendingFilterType = dfl::FILTER_TEEBEE;
  currentNoteMuted = false;
  currentNoteReverse = false;
  reverseNoteMorph = 0.0;
  muteMorph        =   0.0;   // start unmuted
  muteMorphCoeff   =   0.0;   // will be set in setSampleRate

  // TT-303 mute parameters (conservative defaults)
  muteGateFactor   =     0.5;   // 50% gate length
  muteLevelFactor  =     0.6;   // 60% volume
  muteCutoffFactor =     0.7;   // 70% cutoff
  muteEnvFactor    =     0.7;   // 70% envelope mod

  // LFO defaults
  lfoDepth         =    0.0;
  lfoDestination   =    0;

  setEnvMod(25.0);

  oscillator.setWaveTable1(&waveTable1);
  oscillator.setWaveForm1(MipMappedWaveTable::SAW303);
  // Open303's pre-rendered square (the default square model); the circuit model synthesises the pulse instead
  waveTable2.setWaveform(MipMappedWaveTable::SQUARE303);
  oscillator.setWaveTable2(&waveTable2);
  setSquareModel(SQUARE_OPEN303);

  junoOscillator.setWaveform(dfl::AlphaJunoOscillator::SAW01);

  waveTable3.setWaveform(MipMappedWaveTable::SAW303);
  subOscillator.setWaveTable(&waveTable3);

  //mainEnv.setNormalizeSum(true);
  mainEnv.setNormalizeSum(false);

  ampEnv.setAttack(0.0);
  ampEnv.setDecay(1230.0);
  ampEnv.setSustainLevel(0.0);
  ampEnv.setRelease(0.5);
  ampEnv.setTauScale(1.0);

  pitchSlewLimiter.setTimeConstant((float)(slideTime * (22.0/60.0)));  // matches setSlideTime()'s RC mapping
  //ampDeClicker.setTimeConstant(2.0);
  ampDeClicker.setMode(BiquadFilter::LOWPASS12);
  ampDeClicker.setGain( amp2dB(sqrt(0.5)) );
  ampDeClicker.setFrequency(200.0);

  rc1.setTimeConstant(0.0);
  rc2.setTimeConstant(15.0);

  highpass1.setMode(OnePoleFilter::HIGHPASS);
  diodeInputCoupling.setMode(OnePoleFilter::HIGHPASS);
  highpass2.setMode(OnePoleFilter::HIGHPASS);
  allpass.setMode(OnePoleFilter::ALLPASS);
  notch.setMode(BiquadFilter::BANDREJECT);

  setSampleRate(sampleRate);

  // tweakables:
  oscillator.setPulseWidth(50.0);
  oscillator.setCircuitPulseShape(false);   // flat-topped square unless switched on (JC303: the Mods switch)
  highpass1.setCutoff(44.486);
  diodeInputCoupling.setCutoff(1.0 / (2.0 * PI * 1e-6 * 222.2e3));   // C17 1u into R62 220k + R70 2.2k: 0.72 Hz
  highpass2.setCutoff(24.167);
  allpass.setCutoff(14.008);
  notch.setFrequency(7.5164);
  notch.setBandwidth(4.7);

  setFeedbackHighpass(feedbackHighpass);   // also sets the diode corner and the output network's coupling pole
}

Open303::~Open303()
{

}

//-------------------------------------------------------------------------------------------------
// parameter settings:

void Open303::setSampleRate(double newSampleRate)
{
  mainEnv.setSampleRate         (       newSampleRate);
  ampEnv.setSampleRate          (       newSampleRate);
  pitchSlewLimiter.setSampleRate((float)newSampleRate);
  cutoffSmoother.setSampleRate(         newSampleRate);
  cutoffSmoother.setRange(log2(20.0), log2(20000.0));   // smoothed in log2(Hz)
  cutoffSmoother.setState(log2(cutoff));  // seed to current cutoff so we don't sweep up from 0
  ampDeClicker.setSampleRate(    (float)newSampleRate);
  rc1.setSampleRate(             (float)newSampleRate);
  rc2.setSampleRate(             (float)newSampleRate);
  sequencer.setSampleRate(              newSampleRate);
  lfo.setSampleRate(                    newSampleRate);

  // Mute morph smoother: ~2ms one-pole so mute level/cutoff/env changes ramp
  // instead of stepping (avoids clicks at legato hammer/slide boundaries)
  muteMorphCoeff = exp(-1.0 / (0.002 * newSampleRate));

  highpass2.setSampleRate     (         newSampleRate);
  allpass.setSampleRate       (         newSampleRate);
  notch.setSampleRate         (         newSampleRate);
  outputNetwork.setSampleRate (         newSampleRate);
  outputNetwork.setCouplingInFilter(true);   // the diode filter applies C27 / C25 and Q21 at the oversampled rate

  highpass1.setSampleRate     (  oversampling*newSampleRate);
  diodeInputCoupling.setSampleRate(oversampling*newSampleRate);

  oscillator.setSampleRate    (  oversampling*newSampleRate);
  junoOscillator.setSampleRate(  oversampling*newSampleRate);
  phaseshaper.setSampleRate   (  oversampling*newSampleRate);
  subOscillator.setSampleRate (  oversampling*newSampleRate);
  filter.setSampleRate        (  oversampling*newSampleRate);
  diodeFilter.setSampleRate   (  oversampling*newSampleRate);
  switchStep = 1.0 / (0.005 * newSampleRate);
  ms20Filter.setSampleRate    (  oversampling*newSampleRate);
  junoFilter.setSampleRate    (  oversampling*newSampleRate);
  waspFilter.setSampleRate    (  oversampling*newSampleRate);
  semFilter.setSampleRate     (  oversampling*newSampleRate);
  hornetFilter.setSampleRate  (  oversampling*newSampleRate);
  arpFilter.setSampleRate     (  oversampling*newSampleRate);
  sampleRate = newSampleRate;
  updateAccentSweepGain();
  accentGateCoeff = exp(-1.0 / (0.001 * newSampleRate));   // 1 ms
}

void Open303::setCutoff(double newCutoff)
{
  cutoff = newCutoff;
  cutoffKnobModValue = cutoffToKnobPosition(newCutoff);
  calculateEnvModScalerAndOffset(envMod);
}

void Open303::setResonance(double newResonance)
{
  baseResonance = newResonance;
  applyResonance(newResonance);
}

void Open303::applyResonance(double newResonance)
{
  // newResonance is a percentage (0..100). The TeeBee filter takes percent
  // directly and skews/normalizes it internally; the diode filter takes the same
  // saturating-exponential skew (see TeeBeeFilter::setResonance) as a 0..1 value
  // and undoes it to recover the resonance pot's rotation.
  double normalizedResonance = 0.01 * newResonance;               // 0..100 -> 0..1
  double skewedResonance = (1.0 - exp(-3.0*normalizedResonance)) / (1.0 - exp(-3.0));

  filter.setResonance(newResonance);
  diodeFilter.setResonance(skewedResonance);

  // TB-303 accent capacitor: the resonance pot controls the accent-sweep
  // circuit's discharge rate. Higher resonance = slower discharge = the accent
  // sweeps up more smoothly and stacks higher on rapid successive accents.
  resonanceSkewed = skewedResonance;
  setAccentAttack(accentAttack);
  // the MS-20, IR3109 and Wasp models undo the skew (their pots are linear); the knob reaches their pots through a per-model
  // map, so they resonate alike (dfl_ResonanceMap.h)
  auto skewedPot = [normalizedResonance](dfl::FilterType type)
  {
    return (1.0 - exp(-3.0*dfl::resonancePot(type, normalizedResonance))) / (1.0 - exp(-3.0));
  };
  ms20Filter.setResonance(skewedPot(dfl::FILTER_MS20));
  junoFilter.setResonance(skewedPot(dfl::FILTER_JUNO106));
  waspFilter.setResonance(skewedPot(dfl::FILTER_WASP));   // the Wasp's RES pot is linear too
  semFilter.setResonance(skewedPot(dfl::FILTER_SEM));   // the SEM's too
  hornetFilter.setResonance(skewedPot(dfl::FILTER_WASP_A124));
  arpFilter.setResonance(skewedPot(dfl::FILTER_ARP4075));        // the Odyssey's R144 is linear
  outputNetwork.setPot(diodeFilter.getResonancePot());
}

void Open303::setFilterType(dfl::FilterType newType)
{
  pendingFilterType = newType;
  if( idle )
    currentFilterType = newType;   // nothing is sounding yet: switch right away
}

void Open303::resetFilters()
{
  filter.reset();
  diodeFilter.reset();
  ms20Filter.reset();
  junoFilter.reset();
  waspFilter.reset();
  semFilter.reset();
  hornetFilter.reset();
  arpFilter.reset();
  highpass1.reset();
  diodeInputCoupling.reset();
  highpass2.reset();
  allpass.reset();
  notch.reset();
  outputNetwork.reset();
  antiAliasFilter.reset();
}

void Open303::setFilterDrive(double newDriveDb)
{
  // The diode ladder saturates its input (see DiodeLadderFilter::getSample),
  // so drive changes its timbre. TeeBee is linear in TB_303 mode - the call
  // is harmless there but has no audible effect.
  filter.setDrive(newDriveDb);
  diodeFilter.setInputDrive(newDriveDb);
  ms20Filter.setInputDrive(newDriveDb);
  junoFilter.setInputDrive(newDriveDb);
  waspFilter.setInputDrive(newDriveDb);
  semFilter.setInputDrive(newDriveDb);
  hornetFilter.setInputDrive(newDriveDb);
  arpFilter.setInputDrive(newDriveDb);
  circuitDriveDb = newDriveDb;
  updateCircuitCharacter();
}

void Open303::updateCircuitCharacter()
{
  // Saturation Bias and Rail on the MS-20 and IR3109 models, each on its own circuit (the diode filter has its own):
  // - Bias offsets the element the input drives (IR3109 stage 1: the input pair, its resting current trimmed out, the
  //   diode ladder's bias shape; Korg35: one forward limiter diode), see setSaturationBias. It ramps in with
  //   Saturation as the diode's does, from none at 0 dB to the knob's value at +16 dB, so the clean filter stays the
  //   circuit. The IR3109 and SEM keep Bias at 0: even at +16 dB it changed them by under -14 dB.
  // - MS-20: Rail = Clip, the limiter diodes' threshold: 1 = the circuit, lower = the limiter bends earlier. That is a
  //   softer limiter, not a harder one: the earlier diodes already conduct at rest, so the x58 stage's gain falls and
  //   its curve rounds off (at 0 dB a full-scale sine's THD falls from ~4 % to ~3 %, at +16 dB from ~28 % to ~16 %).
  //   The circuit has real rails, so there is no runaway for a ceiling to tame.
  // - Juno: Rail sags the IR3109's +-15 V supplies (the stage OTAs' compliance and the followers' ceiling), see
  //   RolandFilter::setRail.
  // - Wasp: Bias offsets OTA1's input divider (IC1 then sits off its switching point), ramping in the same way; Rail is
  //   its supply, 1 = the circuit, lower = sagging (less headroom and inverter gain), see WaspFilter::setRail.
  // - SEM: Rail sags its +-15 V supplies, see SEMFilter::setRail. (SEMFilter::setSaturationBias would turn OTA1's offset
  //   trim T9 off null; it is held nulled.)
  // - ARP 4075: Bias offsets the limiter pair, ramping in the same way; Rail sags its +15 / -14 V supplies, the
  //   limiter's tail with the +15 V: a quieter, less resonant filter rather than an earlier limit (its threshold is
  //   the pair's 2 VT), see ARP4075Filter::setRail.
  const double bias = circuitBias * std::clamp(circuitDriveDb / 16.0, 0.0, 1.0);
  const double clip = 0.5 * circuitRail;
  ms20Filter.setSaturationBias(bias);
  ms20Filter.setClipThreshold(clip);
  junoFilter.setSaturationBias(0.0);   // Bias held at 0 (trimmed out): too faint on the IR3109 to be worth a knob
  junoFilter.setRail(circuitRail);     // the IR3109's +-15 V supplies
  for (auto* f : { &waspFilter, &hornetFilter })
  {
    f->setSaturationBias(bias);
    f->setRail(circuitRail);
  }
  arpFilter.setSaturationBias(bias);   // the limiter pair's offset
  arpFilter.setRail(circuitRail);      // its +15 / -14 V supplies: quieter and less resonant, see ARP4075Filter::setRail
  semFilter.setSaturationBias(0.0);    // Bias held at 0 (T9 nulled): too faint on the SEM to be worth a knob
  semFilter.setRail(circuitRail);
}

void Open303::setPassbandCompensation(double newCompensation)
{
  // Diode bass comp (the TeeBee has none)
  diodeFilter.setPassbandCompensation(newCompensation);
}

void Open303::setEnvMod(double newEnvMod)
{
  envMod = newEnvMod;
  calculateEnvModScalerAndOffset(envMod);
}

void Open303::setAccent(double newAccent)
{
  accent = 0.01 * newAccent;
}

void Open303::setVolume(double newLevel)
{
  level     = newLevel;
  ampScaler = dB2amp(level);
}

void Open303::setSlideTime(double newSlideTime)
{
  if( newSlideTime >= 0.0 )
  {
    slideTime = newSlideTime;
    // The TB-303 slide is a passive RC lag on the pitch DAC output: the DAC feeds C = 0.22 uF
    // through R = 100 kOhm (stock) up to ~603 kOhm (with the Devil Fish slide pot), so the RC time
    // constant tau = R*C runs from ~22 ms stock to ~133 ms at maximum (~6x). Because the pitch CV is
    // volts/octave, this lag acts in the log-frequency domain (see getSample / triggerNote), which
    // is why the glide is symmetric in pitch and has the same time constant for every interval.
    //
    // slideTime is the full slide time in ms; the stock 303 is ~60 ms, which corresponds to the
    // ~22 ms RC constant (about 3 tau to settle). tau = slideTime * (22/60) keeps the digital
    // one-pole matched to the hardware RC across the whole range, so the Devil Fish "Slide Time" mod
    // knob (which drives this via setParameter, mapping its 0..1 to ~2..360 ms) sweeps tau from
    // ~0.7 ms (near-instant) through 22 ms at the 60 ms stock point up to ~133 ms - i.e. the knob's
    // top lands exactly on the real hardware maximum.
    pitchSlewLimiter.setTimeConstant((float)(slideTime * (22.0/60.0)));
  }
}

void Open303::setPitchBend(double newPitchBend)
{
  pitchWheelFactor = pitchOffsetToFreqFactor(newPitchBend);
}

//------------------------------------------------------------------------------------------------------------
// others:

void Open303::renderBlock(double* out, int numSamples)
{
  while( numSamples > 0 )
  {
    const int n = numSamples < maxBlockSize ? numSamples : maxBlockSize;
    if( idle )
    {
      for(int i=0; i<n; i++)
        out[i] = 0.0;
    }
    else
    {
      for(int i=0; i<n; i++)
        blockPreVca[i] = renderPreVca(blockAmp[i], blockVolume[i]);
      if( preVcaInsert != nullptr )
        preVcaInsert->process(blockPreVca, n);
      for(int i=0; i<n; i++)
        out[i] = applyVca(blockPreVca[i], blockAmp[i], blockVolume[i]);
    }
    out        += n;
    numSamples -= n;
  }
}

void Open303::noteOn(int noteNumber, int velocity, double detune, bool muted, bool hammer, bool reversed,
                     double gateSamples)
{
  if( sequencer.modeWasChanged() )
    allNotesOff();

  if( sequencer.getSequencerMode() != AcidSequencer::OFF )
  {
    if( velocity == 0 )
    {
      sequencer.stop();
      releaseNote(currentNote);
      currentNote = -1;
      currentVel  = 0;
    }
    else
    {
      sequencer.start();
      noteOffCountDown = INT_MAX;
      slideToNextNote  = false;
      currentNote      = noteNumber;
      currentVel       = velocity;
    }
    idle = false;
    return;
  }

  if( velocity == 0 ) // velocity zero indicates note-off events
  {
    MidiNoteEvent releasedNote(noteNumber, 0);
    noteList.remove(releasedNote);
    if( noteList.empty() )
    {
      currentNote = -1;
      currentVel  = 0;
    }
    else
    {
      currentNote = noteList.front().getKey();
      currentVel  = noteList.front().getVelocity();
    }
    releaseNote(noteNumber);
  }
  else // velocity was not zero, so this is an actual note-on
  {
    // check if the note-list is empty (indicating that currently no note is playing) - if so,
    // trigger a new note, otherwise, slide to the new note:
    // (a hammer snaps to the new pitch instead of gliding; a muted note is never accented)
    const bool hasAccent = velocity >= 100 && !muted;
    if( noteList.empty() )
    {
      triggerNote(noteNumber, hasAccent);
      currentNoteReverse = reversed;   // a legato group keeps its first note's flag
      if( gateSamples > 0.0 )          // exact gate length (tied sections included) beats the prediction
      {
        reverseLength = gateSamples;
        updateReverseShape();
      }
    }
    else
    {
      if( hammer )
        hammerToNote(noteNumber, hasAccent);
      else
        slideToNote(noteNumber, hasAccent);
      if( gateSamples > 0.0 )   // the slid-into note's gate: the group's end is known, not projected
      {
        reverseLength = reversePhase + gateSamples;
        updateReverseShape();
        anchorReverseGlide(hammer);
      }
    }
    currentNoteMuted = muted;

    currentNote = noteNumber;
    currentVel  = 64;

    // and we need to add the new note to our list, of course:
    MidiNoteEvent newNote(noteNumber, velocity);
    noteList.push_front(newNote);
  }
  idle = false;
}

void Open303::allNotesOff()
{
  noteList.clear();
  ampEnv.noteOff();
  currentNoteMuted = false;
  currentNote = -1;
  currentVel  = 0;
}

void Open303::triggerNote(int noteNumber, bool hasAccent)
{
  // retrigger osc and reset filter buffers only if amplitude is near zero (to avoid clicks):
  if( idle )
  {
    oscillator.resetPhase();
    junoOscillator.reset();       // center voice at phase 0 with the sub-oscillator, side voices randomized
    phaseshaper.resetPhase();
    subOscillator.resetPhase();
    resetFilters();
    ampDeClicker.reset();
  }

  noteAccented = hasAccent;
  drawRandomModValue();
  currentDecay = hasAccent ? accentDecay : normalDecay;
  if( hasAccent )
  {
    accentGain = accent;
    setMainEnvDecay(accentDecay);
    ampEnv.setRelease(accentAmpRelease);
  }
  else
  {
    accentGain = 0.0;
    setMainEnvDecay(normalDecay);
    ampEnv.setRelease(normalAmpRelease);
  }

  oscFreq = noteToHz(noteNumber);
  // Seed the slew limiter in the log-frequency domain (it glides log(freq), see getSample), so a
  // freshly triggered note starts exactly on pitch with no glide.
  pitchSlewLimiter.setState(log(oscFreq));
  mainEnv.trigger();
  ampEnv.noteOn(true, noteNumber, 64);
  if( lfoKeySync )
    lfo.reset(lfoPhase);   // slid notes (slideToNote) deliberately do not retrigger the LFO
  idle = false;

  // fresh (non-slide) note: no reversed portamento glide
  reverseFreqFrom  = oscFreq;
  reverseLogRatio  = 0.0;
  reverseGlideBase = 0.0;
  reverseInstFreq  = oscFreq;
  reverseSwellNow  = 0.0;

  // reset the reverse-gate swell and predict its length so its peak lands at the note's end. In
  // sequencer mode the step length is known exactly; otherwise we repeat the previous note/group's
  // measured length (steady patterns land dead-on), falling back to a Decay-derived time.
  reversePhase       = 0.0;
  reverseLastOnPhase = 0.0;
  reverseStep        = 0.0;
  if( sequencer.getSequencerMode() != AcidSequencer::OFF && sequencer.isRunning() )
    reverseLength = sequencer.getStepLengthInSamples();
  else
    reverseLength = predictReverseLength();
  updateReverseShape();
}

double Open303::predictReverseLength()
{
  // first note (no history yet): fall back to a Decay-derived time
  if( reverseHistCount == 0 )
    return 0.001 * mainEnv.getDecayTimeConstant() * sampleRate * 2.5;

  // short-biased estimate: a low percentile of recent measured lengths. Because the swell holds at
  // its peak (reverseHoldAt in getSample), UNDER-predicting is graceful (peak lands early, then
  // holds) while OVER-predicting starves a short note of its swell - so we deliberately lean short
  // to keep varied patterns (e.g. a short note after a long one) from dropping out.
  double sorted[8];
  int n = reverseHistCount;
  for(int i=0; i<n; i++) sorted[i] = reverseHist[i];
  for(int i=0; i<n-1; i++)                 // tiny insertion sort (n <= 8)
    for(int j=i+1; j<n; j++)
      if( sorted[j] < sorted[i] ) { double t=sorted[i]; sorted[i]=sorted[j]; sorted[j]=t; }
  double base = sorted[n/4];               // ~25th percentile

  // tempo snap: quantize to the nearest 16th-note multiple (min one 16th) when the host tempo is
  // known - acid lines are grid-locked, so this removes measurement jitter and lands the peak on
  // the musical grid.
  if( reverseTempoBpm > 0.0 )
  {
    double t16 = sampleRate * 60.0 / reverseTempoBpm / 4.0;   // one 16th note, in samples
    if( t16 >= 1.0 )
    {
      double mult = floor(base / t16 + 0.5);
      if( mult < 1.0 ) mult = 1.0;
      base = mult * t16;
    }
  }
  return base;
}

void Open303::updateReverseShape()
{
  if( reverseLength < 1.0 )
    reverseLength = 1.0;

  // auto-shape for a faithful reversal: the time-reverse of the forward amplitude decay e^(-t/tau)
  // over a note of length L is the floored swell e^(K*(r-1)) with K = L/tau (see getSample). So set
  // the swell shape from the note length divided by the amp-envelope decay time - this makes the
  // reverse gate match an actual "played backwards" note regardless of tempo or decay setting.
  // The upper clamp bounds the swell's head floor at e^-K (~-22 dB at K=2.5): this keeps reverse
  // notes as loud as forward ones and guarantees a note that ends before the predicted
  // reverseLength still rises well above silence instead of sounding "skipped".
  double ampDecaySamples = 0.001 * ampEnv.getDecay() * sampleRate;
  if( ampDecaySamples < 1.0 )
    ampDecaySamples = 1.0;
  double k = reverseLength / ampDecaySamples;
  if( k < 0.1 ) k = 0.1;    // keep the glide normalizer (e^k - 1) well-conditioned
  if( k > 2.5 ) k = 2.5;    // head floor >= e^-2.5: loudness/audibility guarantee
  reverseShape = k;

  // the reversed filter/accent sweep mirrors the *main* (filter) envelope's decay, so the Decay
  // knob shapes the reverse sweep just like it shapes the forward one. A wider clamp keeps more
  // sweep drama; audibility is governed by the amplitude swell above, not by this one.
  double mainDecaySamples = 0.001 * mainEnv.getDecayTimeConstant() * sampleRate;
  if( mainDecaySamples < 1.0 )
    mainDecaySamples = 1.0;
  double km = reverseLength / mainDecaySamples;
  if( km < 0.1 ) km = 0.1;
  if( km > 4.0 ) km = 4.0;
  reverseShapeMain = km;
  reverseDen       = exp(km) - 1.0;   // cached normalizer for the glide's swell
}

void Open303::extendReverseGroup()
{
  // A slide/hammer extends the current note into a tied group, so the reverse swell must keep
  // climbing toward the (still unknown) group end rather than peaking at the first note. We don't
  // reset the phase; instead we measure the note-on spacing and project the horizon at least one
  // step past the latest note. Only adopt the projection when we have no better prediction (first
  // group) or when the group is outlasting the predicted length - this keeps the swell from
  // plateauing early while preserving an accurate cross-group prediction for steady patterns.
  double step = reversePhase - reverseLastOnPhase;
  reverseLastOnPhase = reversePhase;
  if( step > 0.0 )
    reverseStep = step;
  double projected = reversePhase + reverseStep;
  if( reverseMeasured <= 0.0 || projected > reverseLength )
  {
    reverseLength = projected;
    updateReverseShape();
  }
}

void Open303::slideToNote(int noteNumber, bool hasAccent)
{
  oscFreq = noteToHz(noteNumber);

  noteAccented = hasAccent;
  drawRandomModValue();
  currentDecay = hasAccent ? accentDecay : normalDecay;

  if( hasAccent )
  {
    accentGain = accent;
    setMainEnvDecay(accentDecay);
    ampEnv.setRelease(accentAmpRelease);
  }
  else
  {
    accentGain = 0.0;
    setMainEnvDecay(normalDecay);
    ampEnv.setRelease(normalAmpRelease);
  }
  idle = false;
  extendReverseGroup();
  anchorReverseGlide(false);
}

void Open303::anchorReverseGlide(bool jump)
{
  // inverse portamento: glide from wherever the pitch is now toward oscFreq, along the rest of the
  // swell so it arrives in the note's audible end (see getSample). Anchored after the swell's length
  // is final, at the current pitch and swell, so it neither jumps nor stalls across mid-group slides.
  reverseFreqFrom  = jump ? oscFreq : reverseInstFreq;
  reverseLogRatio  = log(oscFreq / reverseFreqFrom);
  reverseGlideBase = reverseSwellNow = reverseGlideSwell();
}

void Open303::hammerToNote(int noteNumber, bool hasAccent)
{
  // Hammer is a slide with an instant pitch change: snap the slew limiter to the new pitch
  slideToNote(noteNumber, hasAccent);
  pitchSlewLimiter.setState(log(oscFreq));  // the slew runs on log(freq)
  anchorReverseGlide(true);   // reverse mode: no inverse portamento either
}

void Open303::releaseNote(int noteNumber)
{
  // check if the note-list is empty now. if so, trigger a release, otherwise slide to the note
  // at the beginning of the list (this is the most recent one which is still in the list). this
  // initiates a slide back to the most recent note that is still being held:
  if( noteList.empty() )
  {
    //filterEnvelope.noteOff();
    ampEnv.noteOff();
    // remember how long this note lasted, to predict the next note's reverse-swell length:
    if( reversePhase > 0.0 )
    {
      reverseMeasured = reversePhase;
      reverseHist[reverseHistPos] = reversePhase;   // feed the prediction history ring buffer
      reverseHistPos = (reverseHistPos + 1) % 8;
      if( reverseHistCount < 8 ) reverseHistCount++;
    }
  }
  else
  {
    // initiate slide back:
    oscFreq     = noteToHz(currentNote);
    anchorReverseGlide(false);   // in reverse too
  }
}

bool Open303::installPitchMap(const std::array<double, TuningTable::kNumNotes>& frequencies)
{
  if( !TuningTable::validateFrequencies(frequencies) )
    return false;
  const int write = 1 - pitchBank.load(std::memory_order_relaxed);
  pitchBanks[write] = frequencies;
  pitchBank.store(write, std::memory_order_release);
  return true;
}

double Open303::noteToHz(int noteNumber) const
{
  noteNumber = clip(noteNumber, 0, TuningTable::kNumNotes - 1);
  return pitchBanks[pitchBank.load(std::memory_order_acquire)][(size_t) noteNumber] * tuning / 440.0;
}

void Open303::setMainEnvDecay(double newDecay)
{
  mainEnv.setDecayTimeConstant(newDecay);
  updateNormalizer1();
  updateNormalizer2();
}

void Open303::calculateEnvModScalerAndOffset(double envModPercent)
{
  bool useMeasuredMapping = true; // might be shown as user parameter later
  if( useMeasuredMapping == true )
  {
    // define some constants that arise from the measurements:
    const double c0   = 3.138152786059267e+002;  // lowest nominal cutoff
    const double c1   = 2.394411986817546e+003;  // highest nominal cutoff
    const double oF   = 0.048292930943553;       // factor in line equation for offset
    const double oC   = 0.294391201442418;       // constant in line equation for offset
    const double sLoF = 3.773996325111173;       // factor in line eq. for scaler at low cutoff
    const double sLoC = 0.736965594166206;       // constant in line eq. for scaler at low cutoff
    const double sHiF = 4.194548788411135;       // factor in line eq. for scaler at high cutoff
    const double sHiC = 0.864344900642434;       // constant in line eq. for scaler at high cutoff

    // do the calculation of the scaler and offset:
    double e   = linToLin(envModPercent, 0.0, 100.0, 0.0, 1.0);
    double c   = expToLin(cutoff, c0,   c1,   0.0, 1.0);
    double sLo = sLoF*e + sLoC;
    double sHi = sHiF*e + sHiC;
    envScaler  = (1-c)*sLo + c*sHi;
    envOffset  =  oF*c + oC;
  }
  else
  {
    double upRatio   = pitchOffsetToFreqFactor(      envUpFraction *envModPercent);
    double downRatio = pitchOffsetToFreqFactor(-(1.0-envUpFraction)*envModPercent);
    envScaler        = upRatio - downRatio;
    if( envScaler != 0.0 ) // avoid division by zero
      envOffset = - (downRatio - 1.0) / (upRatio - downRatio);
    else
      envOffset = 0.0;
  }
}

void Open303::updateNormalizer1()
{
  // Normalization intentionally disabled: the leaky integrator rc1 runs unnormalized (n1 = 1) to
  // preserve the envelope depth this emulation was voiced against. The analytic
  // LeakyIntegrator::getNormalizer() compensation was previously computed here and then immediately
  // overwritten (the "// test" line), i.e. dead - so the call has been removed.
  n1 = 1.0;
}

void Open303::updateAccentSweepGain()
{
  const double stock = 15.0;
  // Makeup tracks the sweep knob only; the resonance stretch is left
  // un-normalized so it also lowers the accent peak, like a real analog cap.
  const double sweep = accentAttack;
  if( sweep == stock )
    accentSweepGain = 1.0;
  else
    accentSweepGain = LeakyIntegrator::getNormalizer(accentDecay, sweep, sampleRate)
                    / LeakyIntegrator::getNormalizer(accentDecay, stock, sampleRate);
}

void Open303::updateNormalizer2()
{
  n2 = LeakyIntegrator::getNormalizer(mainEnv.getDecayTimeConstant(), rc2.getTimeConstant(),
    sampleRate);
  n2 = 1.0; // test
}
