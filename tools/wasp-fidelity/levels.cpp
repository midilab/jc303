// Output level (RMS dB re a resonance-0 LP render at 0 dB drive) of the Wasp filter on a band-limited saw, per mode,
// pot and drive: for the BP / HP output gains and the drive make-up.
// usage: levels <edp|a124> [noteHz=110]
#include "dfl_WaspFilter.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
using F = dfl::WaspFilter;
static F::Revision parseRev(const char* s)
{
  return strcmp(s, "a124") == 0 ? F::Revision::A124 : F::Revision::EDP;
}
static const double SR = 4.0 * 44100.0;
static double polyblep(double t, double dt)
{
  if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
  if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
  return 0.0;
}
static double rms(F::Revision rev, double cut, double pot, double mode, double driveDb, double note)
{
  F f(rev); f.setSampleRate(SR); f.setCutoff(cut); f.setResonance((1.0 - exp(-3.0 * pot)) / 0.950213);
  f.setMode(mode); f.setInputDrive(driveDb); f.reset();
  double ph = 0.0, dt = note / SR, acc = 0.0; long n0 = (long)(0.2 * SR), N = (long)(0.3 * SR);
  for (long n = 0; n < n0 + N; ++n)
  {
    double saw = 2.0 * ph - 1.0 - polyblep(ph, dt);
    ph += dt; if (ph >= 1.0) ph -= 1.0;
    double y = f.getSample(saw);
    if (n >= n0) acc += y * y;
  }
  return 10.0 * log10(acc / N + 1e-30);
}
int main(int argc, char** argv)
{
  F::Revision rev = argc > 1 ? parseRev(argv[1]) : F::Revision::EDP;
  double note = argc > 2 ? atof(argv[2]) : 110.0;
  const double cuts[] = { 500, 1000, 2000 };
  const double ref = rms(rev, 1000, 0.0, 0.0, 0.0, note);
  printf("mode levels (dB re LP pot 0 cutoff 1k), drive 0, note %g Hz; cols cutoff 500 / 1k / 2k\n", note);
  for (double pot : { 0.0, 0.5, 0.8, 1.0 })
    for (double mode : { 0.0, 0.25, 0.5, 0.75, 1.0 })
    {
      printf("  pot %.1f mode %.2f:", pot, mode);
      for (double c : cuts) printf(" %6.1f", rms(rev, c, pot, mode, 0.0, note) - ref);
      printf("\n");
    }
  printf("drive levels (LP, dB re drive 0 at the same setting); cols drive 0 / 3 / 6 / 9 / 16 dB\n");
  for (double pot : { 0.0, 0.5, 1.0 })
    for (double c : cuts)
    {
      double r0 = rms(rev, c, pot, 0.0, 0.0, note);
      printf("  pot %.1f cut %5.0f:", pot, c);
      for (double d : { 0.0, 3.0, 6.0, 9.0, 16.0 }) printf(" %6.1f", rms(rev, c, pot, 0.0, d, note) - r0);
      printf("\n");
    }
  return 0;
}
