// Renders the MS-20 filter in circuit volts, for comparison with SPICE transients: float32 samples on stdout.
// usage: render <cutoff Hz> <pot 0..1> <mode 0..1> <amp V> <freq Hz> <seconds> [kick V] [bias] [clip]
//   bias / clip: setSaturationBias (default 0) / setClipThreshold (default 0.5 = the circuit)
//   amp / freq: sine at the filter input (V, at the input pad); kick: a 50 us pulse at 1 ms (V)
// Output: the circuit's output node (after C32), in volts. Sample rate 4 x 44.1 kHz.
#define protected public
#include "dfl_MS20Filter.h"
#undef protected
#include <cstdio>
#include <cstdlib>
#include <cmath>
using F = dfl::MS20Filter;
int main(int argc, char** argv)
{
  if (argc < 7) { fprintf(stderr, "usage: render <cutoff> <pot> <mode> <amp> <freq> <seconds> [kick] [bias] [clip]\n"); return 1; }
  const double SR = 4.0 * 44100.0;
  const double pot = atof(argv[2]);
  F f; f.setSampleRate(SR); f.setCutoff(atof(argv[1]));
  f.setResonance((1.0 - exp(-3.0 * pot)) / 0.950213); f.setMode(atof(argv[3]));
  if (argc > 8) f.setSaturationBias(atof(argv[8]));
  if (argc > 9) f.setClipThreshold(atof(argv[9]));
  f.reset();
  const double amp = atof(argv[4]), fr = atof(argv[5]), kick = argc > 7 ? atof(argv[7]) : 0.0;
  const double volts = F::K35_INPUT_VOLTS;
  const double mode = f.mode;
  const double wl = std::max(0.0, 1.0 - 2.0 * mode), wh = std::max(0.0, 2.0 * mode - 1.0), wb = 1.0 - wl - wh;
  const double outScale = (wl * f.gainLp + wb * f.gainBp + wh * f.gainHp) * volts;
  const long N = (long)(atof(argv[6]) * SR);
  for (long n = 0; n < N; ++n)
  {
    double t = n / SR;
    double v = amp * sin(2 * M_PI * fr * t) + ((t >= 1e-3 && t < 1.05e-3) ? kick : 0.0);
    float y = (float)(f.getSample(v / volts) * outScale);
    fwrite(&y, sizeof(float), 1, stdout);
  }
  return 0;
}
