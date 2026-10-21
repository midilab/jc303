#include "rosic_Open303.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

static int failures = 0;

#define CHECK(cond)                                                      \
  do {                                                                   \
    if(!(cond)) {                                                        \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
      failures++;                                                        \
    }                                                                    \
  } while(0)

using Config = std::function<void(rosic::Open303&)>;

static std::vector<double> render(const Config& config, int n = 22050)
{
  rosic::Open303 synth;
  synth.setSampleRate(44100.0);
  synth.setFilterType(dfl::FILTER_DIODE);
  synth.setCutoff(800.0);
  synth.setResonance(60.0);
  synth.setFilterFmDepth(0.3);
  synth.setLfoWaveform(0);
  synth.setLfoRate(3.0);
  synth.setLfoDepth(0.0);
  synth.setLfoOn(true);
  config(synth);
  synth.noteOn(45, 100, 0.0);

  std::vector<double> out(n);
  for(int i = 0; i < n; i++)
    out[i] = synth.getSample();
  return out;
}

static double maxDiff(const std::vector<double>& a, const std::vector<double>& b)
{
  double d = 0.0;
  for(size_t i = 0; i < a.size(); i++)
    d = std::fmax(d, std::fabs(a[i] - b[i]));
  return d;
}

static const Config none = [](rosic::Open303&) {};

static void lfoSlotMatchesLegacyDepth()
{
  for(int dest = rosic::MOD_DEST_CUTOFF; dest <= rosic::MOD_DEST_FILTER_FM; dest++)
  {
    std::vector<double> legacy = render([&](rosic::Open303& s) {
      s.setLfoDepth(0.7); s.setLfoDestination(dest); });
    std::vector<double> slot = render([&](rosic::Open303& s) {
      s.setModSlot(2, rosic::MOD_SRC_LFO, dest, 0.7); });
    CHECK(maxDiff(legacy, slot) < 1e-12);
  }
}

static void slotsAddUp()
{
  std::vector<double> full = render([](rosic::Open303& s) {
    s.setLfoDepth(1.0); s.setLfoDestination(rosic::MOD_DEST_CUTOFF); });
  std::vector<double> split = render([](rosic::Open303& s) {
    s.setLfoDepth(0.5); s.setLfoDestination(rosic::MOD_DEST_CUTOFF);
    s.setModSlot(2, rosic::MOD_SRC_LFO, rosic::MOD_DEST_CUTOFF, 0.5); });
  CHECK(maxDiff(full, split) < 1e-9);
}

static void negativeAmountInvertsTheLfo()
{
  std::vector<double> pos = render([](rosic::Open303& s) {
    s.setModSlot(2, rosic::MOD_SRC_LFO, rosic::MOD_DEST_CUTOFF, 0.6); });
  std::vector<double> neg = render([](rosic::Open303& s) {
    s.setModSlot(2, rosic::MOD_SRC_LFO, rosic::MOD_DEST_CUTOFF, -0.6); });
  CHECK(maxDiff(pos, neg) > 1e-3);
}

static void envelopeSourceMovesEveryDestination()
{
  std::vector<double> base = render(none);
  for(int dest : { rosic::MOD_DEST_CUTOFF, rosic::MOD_DEST_VOLUME, rosic::MOD_DEST_PITCH,
                   rosic::MOD_DEST_RESONANCE, rosic::MOD_DEST_FILTER_FM })
  {
    std::vector<double> mod = render([&](rosic::Open303& s) {
      s.setModSlot(1, rosic::MOD_SRC_ENV, dest, 0.8); });
    CHECK(maxDiff(base, mod) > 1e-3);
  }

  // pulse width only shapes the circuit square
  const Config square = [](rosic::Open303& s) { s.setWaveform(1.0); s.setSquareModel(rosic::SQUARE_CIRCUIT); };
  std::vector<double> squareBase = render(square);
  std::vector<double> squareMod = render([&](rosic::Open303& s) {
    square(s); s.setModSlot(1, rosic::MOD_SRC_ENV, rosic::MOD_DEST_PULSE_WIDTH, 0.8); });
  CHECK(maxDiff(squareBase, squareMod) > 1e-3);

  // overdrive is applied by the host: the core only reports the offset
  rosic::Open303 s;
  s.setSampleRate(44100.0);
  s.setLfoOn(true);
  s.setModSlot(1, rosic::MOD_SRC_ENV, rosic::MOD_DEST_OVERDRIVE, 1.0);
  s.noteOn(45, 100, 0.0);
  s.getSample();
  CHECK(s.getLfoOverdriveMod() != 0.0);
}

static void offSlotAndZeroAmountAreTransparent()
{
  std::vector<double> base = render(none);
  CHECK(maxDiff(base, render([](rosic::Open303& s) {
    s.setModSlot(2, rosic::MOD_SRC_OFF, rosic::MOD_DEST_CUTOFF, 1.0); })) < 1e-12);
  CHECK(maxDiff(base, render([](rosic::Open303& s) {
    s.setModSlot(1, rosic::MOD_SRC_OFF, rosic::MOD_DEST_CUTOFF, 0.0); })) < 1e-12);
}

static void envelopeSlotIsHardwiredAndBipolar()
{
  // slot 1 is always the envelope, whatever source is passed
  std::vector<double> env = render([](rosic::Open303& s) {
    s.setModSlot(1, rosic::MOD_SRC_ENV, rosic::MOD_DEST_CUTOFF, 0.8); });
  std::vector<double> off = render([](rosic::Open303& s) {
    s.setModSlot(1, rosic::MOD_SRC_OFF, rosic::MOD_DEST_CUTOFF, 0.8); });
  CHECK(maxDiff(env, off) < 1e-12);

  // bipolar: positive amount pushes up while the envelope is high and below the knob value
  // once it has decayed
  for(int slot : { 1, 2, 3 })   // free slots with the Env source are bipolar too
  {
    rosic::Open303 s;
    s.setSampleRate(44100.0);
    s.setLfoOn(true);
    s.setModSlot(slot, rosic::MOD_SRC_ENV, rosic::MOD_DEST_OVERDRIVE, 1.0);
    s.noteOn(45, 100, 0.0);
    s.getSample();
    double early = s.getLfoOverdriveMod();
    for(int i = 0; i < 44100; i++)
      s.getSample();
    double late = s.getLfoOverdriveMod();
    CHECK(early > 0.0);
    CHECK(late < 0.0);
  }
}

static void modsOffDisablesTheMatrix()
{
  std::vector<double> base = render([](rosic::Open303& s) { s.setLfoOn(false); });
  std::vector<double> mod = render([](rosic::Open303& s) {
    s.setLfoOn(false);
    s.setModSlot(1, rosic::MOD_SRC_ENV, rosic::MOD_DEST_CUTOFF, 1.0);
    s.setModSlot(2, rosic::MOD_SRC_LFO, rosic::MOD_DEST_PITCH, 1.0); });
  CHECK(maxDiff(base, mod) < 1e-12);
}

static void slotZeroIsOwnedByLegacySetters()
{
  std::vector<double> base = render(none);
  std::vector<double> mod = render([](rosic::Open303& s) {
    s.setModSlot(0, rosic::MOD_SRC_LFO, rosic::MOD_DEST_PITCH, 1.0); });
  CHECK(maxDiff(base, mod) < 1e-12);
}

static void newDestinationsMoveTheSound()
{
  // accented note (velocity 100) with the accent knob up so the accent destination has something
  // to scale; a cutoff LFO so the LFO rate / depth destinations are audible
  Config knobs = [](rosic::Open303& s) {
    s.setAccent(50.0);
    s.setLfoDepth(0.5); s.setLfoDestination(rosic::MOD_DEST_CUTOFF); };
  std::vector<double> base = render(knobs);
  for(int dest : { rosic::MOD_DEST_ENV_MOD, rosic::MOD_DEST_DECAY, rosic::MOD_DEST_ACCENT,
                   rosic::MOD_DEST_WAVEFORM, rosic::MOD_DEST_LFO_RATE, rosic::MOD_DEST_LFO_DEPTH,
                   rosic::MOD_DEST_FILTER_MORPH })
  {
    std::vector<double> mod = render([&](rosic::Open303& s) {
      knobs(s); s.setModSlot(1, rosic::MOD_SRC_ENV, dest, 0.8); });
    CHECK(maxDiff(base, mod) > 1e-3);
  }
}

static void waveformAndPulseWidthReachEveryOscModel()
{
  // the Waveform destination is the Juno pulse morph and the phaseshaper shape; Pulse Width is the Juno chop
  struct Case { rosic::OscModel model; int dest; };
  for(Case c : { Case{ rosic::OSC_ALPHA_JUNO, rosic::MOD_DEST_WAVEFORM }, Case{ rosic::OSC_ALPHA_JUNO, rosic::MOD_DEST_PULSE_WIDTH },
                 Case{ rosic::OSC_PHASESHAPER, rosic::MOD_DEST_WAVEFORM } })
  {
    Config osc = [c](rosic::Open303& s) {
      s.setOscModel(c.model);
      s.setJunoWaveform(2);   // SAW03: the chop follows the pulse width
      s.setWaveform(0.3); };
    std::vector<double> base = render(osc);
    std::vector<double> mod = render([&](rosic::Open303& s) {
      osc(s); s.setModSlot(1, rosic::MOD_SRC_ENV, c.dest, 0.8); });
    CHECK(maxDiff(base, mod) > 1e-3);
  }
}

static void formantModeTakesWaveformAndSplit()
{
  Config formant = [](rosic::Open303& s) {
    s.setOscModel(rosic::OSC_PHASESHAPER);
    s.setPhaseshaperMode(dfl::PhaseshaperOscillator::FORMANT);
    s.setWaveform(0.3); };
  std::vector<double> base = render(formant);
  CHECK(maxDiff(base, render([&](rosic::Open303& s) { formant(s); s.setPhaseshaperSplit(0.6); })) > 1e-3);
  CHECK(maxDiff(base, render([&](rosic::Open303& s) {
    formant(s); s.setModSlot(1, rosic::MOD_SRC_ENV, rosic::MOD_DEST_WAVEFORM, 0.8); })) > 1e-3);
}

// period (samples) by autocorrelation over a window, searched between 1 and 1000 Hz at 44.1 kHz
static double periodOf(const std::vector<double>& x, int from, int len)
{
  double best = -1e300; int bestLag = 0;
  for(int lag = 44; lag < 1000; lag++)
  {
    double acc = 0.0;
    for(int i = from; i < from + len; i++)
      acc += x[i] * x[i + lag];
    if( acc > best ) { best = acc; bestLag = lag; }
  }
  return bestLag;
}

static void envPlusIsUnipolarAndSettlesOnPitch()
{
  // a bright, short-decay note; the envelope's RC tail takes about 2 s to die out
  Config bright = [](rosic::Open303& s) {
    s.setCutoff(2000.0); s.setResonance(0.0); s.setEnvMod(0.0); s.setDecay(100.0); };
  const int n = 3 * 44100;
  std::vector<double> base = render(bright, n);
  std::vector<double> plus = render([&](rosic::Open303& s) {
    bright(s); s.setModSlot(2, rosic::MOD_SRC_ENV_PLUS, rosic::MOD_DEST_PITCH, 0.5); }, n);
  std::vector<double> bipolar = render([&](rosic::Open303& s) {
    bright(s); s.setModSlot(2, rosic::MOD_SRC_ENV, rosic::MOD_DEST_PITCH, 0.5); }, n);

  const int late = n - 6000, early = 200;
  CHECK(periodOf(plus, early, 2000) < periodOf(base, early, 2000) - 20.0);   // starts sharp
  CHECK(std::fabs(periodOf(plus, late, 4000) - periodOf(base, late, 4000)) <= 1.0);   // lands on the note
  CHECK(periodOf(bipolar, late, 4000) > periodOf(base, late, 4000) + 20.0);   // bipolar Env rests flat
}

static void lfoDepthDestinationCanOpenAClosedLfo()
{
  // LFO depth knob at zero: the envelope fades the cutoff LFO in at the attack
  Config lfoOff = [](rosic::Open303& s) { s.setLfoDestination(rosic::MOD_DEST_CUTOFF); };
  std::vector<double> base = render(lfoOff);
  std::vector<double> mod = render([&](rosic::Open303& s) {
    lfoOff(s); s.setModSlot(1, rosic::MOD_SRC_ENV, rosic::MOD_DEST_LFO_DEPTH, 1.0); });
  CHECK(maxDiff(base, mod) > 1e-3);
}

static void knobValuesAreRestoredWhenModulationStops()
{
  auto modulated = [](rosic::Open303& s, int dest) {
    if( dest == rosic::MOD_DEST_WAVEFORM ) return s.oscillator.getBlendFactor() != 0.25;
    if( dest == rosic::MOD_DEST_LFO_RATE ) return s.lfo.getRate() != 3.0;
    return s.mainEnv.getDecayTimeConstant() != s.getDecay();
  };
  for(int dest : { rosic::MOD_DEST_WAVEFORM, rosic::MOD_DEST_DECAY, rosic::MOD_DEST_LFO_RATE,
                   rosic::MOD_DEST_ENV_MOD })
  {
    rosic::Open303 s;
    s.setSampleRate(44100.0);
    s.setLfoOn(true);
    s.setWaveform(0.25);
    s.setEnvMod(40.0);
    s.setLfoRate(3.0);
    s.setModSlot(2, rosic::MOD_SRC_ENV, dest, 1.0);
    s.noteOn(45, 64, 0.0);
    for(int i = 0; i < 1000; i++)
      s.getSample();
    if( dest != rosic::MOD_DEST_ENV_MOD )
      CHECK(modulated(s, dest));

    // getters report the knob, not the modulated value
    CHECK(s.getWaveform() == 0.25);
    CHECK(s.getEnvMod() == 40.0);

    s.setModSlot(2, rosic::MOD_SRC_ENV, dest, 0.0);
    s.getSample();
    if( dest != rosic::MOD_DEST_ENV_MOD )
      CHECK(!modulated(s, dest));
  }
}

static void accentDestinationLeavesUnaccentedNotesAlone()
{
  auto renderSoft = [](const Config& config) {
    rosic::Open303 s;
    s.setSampleRate(44100.0);
    s.setAccent(50.0);
    s.setLfoOn(true);
    config(s);
    s.noteOn(45, 64, 0.0);   // below the accent threshold
    std::vector<double> out(8192);
    for(double& x : out)
      x = s.getSample();
    return out;
  };
  std::vector<double> base = renderSoft(none);
  std::vector<double> mod = renderSoft([](rosic::Open303& s) {
    s.setModSlot(1, rosic::MOD_SRC_ENV, rosic::MOD_DEST_ACCENT, 1.0); });
  CHECK(maxDiff(base, mod) < 1e-12);
}

static std::vector<double> renderNote(int note, int velocity, const Config& config, int n = 8192)
{
  rosic::Open303 s;
  s.setSampleRate(44100.0);
  s.setLfoOn(true);
  config(s);
  s.noteOn(note, velocity, 0.0);
  std::vector<double> out(n);
  for(double& x : out)
    x = s.getSample();
  return out;
}

static void accentSourceIsAGateOnAccentedNotes()
{
  Config mod = [](rosic::Open303& s) {
    s.setModSlot(2, rosic::MOD_SRC_ACCENT, rosic::MOD_DEST_PITCH, 1.0); };
  // unaccented: gate is 0, nothing moves
  CHECK(maxDiff(renderNote(45, 64, none), renderNote(45, 64, mod)) < 1e-12);
  // accented: works even with the accent knob at zero
  CHECK(maxDiff(renderNote(45, 100, none), renderNote(45, 100, mod)) > 1e-3);
}

static void keytrackIsZeroAtTheCenterNote()
{
  Config mod = [](rosic::Open303& s) {
    s.setModSlot(2, rosic::MOD_SRC_KEYTRACK, rosic::MOD_DEST_CUTOFF, 1.0); };
  CHECK(maxDiff(renderNote(48, 64, none), renderNote(48, 64, mod)) < 1e-9);
  CHECK(maxDiff(renderNote(60, 64, none), renderNote(60, 64, mod)) > 1e-3);
}

static void randomSourceChangesPerNoteAndIsRepeatable()
{
  auto run = [](bool modulate) {
    rosic::Open303 s;
    s.setSampleRate(44100.0);
    s.setLfoOn(true);
    if( modulate )
      s.setModSlot(2, rosic::MOD_SRC_RANDOM, rosic::MOD_DEST_PITCH, 1.0);
    std::vector<double> out;
    for(int n = 0; n < 4; n++)
    {
      s.noteOn(45, 64, 0.0);
      for(int i = 0; i < 4000; i++)
        out.push_back(s.getSample());
      s.noteOn(45, 0, 0.0);
    }
    return out;
  };
  std::vector<double> a = run(true), b = run(true);
  CHECK(maxDiff(a, b) < 1e-12);
  CHECK(maxDiff(a, run(false)) > 1e-3);

  rosic::Open303 s;
  s.setSampleRate(44100.0);
  double first = 0.0;
  bool changed = false;
  for(int n = 0; n < 16; n++)
  {
    s.noteOn(45, 64, 0.0);
    double v = s.getRandomModValue();
    CHECK(v >= -1.0 && v <= 1.0);
    if( n == 0 ) first = v;
    else if( v != first ) changed = true;
    s.noteOn(45, 0, 0.0);
  }
  CHECK(changed);
}

static void cutoffKnobSourceFollowsTheKnobPosition()
{
  rosic::Open303 s;
  double knob1k = std::log(1000.0 / rosic::Open303::CUTOFF_KNOB_MIN)
                  / std::log(rosic::Open303::CUTOFF_KNOB_MAX / rosic::Open303::CUTOFF_KNOB_MIN);
  CHECK(std::fabs(s.getCutoff() - 1000.0) < 1e-12);
  CHECK(std::fabs(s.getCutoffKnobModValue() - knob1k) < 1e-12);
  s.setCutoff(rosic::Open303::CUTOFF_KNOB_MIN);
  CHECK(std::fabs(s.getCutoffKnobModValue()) < 1e-12);
  s.setCutoff(rosic::Open303::CUTOFF_KNOB_MAX);
  CHECK(std::fabs(s.getCutoffKnobModValue() - 1.0) < 1e-12);
  s.setCutoff(std::sqrt(rosic::Open303::CUTOFF_KNOB_MIN * rosic::Open303::CUTOFF_KNOB_MAX));
  CHECK(std::fabs(s.getCutoffKnobModValue() - 0.5) < 1e-12);

  auto at = [](double cutoff) {
    return [cutoff](rosic::Open303& s) {
      s.setCutoff(cutoff);
      s.setModSlot(2, rosic::MOD_SRC_CUTOFF, rosic::MOD_DEST_PITCH, 1.0); };
  };
  auto plain = [](double cutoff) {
    return [cutoff](rosic::Open303& s) { s.setCutoff(cutoff); };
  };
  // knob fully left: source is 0, nothing moves; open knob shifts the pitch
  CHECK(maxDiff(renderNote(45, 64, plain(rosic::Open303::CUTOFF_KNOB_MIN)),
                renderNote(45, 64, at(rosic::Open303::CUTOFF_KNOB_MIN))) < 1e-12);
  CHECK(maxDiff(renderNote(45, 64, plain(1000.0)), renderNote(45, 64, at(1000.0))) > 1e-3);
}

static void leakyNormalizerHandlesEqualTimeConstants()
{
  // Two cascaded identical one-poles peak at n = fs*tau; brute-force the peak to compare.
  const double fs = 44100.0, tau = 15.0;
  const double x = std::exp(-1.0 / (fs * 0.001 * tau)), b = 1.0 - x;
  double y1 = 0.0, y2 = 0.0, peak = 0.0;
  for(int n = 0; n < 10000; n++)
  {
    y1 = b * (n == 0 ? 1.0 : 0.0) + x * y1;
    y2 = b * y1 + x * y2;
    peak = std::max(peak, y2);
  }
  const double norm = rosic::LeakyIntegrator::getNormalizer(tau, tau, fs);
  CHECK(std::isfinite(norm));
  CHECK(std::fabs(norm * peak - 1.0) < 1e-3);
}

static void cutoffMovesGlide()
{
  // the anti-zipper smoother works in log frequency: a tenth of an octave is not there after 1 ms (it was there in a
  // few samples when the smoother was fed Hz), and is there after 200 ms
  auto after = [](double to, int samples)
  {
    rosic::Open303 synth;
    synth.setSampleRate(44100.0);
    synth.setFilterType(dfl::FILTER_DIODE);
    synth.setCutoff(600.0);
    synth.setResonance(0.0);
    synth.setEnvMod(0.0);
    synth.noteOn(45, 64, 0.0);
    for(int i = 0; i < 4410; i++)
      synth.getSample();
    synth.setCutoff(to);
    for(int i = 0; i < samples; i++)
      synth.getSample();
    return synth.getFilterCutoff();
  };
  const double to = 600.0 * std::exp2(0.1);
  auto fraction = [&](int samples)
  {
    const double lo = after(600.0, samples);
    return std::log2(after(to, samples) / lo) / 0.1;
  };
  CHECK(fraction(44) < 0.6);
  CHECK(std::fabs(fraction(8820) - 1.0) < 0.01);
}

static void slideBackHonoursTheTuning()
{
  // releasing a slid-to note glides back to the held note at the master tuning, not at A4 = 440 Hz
  rosic::Open303 synth;
  synth.setSampleRate(44100.0);
  synth.setCutoff(500.0);
  synth.setResonance(0.0);
  synth.setEnvMod(0.0);
  synth.setTuning(415.0);
  synth.noteOn(45, 64, 0.0);
  synth.noteOn(57, 64, 0.0);
  for(int i = 0; i < 8820; i++)
    synth.getSample();
  synth.noteOn(57, 0, 0.0);

  // fundamental from upward zero crossings of a lowpassed copy, after the glide settles
  double lp = 0.0, prev = 0.0;
  int first = -1, last = -1, count = 0;
  for(int i = 0; i < 44100; i++)
  {
    lp += 0.02 * (synth.getSample() - lp);
    if(i >= 13230 && prev <= 0.0 && lp > 0.0)
    {
      if(first < 0) first = i; else { last = i; count++; }
    }
    prev = lp;
  }
  const double hz = count * 44100.0 / (last - first);
  CHECK(std::fabs(hz - 415.0 / 4.0) < 1.0);   // A2 at A4 = 415 Hz
}

static double rmsOf(const std::vector<double>& y, size_t from, size_t to)
{
  double e = 0.0;
  for(size_t i = from; i < to; i++)
    e += y[i] * y[i];
  return std::sqrt(e / (to - from));
}

static void silenceSleepsAndNotesWake()
{
  // after a release the voice sleeps (exact zeros, no oscillator or filter work); the next note wakes it sounding as
  // the first did
  rosic::Open303 synth;
  synth.setSampleRate(44100.0);
  synth.setFilterType(dfl::FILTER_DIODE);
  synth.setCutoff(800.0);
  synth.setResonance(60.0);
  std::vector<double> first(8820), tail(88200), again(8820);
  synth.noteOn(45, 100, 0.0);
  for(auto& x : first) x = synth.getSample();
  synth.noteOn(45, 0, 0.0);
  CHECK(!synth.isAsleep());
  for(auto& x : tail) x = synth.getSample();
  CHECK(synth.isAsleep());
  bool zeros = true;
  for(size_t i = 66150; i < tail.size(); i++) zeros = zeros && tail[i] == 0.0;
  CHECK(zeros);
  synth.noteOn(45, 100, 0.0);
  CHECK(!synth.isAsleep());
  for(auto& x : again) x = synth.getSample();
  CHECK(std::fabs(20.0 * std::log10(rmsOf(again, 2205, 8820) / rmsOf(first, 2205, 8820))) < 0.5);

  // a held note never sleeps
  rosic::Open303 held;
  held.setSampleRate(44100.0);
  held.noteOn(45, 100, 0.0);
  for(int i = 0; i < 44100 * 20; i++) held.getSample();
  CHECK(!held.isAsleep());

  // a filter switch while asleep takes effect at once; the next note sounds through the new filter
  synth.noteOn(45, 0, 0.0);
  for(int i = 0; i < 88200; i++) synth.getSample();
  CHECK(synth.isAsleep());
  synth.setFilterType(dfl::FILTER_MS20);
  for(int i = 0; i < 100; i++) synth.getSample();
  synth.noteOn(45, 100, 0.0);
  for(auto& x : again) x = synth.getSample();
  CHECK(rmsOf(again, 2205, 8820) > 0.1 * rmsOf(first, 2205, 8820));
  CHECK(synth.getActiveFilterType() == dfl::FILTER_MS20);
}

int main()
{
  lfoSlotMatchesLegacyDepth();
  slotsAddUp();
  negativeAmountInvertsTheLfo();
  envelopeSourceMovesEveryDestination();
  offSlotAndZeroAmountAreTransparent();
  envelopeSlotIsHardwiredAndBipolar();
  modsOffDisablesTheMatrix();
  slotZeroIsOwnedByLegacySetters();
  newDestinationsMoveTheSound();
  waveformAndPulseWidthReachEveryOscModel();
  formantModeTakesWaveformAndSplit();
  envPlusIsUnipolarAndSettlesOnPitch();
  lfoDepthDestinationCanOpenAClosedLfo();
  knobValuesAreRestoredWhenModulationStops();
  accentDestinationLeavesUnaccentedNotesAlone();
  accentSourceIsAGateOnAccentedNotes();
  keytrackIsZeroAtTheCenterNote();
  randomSourceChangesPerNoteAndIsRepeatable();
  cutoffKnobSourceFollowsTheKnobPosition();
  leakyNormalizerHandlesEqualTimeConstants();
  slideBackHonoursTheTuning();
  cutoffMovesGlide();
  silenceSleepsAndNotesWake();
  if(failures == 0)
    std::printf("open303_mod_test: all passed\n");
  return failures == 0 ? 0 : 1;
}
