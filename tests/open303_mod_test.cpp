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
  for(int dest = 0; dest <= 5; dest++)
  {
    std::vector<double> legacy = render([&](rosic::Open303& s) {
      s.setLfoDepth(0.7); s.setLfoDestination(dest); });
    std::vector<double> slot = render([&](rosic::Open303& s) {
      s.setModSlot(1, rosic::MOD_SRC_LFO, dest, 0.7); });
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
    s.setModSlot(1, rosic::MOD_SRC_LFO, rosic::MOD_DEST_CUTOFF, 0.6); });
  std::vector<double> neg = render([](rosic::Open303& s) {
    s.setModSlot(1, rosic::MOD_SRC_LFO, rosic::MOD_DEST_CUTOFF, -0.6); });
  CHECK(maxDiff(pos, neg) > 1e-3);
}

static void envelopeSourceMovesEveryDestination()
{
  std::vector<double> base = render(none);
  for(int dest : { 0, 1, 2, 3, 5 })
  {
    std::vector<double> mod = render([&](rosic::Open303& s) {
      s.setModSlot(1, rosic::MOD_SRC_ENV, dest, 0.8); });
    CHECK(maxDiff(base, mod) > 1e-3);
  }

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
    s.setModSlot(1, rosic::MOD_SRC_OFF, rosic::MOD_DEST_CUTOFF, 1.0); })) < 1e-12);
  CHECK(maxDiff(base, render([](rosic::Open303& s) {
    s.setModSlot(1, rosic::MOD_SRC_ENV, rosic::MOD_DEST_CUTOFF, 0.0); })) < 1e-12);
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

int main()
{
  lfoSlotMatchesLegacyDepth();
  slotsAddUp();
  negativeAmountInvertsTheLfo();
  envelopeSourceMovesEveryDestination();
  offSlotAndZeroAmountAreTransparent();
  modsOffDisablesTheMatrix();
  slotZeroIsOwnedByLegacySetters();
  if(failures == 0)
    std::printf("open303_mod_test: all passed\n");
  return failures == 0 ? 0 : 1;
}
