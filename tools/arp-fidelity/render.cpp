// Renders the ARP 4075 filter in circuit volts, for comparison with SPICE transients: float32 samples on stdout.
// usage: render <cutoff Hz> <pot 0..1> <mode 0..1> <amp V> <freq Hz> <seconds> [kick V] [node] [bias rail]
//   amp / freq: sine at the mixer pot (V, into R123 1M); kick: a 100 us pulse at 1 ms (V)
//   node: out (default: the mode's output in circuit volts re the DC point), o1..o4 (stage outputs re the reference).
//   Sample rate 4 x 44.1 kHz. ARP_MOD=1 in the environment: the 2k2 mod.
#define protected public
#include "dfl_ARP4075Filter.h"
#undef protected
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
using F = dfl::ARP4075Filter;
int main(int argc, char** argv)
{
  if (argc < 7) { fprintf(stderr, "usage: render <cutoff> <pot> <mode> <amp> <freq> <seconds> [kick] [node] [bias rail]\n"); return 1; }
  const double SR = 4.0 * 44100.0, pot = atof(argv[2]);
  F f; f.setSampleRate(SR); f.setTailMod(getenv("ARP_MOD") && atoi(getenv("ARP_MOD"))); f.setCutoff(atof(argv[1]));
  f.setResonance((1.0 - exp(-3.0 * pot)) / 0.950213); f.setMode(atof(argv[3]));
  if (argc > 10) { f.setSaturationBias(atof(argv[9])); f.setRail(atof(argv[10])); }
  f.reset();
  const double amp = atof(argv[4]), fr = atof(argv[5]), kick = argc > 7 ? atof(argv[7]) : 0.0;
  const char* node = argc > 8 ? argv[8] : "out";
  const double mode = f.getMode();
  const double wl = std::max(0.0, 1.0 - 2.0 * mode), wh = std::max(0.0, 2.0 * mode - 1.0), wb = 1.0 - wl - wh;
  const double outScale = wl * f.gainLp + wb * f.gainBp + wh * f.gainHp;
  const long N = (long)(atof(argv[6]) * SR);
  for (long n = 0; n < N; ++n)
  {
    double t = n / SR;
    double v = amp * sin(2 * M_PI * fr * t) + ((t >= 1e-3 && t < 1.1e-3) ? kick : 0.0);
    double o = f.getSample(v / F::INPUT_VOLTS) * outScale;
    if (node[0] == 'o' && node[1] >= '1' && node[1] <= '4') o = f.getStageOutput(node[1] - '1');
    float yv = (float)o;
    fwrite(&yv, sizeof(float), 1, stdout);
  }
  return 0;
}
