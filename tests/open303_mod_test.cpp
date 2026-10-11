#include "rosic_Open303.h"

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
  synth.setFilterType(rosic::FILTER_DIODE);
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
                   rosic::MOD_DEST_WAVEFORM, rosic::MOD_DEST_LFO_RATE, rosic::MOD_DEST_LFO_DEPTH })
  {
    std::vector<double> mod = render([&](rosic::Open303& s) {
      knobs(s); s.setModSlot(1, rosic::MOD_SRC_ENV, dest, 0.8); });
    CHECK(maxDiff(base, mod) > 1e-3);
  }
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
  lfoDepthDestinationCanOpenAClosedLfo();
  knobValuesAreRestoredWhenModulationStops();
  accentDestinationLeavesUnaccentedNotesAlone();
  if(failures == 0)
    std::printf("open303_mod_test: all passed\n");
  return failures == 0 ? 0 : 1;
}
