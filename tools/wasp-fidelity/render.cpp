// Renders the Wasp filter in circuit volts, for comparison with SPICE transients: float32 samples on stdout.
// usage: render <edp|a124> <cutoff Hz> <pot 0..1> <mode 0..1> <amp V> <freq Hz> <seconds> [kick V] [node] [bias rail]
//   amp / freq: sine at R1 (V); kick: a 100 us pulse at 1 ms (V)
//   node: out (default: the mode's output re its rest voltage, inverted back, in circuit volts), or n0..n7 (the
//   circuit nodes re ground: 0 IC1 in, 1 HP, 2 IC3 in, 3 BP, 4 IC5 in, 5 LP, 6 D1 / D2, 7 C7). Sample rate 4 x 44.1 kHz.
//   Also: render dc <edp|a124>  prints the DC operating point.
#define protected public
#include "dfl_WaspFilter.h"
#undef protected
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
using F = dfl::WaspFilter;
static F::Revision parseRev(const char* s)
{
  return strcmp(s, "a124") == 0 ? F::Revision::A124 : F::Revision::EDP;
}
int main(int argc, char** argv)
{
  const double SR = 4.0 * 44100.0;
  if (argc >= 3 && strcmp(argv[1], "dc") == 0)
  {
    F f(parseRev(argv[2])); f.setSampleRate(SR); f.reset();
    printf("vdd %.3f:", f.getSupply());
    for (int k = 0; k < F::NN; ++k) printf(" n%d %.4f", k, f.getNode(k));
    printf("\n");
    return 0;
  }
  if (argc < 8) { fprintf(stderr, "usage: render <edp|a124> <cutoff> <pot> <mode> <amp> <freq> <seconds> [kick] [node] [bias rail]\n"); return 1; }
  const double pot = atof(argv[3]);
  F f(parseRev(argv[1])); f.setSampleRate(SR); f.setCutoff(atof(argv[2]));
  f.setResonance((1.0 - exp(-3.0 * pot)) / 0.950213); f.setMode(atof(argv[4]));
  if (argc > 11) { f.setSaturationBias(atof(argv[10])); f.setRail(atof(argv[11])); }
  f.reset();
  const double amp = atof(argv[5]), fr = atof(argv[6]), kick = argc > 8 ? atof(argv[8]) : 0.0;
  const char* node = argc > 9 ? argv[9] : "out";
  const double mode = f.getMode();
  const double wl = std::max(0.0, 1.0 - 2.0 * mode), wh = std::max(0.0, 2.0 * mode - 1.0), wb = 1.0 - wl - wh;
  const double outScale = wl * f.gainLp + wb * f.gainBp + wh * f.gainHp;
  const long N = (long)(atof(argv[7]) * SR);
  for (long n = 0; n < N; ++n)
  {
    double t = n / SR;
    double v = amp * sin(2 * M_PI * fr * t) + ((t >= 1e-3 && t < 1.1e-3) ? kick : 0.0);
    double o = f.getSample(v / f.getInputVolts()) * outScale;
    if (node[0] == 'n' && node[1] >= '0' && node[1] <= '7') o = f.getNode(node[1] - '0');
    float y = (float)o;
    fwrite(&y, sizeof(float), 1, stdout);
  }
  return 0;
}
