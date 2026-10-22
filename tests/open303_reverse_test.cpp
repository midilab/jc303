// Reverse gate: the inverse portamento of reversed slides.
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

static constexpr double FS = 44100.0;

static void render(rosic::Open303& s, std::vector<double>& out, int n)
{
  for(int i = 0; i < n; i++)
    out.push_back(s.getSample());
}

// fundamental from upward zero crossings of a lowpassed copy (two poles, so the saw's harmonics
// add no crossings)
static double pitchHz(const std::vector<double>& y, int from, int to)
{
  double lp1 = 0.0, lp = 0.0, prev = 0.0;
  int first = -1, last = -1, count = 0;
  for(int i = from - 2000; i < to; i++)   // settled before the window
  {
    lp1 += 0.01 * (y[i] - lp1);
    lp  += 0.01 * (lp1 - lp);
    if(i < from) { prev = lp; continue; }
    if(prev <= 0.0 && lp > 0.0)
    {
      if(first < 0) first = i; else { last = i; ++count; }
    }
    prev = lp;
  }
  return count > 0 ? count * FS / (last - first) : 0.0;
}

static void setup(rosic::Open303& s)
{
  s.setSampleRate(FS);
  s.setCutoff(2000.0);
  s.setResonance(0.0);
  s.setEnvMod(25.0);
}

// A reversed slide glides late: it holds the old pitch through the quiet head of the slid-into note
// and arrives in its loud end. The sequencer's gate of the slid-into note sets where that end is.
static void reversedSlideGlidesLate(double decay)
{
  rosic::Open303 s;
  setup(s);
  s.setDecay(decay);
  std::vector<double> y;
  const int step = 5512, slidGate = 11025;
  s.noteOn(45, 80, 0.0, false, false, true, step + 2000.0);   // gate: to the slide target, plus the overhang
  render(s, y, step);
  s.noteOn(57, 80, 0.0, false, false, true, slidGate);        // legato
  s.noteOn(45, 0, 0.0);
  render(s, y, slidGate);
  const double early = pitchHz(y, step + 500, step + 3000);
  const double mid   = pitchHz(y, step + slidGate / 2 - 1000, step + slidGate / 2 + 1000);
  const double late  = pitchHz(y, step + slidGate - 1500, step + slidGate);
  std::printf("  reversed slide, decay %.0f ms: early %.1f Hz, mid %.1f Hz, late %.1f Hz\n", decay, early, mid, late);
  CHECK(early < mid && mid < 0.85 * late);   // still gliding at the slid note's middle, at any decay
  CHECK(late > 110.0 * 1.85);
  if(decay <= 200.0)   // a short decay's swell is back-loaded: the glide waits for the end
  {
    CHECK(early < 110.0 * 1.15);
    CHECK(mid < 110.0 * 1.45);
  }
}

// Releasing the newer of two held keys slides back to the older one, in reverse too
static void reversedSlideBack()
{
  rosic::Open303 s;
  setup(s);
  s.setReverseGate(1.0);
  std::vector<double> y;
  s.noteOn(45, 80, 0.0);
  render(s, y, 4410);
  s.noteOn(57, 80, 0.0);
  render(s, y, 4410);
  s.noteOn(57, 0, 0.0);   // 45 still held
  render(s, y, 88200);
  const double end = pitchHz(y, (int) y.size() - 4000, (int) y.size());
  std::printf("  slide back: %.1f Hz\n", end);
  CHECK(std::fabs(end / 110.0 - 1.0) < 0.05);
}

int main()
{
  reversedSlideGlidesLate(200.0);
  reversedSlideGlidesLate(1000.0);
  reversedSlideBack();
  std::printf(failures == 0 ? "all passed\n" : "%d failures\n", failures);
  return failures == 0 ? 0 : 1;
}
