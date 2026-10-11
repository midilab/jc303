#include "rosic_Open303.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int failures = 0;

#define CHECK(cond)                                                      \
  do {                                                                   \
    if(!(cond)) {                                                        \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
      failures++;                                                        \
    }                                                                    \
  } while(0)

struct Setup
{
  int    filterType  = dfl::FILTER_DIODE;   // FM and resonance both audible
  bool   lfoOn       = true;
  int    dest        = 0;
  double depth       = 1.0;
};

static std::vector<double> render(const Setup& s, int n = 22050)
{
  rosic::Open303 synth;
  synth.setSampleRate(44100.0);
  synth.setFilterType((dfl::FilterType) s.filterType);
  synth.setCutoff(800.0);
  synth.setResonance(60.0);
  synth.setFilterFmDepth(0.3);
  synth.setLfoWaveform(0);
  synth.setLfoRate(3.0);
  synth.setLfoDepth(s.depth);
  synth.setLfoDestination(s.dest);
  synth.setLfoOn(s.lfoOn);
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

static void everyDestinationChangesTheSound()
{
  Setup off; off.lfoOn = false;
  std::vector<double> base = render(off);

  // overdrive is handled in JC303, the core is untouched
  const int audible[] = { rosic::MOD_DEST_CUTOFF, rosic::MOD_DEST_VOLUME, rosic::MOD_DEST_PITCH,
                          rosic::MOD_DEST_RESONANCE, rosic::MOD_DEST_FILTER_FM };
  for(int dest : audible)
  {
    Setup s; s.dest = dest;
    CHECK(maxDiff(base, render(s)) > 1e-3);
  }

  Setup od; od.dest = rosic::MOD_DEST_OVERDRIVE;
  CHECK(maxDiff(base, render(od)) < 1e-12);
}

static void zeroDepthIsTransparent()
{
  Setup off; off.lfoOn = false;
  std::vector<double> base = render(off);
  for(int dest = rosic::MOD_DEST_CUTOFF; dest <= rosic::MOD_DEST_FILTER_FM; dest++)
  {
    Setup s; s.dest = dest; s.depth = 0.0;
    CHECK(maxDiff(base, render(s)) < 1e-12);
  }
}

int main()
{
  everyDestinationChangesTheSound();
  zeroDepthIsTransparent();
  if(failures == 0)
    std::printf("open303_lfo_test: all passed\n");
  return failures == 0 ? 0 : 1;
}
