// Small-signal magnitude (dB) of the ARP 4075 filter by sine probe, for comparison with the SPICE model.
// usage: probe <cutoff Hz> <resonance 0..1 (skewed, as Open303 passes it)> <mode 0..1> <f1> [f2 ...]
//   or:  probe <cutoff> pot:<pot 0..1> <mode> <f1> ...   (pot position directly)
//   ARP_MOD=1 in the environment: the 2k2 mod
#include "dfl_ARP4075Filter.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <complex>
using F = dfl::ARP4075Filter;
static const double SR = 4.0 * 44100.0;
int main(int argc, char** argv)
{
  if (argc < 5) { fprintf(stderr, "usage: probe <cutoff> <res|pot:x> <mode> <f>...\n"); return 1; }
  double cut = atof(argv[1]);
  double res;
  if (strncmp(argv[2], "pot:", 4) == 0) { double p = atof(argv[2] + 4); res = (1.0 - exp(-3.0 * p)) / 0.950213; }
  else res = atof(argv[2]);
  double mode = atof(argv[3]);
  for (int a = 4; a < argc; ++a)
  {
    double fr = atof(argv[a]);
    F f; f.setSampleRate(SR); f.setTailMod(getenv("ARP_MOD") && atoi(getenv("ARP_MOD"))); f.setCutoff(cut);
    f.setResonance(res); f.setMode(mode); f.reset();
    const double amp = 1e-3;
    int S = (int)(SR * std::max(0.3, 40.0 / fr)), N = (int)(SR * std::max(0.1, 20.0 / fr));
    N = (int)(std::floor(N * fr / SR) * SR / fr); if (N < 10) N = (int)(SR / fr);
    std::complex<double> acc = 0; double w = 2 * M_PI * fr / SR;
    for (int n = 0; n < S + N; ++n) { double y = f.getSample(amp * sin(w * n)); if (n >= S) acc += y * std::polar(1.0, -w * n); }
    printf("%g %.4f\n", fr, 20 * log10(2 * std::abs(acc) / N / amp + 1e-30));
  }
  return 0;
}
