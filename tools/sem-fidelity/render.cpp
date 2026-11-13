// Renders the SEM filter in circuit volts, for comparison with SPICE transients: float32 samples on stdout.
// usage: render <cutoff Hz> <pot 0..1> <mode 0..1> <amp V> <freq Hz> <seconds> [kick V] [node] [bias rail]
//   amp / freq: sine at R155 (V); kick: a 100 us pulse at 1 ms (V)
//   node: out (default: the mode's output re its rest voltage, inverted back, in circuit volts), or n0..n8 (the
//   circuit nodes re ground: 0 A12 in, 1 C20, 2 BP, 3 C21, 4 LP, 5 R159 / R158, 6 D11 / D12 - R156, 7 C22, 8 HP).
//   Sample rate 4 x 44.1 kHz. Also: render dc  prints the DC operating point.
#define protected public
#include "dfl_SEMFilter.h"
#undef protected
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
using F = dfl::SEMFilter;
int main(int argc, char** argv)
{
  const double SR = 4.0 * 44100.0;
  if (argc >= 2 && strcmp(argv[1], "dc") == 0)
  {
    F f; f.setSampleRate(SR); f.reset();
    printf("vdd %.3f:", f.getSupply());
    for (int k = 0; k < F::NN; ++k) printf(" n%d %.4f", k, f.getNode(k));
    printf("\n");
    return 0;
  }
  if (argc < 7) { fprintf(stderr, "usage: render <cutoff> <pot> <mode> <amp> <freq> <seconds> [kick] [node] [bias rail]\n"); return 1; }
  const double pot = atof(argv[2]);
  F f; f.setSampleRate(SR); f.setCutoff(atof(argv[1]));
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
    if (node[0] == 'n' && node[1] >= '0' && node[1] <= '8') o = f.getNode(node[1] - '0');
    float y = (float)o;
    fwrite(&y, sizeof(float), 1, stdout);
  }
  return 0;
}
