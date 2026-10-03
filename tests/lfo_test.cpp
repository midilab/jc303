#include "dfl_LFO.h"

#include <cstdio>

static int failures = 0;

#define CHECK(cond)                                                      \
  do {                                                                   \
    if(!(cond)) {                                                        \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
      failures++;                                                        \
    }                                                                    \
  } while(0)

// 100 Hz at 10 kHz = 100 samples per cycle
static dfl::LFO makeLfo(int waveform)
{
  dfl::LFO lfo;
  lfo.setSampleRate(10000.0);
  lfo.setRate(100.0);
  lfo.setWaveform(waveform);
  lfo.reset();
  return lfo;
}

static double sampleAt(dfl::LFO& lfo, int n)
{
  double v = 0.0;
  for(int i = 0; i <= n; i++)
    v = lfo.getSample();
  return v;
}

static void contourZeroIsLinear()
{
  dfl::LFO lfo = makeLfo(1);  // saw up
  CHECK(lfo.getContour() == 0.0);
  CHECK(std::fabs(sampleAt(lfo, 50) - 0.5) < 1e-9);
}

static void contourClamps()
{
  dfl::LFO lfo = makeLfo(1);
  lfo.setContour(5.0);
  CHECK(lfo.getContour() == 1.0);
  lfo.setContour(-5.0);
  CHECK(lfo.getContour() == -1.0);
}

static void powerCurveSagAndBulge()
{
  dfl::LFO sag = makeLfo(1);
  sag.setContour(1.0);
  CHECK(std::fabs(sampleAt(sag, 50) - std::pow(0.5, 4.0)) < 1e-9);

  dfl::LFO bulge = makeLfo(1);
  bulge.setContour(-1.0);
  CHECK(std::fabs(sampleAt(bulge, 50) - std::pow(0.5, 0.25)) < 1e-9);

  dfl::LFO down = makeLfo(2);  // saw down: 1 -> 0, still endpoint-preserving
  down.setContour(1.0);
  CHECK(std::fabs(down.getSample() - 1.0) < 1e-9);
}

static void squareLagSlewsEdges()
{
  dfl::LFO lfo = makeLfo(3);  // square, high for first 50 samples
  lfo.setContour(-1.0);
  double first = lfo.getSample();
  CHECK(first < 0.6);                    // slewed up from the 0.5 start, not a hard edge
  double last = sampleAt(lfo, 48);
  CHECK(last > first);                   // still rising
  CHECK(last < 1.0);
}

static void squareEdgeEmphasisStaysUnipolar()
{
  dfl::LFO lfo = makeLfo(3);
  lfo.setContour(1.0);
  for(int i = 0; i < 1000; i++)
  {
    double v = lfo.getSample();
    CHECK(v >= 0.0 && v <= 1.0);
  }
}

static void resetClearsFilterState()
{
  dfl::LFO lfo = makeLfo(3);
  lfo.setContour(-1.0);
  sampleAt(lfo, 40);
  lfo.reset();
  CHECK(lfo.getSample() < 0.6);          // same as a fresh LFO
}

int main()
{
  contourZeroIsLinear();
  contourClamps();
  powerCurveSagAndBulge();
  squareLagSlewsEdges();
  squareEdgeEmphasisStaysUnipolar();
  resetClearsFilterState();
  if(failures == 0)
    std::printf("lfo_test: all passed\n");
  return failures == 0 ? 0 : 1;
}
