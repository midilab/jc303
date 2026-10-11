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
  subOscGain       =     0.0;
  subOscBlend      =     1.0;  // -1 octave
  currentNote      =    -1;
  currentVel       =     0;
  noteOffCountDown =     0;
  slideToNextNote  = false;
  idle             = true;
  currentFilterType = dfl::FILTER_TEEBEE;

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

  waveTable3.setWaveform(MipMappedWaveTable::SAW303);
  subOscillator.setWaveTable(&waveTable3);

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
  oscillator.setCircuitPulseShape(false);   // flat-topped square unless switched on (JC303: the Mods switch)
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
  lfo.setSampleRate(                    newSampleRate);

  highpass2.setSampleRate     (         newSampleRate);
  allpass.setSampleRate       (         newSampleRate);
  notch.setSampleRate         (         newSampleRate);
  outputNetwork.setSampleRate (         newSampleRate);

  highpass1.setSampleRate     (  oversampling*newSampleRate);

  oscillator.setSampleRate    (  oversampling*newSampleRate);
  subOscillator.setSampleRate (  oversampling*newSampleRate);
  filter.setSampleRate        (  oversampling*newSampleRate);
  diodeFilter.setSampleRate   (  oversampling*newSampleRate);
  sampleRate = newSampleRate;
  updateAccentSweepGain();
}

void Open303::setCutoff(double newCutoff)
{
  cutoff = newCutoff;
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
  outputNetwork.setPot(diodeFilter.getResonancePot());
}

void Open303::setFilterType(dfl::FilterType newType)
{
  currentFilterType = newType;
}

void Open303::setFilterDrive(double newDriveDb)
{
  // The diode ladder saturates its input (see DiodeLadderFilter::getSample),
  // so drive changes its timbre. TeeBee is linear in TB_303 mode - the call
  // is harmless there but has no audible effect.
  filter.setDrive(newDriveDb);
  diodeFilter.setInputDrive(newDriveDb);
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
    subOscillator.resetPhase();
    filter.reset();
    diodeFilter.reset();
    highpass1.reset();
    highpass2.reset();
    allpass.reset();
    notch.reset();
    outputNetwork.reset();
    antiAliasFilter.reset();
    ampDeClicker.reset();
  }

  noteAccented = hasAccent;
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

  oscFreq = pitchToFreq(noteNumber, tuning);
  pitchSlewLimiter.setState(oscFreq);
  mainEnv.trigger();
  ampEnv.noteOn(true, noteNumber, 64);
  if( lfoKeySync )
    lfo.reset(lfoPhase);   // slid notes (slideToNote) deliberately do not retrigger the LFO
  idle = false;
}

void Open303::slideToNote(int noteNumber, bool hasAccent)
{
  oscFreq = pitchToFreq(noteNumber, tuning);

  noteAccented = hasAccent;
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
  n1 = LeakyIntegrator::getNormalizer(mainEnv.getDecayTimeConstant(), rc1.getTimeConstant(),
    sampleRate);
  n1 = 1.0; // test
}

void Open303::updateAccentSweepGain()
{
  const double stock = 15.0;
  const double sweep = rc2.getTimeConstant();
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
