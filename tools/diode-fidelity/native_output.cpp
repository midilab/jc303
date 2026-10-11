// Checks dfl::TB303OutputNetwork (Diode Native's output path: Q21 coupling, C14, the VCA's two-tap input with the
// resonance-pot wiper, and the VCA/mixer/line-out coupling) against SPICE: tb303_vca.py's chained netlist, ladder top
// (L0 - R0) -> output jack, Q21 BF = 200 (Stinchcombe's network, coupling pole 578.1 rad/s). Levels are relative to
// pot 0 at 1 kHz, so the table checks the wiper's level makeup as well as the bass corner.
// usage: native_output [-v]      exit 0 when every point is within tolerance
#include "dfl_TB303OutputNetwork.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static double levelDb(dfl::TB303OutputNetwork& net, double f, double fs)
{
  net.reset();
  const int settle = (int)(fs * 3.0), n = (int)(fs * 2.0);
  double re = 0.0, im = 0.0;
  for (int i = 0; i < settle + n; ++i)
  {
    const double ph = 2.0 * M_PI * f * i / fs;
    const double y = net.getSample(std::sin(ph));
    if (i >= settle) { re += y * std::cos(ph); im += y * std::sin(ph); }
  }
  return 20.0 * std::log10(2.0 * std::sqrt(re * re + im * im) / n);
}

int main(int argc, char** argv)
{
  const bool verbose = argc > 1 && !std::strcmp(argv[1], "-v");
  const double fs = 44100.0;
  const double freqs[7] = { 10, 20, 30, 50, 100, 200, 1000 };
  const double pots[5] = { 0.0, 0.25, 0.5, 0.75, 1.0 };
  const double spice[5][7] = {
    { -39.9, -25.7, -18.7, -11.2, -4.3, -1.2,  0.0 },
    { -38.2, -23.9, -16.8,  -9.2, -2.0,  1.6,  3.3 },
    { -36.9, -22.5, -15.4,  -7.7, -0.2,  3.6,  5.4 },
    { -35.8, -21.3, -14.1,  -6.3,  1.4,  5.5,  7.5 },
    { -34.8, -20.0, -12.7,  -4.8,  3.2,  7.6, 10.0 } };
  dfl::TB303OutputNetwork net;
  net.setSampleRate(fs);
  net.setCouplingPole(578.1 / (2.0 * M_PI));
  net.setPot(0.0);
  const double ref = levelDb(net, 1000.0, fs);
  int fails = 0; double worst = 0.0;
  for (int p = 0; p < 5; ++p)
  {
    net.setPot(pots[p]);
    if (verbose) printf("pot %.2f:", pots[p]);
    for (int k = 0; k < 7; ++k)
    {
      const double d = levelDb(net, freqs[k], fs) - ref - spice[p][k];
      const double tol = freqs[k] < 20.0 ? 1.0 : 0.5;
      if (std::fabs(d) > tol) ++fails;
      worst = std::fmax(worst, std::fabs(d));
      if (verbose) printf(" %g Hz %+.2f", freqs[k], d);
    }
    if (verbose) printf("\n");
  }
  printf("native_output: %d of 35 points out of tolerance, worst %.2f dB\n", fails, worst);
  return fails ? 1 : 0;
}
