#include "rosic_Open303.h"
using namespace rosic;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

Open303::Open303()
{
  tuning           =   440.0;
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
  reverseDen       = exp(reverseShape) - 1.0;
  reverseShapeMain =     3.5;
  reverseFreqFrom  =   440.0;
  reverseLogRatio  =     0.0;
  reverseGlideBase =     0.0;
  reverseInstFreq  =   440.0;
  reverseSwellNow  =     0.0;
  slideTime        =    60.0;
  cutoff           =  1000.0;
  envUpFraction    =     2.0/3.0;
  normalAttack     =     3.0;
  accentAttack     =     3.0;
  normalDecay      =  1000.0;
  accentDecay      =   200.0;
  normalAmpRelease =     1.0;
  accentAmpRelease =    50.0;
  accentGain       =     0.0;
  pitchWheelFactor =     1.0;
  currentNote      =    -1;
  currentVel       =     0;
  noteOffCountDown =     0;
  slideToNextNote  = false;
  idle             = true;

  setEnvMod(25.0);

  oscillator.setWaveTable1(&waveTable1);
  oscillator.setWaveForm1(MipMappedWaveTable::SAW303);
  oscillator.setWaveTable2(&waveTable2);
  oscillator.setWaveForm2(MipMappedWaveTable::SQUARE303);

  //mainEnv.setNormalizeSum(true);
  mainEnv.setNormalizeSum(false);

  ampEnv.setAttack(0.0);
  ampEnv.setDecay(1230.0);
  ampEnv.setSustainLevel(0.0);
  ampEnv.setRelease(0.5);
  ampEnv.setTauScale(1.0);

  pitchSlewLimiter.setTimeConstant(60.0);
  //ampDeClicker.setTimeConstant(2.0);
  ampDeClicker.setMode(BiquadFilter::LOWPASS12);
  ampDeClicker.setGain( amp2dB(sqrt(0.5)) );
  ampDeClicker.setFrequency(200.0);

  rc1.setTimeConstant(0.0);
  rc2.setTimeConstant(15.0);

  highpass1.setMode(OnePoleFilter::HIGHPASS);
  highpass2.setMode(OnePoleFilter::HIGHPASS);
  allpass.setMode(OnePoleFilter::ALLPASS);
  notch.setMode(BiquadFilter::BANDREJECT);

  setSampleRate(sampleRate);

  // tweakables:
  oscillator.setPulseWidth(50.0);
  highpass1.setCutoff(44.486);
  highpass2.setCutoff(24.167);
  allpass.setCutoff(14.008);
  notch.setFrequency(7.5164);
  notch.setBandwidth(4.7);

  filter.setFeedbackHighpassCutoff(150.0);
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
  ampDeClicker.setSampleRate(    (float)newSampleRate);
  rc1.setSampleRate(             (float)newSampleRate);
  rc2.setSampleRate(             (float)newSampleRate);
  sequencer.setSampleRate(              newSampleRate);

  highpass2.setSampleRate     (         newSampleRate);
  allpass.setSampleRate       (         newSampleRate);
  notch.setSampleRate         (         newSampleRate);

  highpass1.setSampleRate     (  oversampling*newSampleRate);

  oscillator.setSampleRate    (  oversampling*newSampleRate);
  filter.setSampleRate        (  oversampling*newSampleRate);
}

void Open303::setCutoff(double newCutoff)
{
  cutoff = newCutoff;
  calculateEnvModScalerAndOffset();
}

void Open303::setEnvMod(double newEnvMod)
{
  envMod = newEnvMod;
  calculateEnvModScalerAndOffset();
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
    pitchSlewLimiter.setTimeConstant((float)(0.2*slideTime));  // \todo: tweak the scaling constant
  }
}

void Open303::setPitchBend(double newPitchBend)
{
  pitchWheelFactor = pitchOffsetToFreqFactor(newPitchBend);
}

//------------------------------------------------------------------------------------------------------------
// others:

void Open303::noteOn(int noteNumber, int velocity, double detune)
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
    if( noteList.empty() )
      triggerNote(noteNumber, velocity >= 100);
    else
      slideToNote(noteNumber, velocity >= 100);

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
  currentNote = -1;
  currentVel  = 0;
}

void Open303::triggerNote(int noteNumber, bool hasAccent)
{
  // retrigger osc and reset filter buffers only if amplitude is near zero (to avoid clicks):
  if( idle )
  {
    oscillator.resetPhase();
    filter.reset();
    highpass1.reset();
    highpass2.reset();
    allpass.reset();
    notch.reset();
    antiAliasFilter.reset();
    ampDeClicker.reset();
  }

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

  oscFreq = pitchToFreq(noteNumber, tuning);
  pitchSlewLimiter.setState(oscFreq);
  mainEnv.trigger();
  ampEnv.noteOn(true, noteNumber, 64);
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
  reverseDen   = exp(k) - 1.0;   // cached normalizer for the swell

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
}

void Open303::slideToNote(int noteNumber, bool hasAccent)
{
  oscFreq = pitchToFreq(noteNumber, tuning);

  // inverse portamento: glide from wherever the pitch currently is toward the new note, spanning the
  // remaining swell so it arrives during the note's audible tail (see getSample). Anchoring to the
  // current pitch and current swell keeps the glide continuous even across mid-group slides.
  reverseFreqFrom  = reverseInstFreq;
  reverseGlideBase = reverseSwellNow;
  reverseLogRatio  = log(oscFreq / reverseFreqFrom);

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

  // A slide extends the current note into a tied group, so the reverse swell must keep climbing
  // toward the (still unknown) group end rather than peaking at the first note. We don't reset the
  // phase; instead we measure the note-on spacing and project the horizon at least one step past the
  // latest note. Only adopt the projection when we have no better prediction (first group) or when
  // the group is outlasting the predicted length - this keeps the swell from plateauing early while
  // preserving an accurate cross-group prediction for steady patterns.
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
    oscFreq     = pitchToFreq(currentNote);
  }
}

void Open303::setMainEnvDecay(double newDecay)
{
  mainEnv.setDecayTimeConstant(newDecay);
  updateNormalizer1();
  updateNormalizer2();
}

void Open303::calculateEnvModScalerAndOffset()
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
    double e   = linToLin(envMod, 0.0, 100.0, 0.0, 1.0);
    double c   = expToLin(cutoff, c0,   c1,   0.0, 1.0);
    double sLo = sLoF*e + sLoC;
    double sHi = sHiF*e + sHiC;
    envScaler  = (1-c)*sLo + c*sHi;
    envOffset  =  oF*c + oC;
  }
  else
  {
    double upRatio   = pitchOffsetToFreqFactor(      envUpFraction *envMod);
    double downRatio = pitchOffsetToFreqFactor(-(1.0-envUpFraction)*envMod);
    envScaler        = upRatio - downRatio;
    if( envScaler != 0.0 ) // avoid division by zero
      envOffset = - (downRatio - 1.0) / (upRatio - downRatio);
    else
      envOffset = 0.0;
  }
}

void Open303::updateNormalizer1()
{
  n1 = LeakyIntegrator::getNormalizer(mainEnv.getDecayTimeConstant(), rc1.getTimeConstant(),
    sampleRate);
  n1 = 1.0; // test
}

void Open303::updateNormalizer2()
{
  n2 = LeakyIntegrator::getNormalizer(mainEnv.getDecayTimeConstant(), rc2.getTimeConstant(),
    sampleRate);
  n2 = 1.0; // test
}
