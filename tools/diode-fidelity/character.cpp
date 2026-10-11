// Diode filter renders for the SPICE character comparisons (spice/character.py), and the Clip identity.
// usage: character <cutoff> <pot> <amp V> <freq> <secs> <bite> <clip> <sharp> <warm> [hifi=1] [q12 q21]
//          sine at the VCF input in the circuit's volts (DSP units = V / 2.6109, as large_signal), output float32;
//          q12 q21: Bite's two parts separately, as fractions of Bite 1, for diagnostics
//        character identity      checks y(x; clip) = s y(x / s; circuit), s = 2^(2 clip - 1), both qualities and
//          tension -1 / 0 / 1; exit 0 if it holds to rounding
#define protected public
#include "dfl_DiodeLadderFilter.h"
#undef protected
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using F = dfl::DiodeLadderFilter;
static const double SR = 4.0 * 44100.0;
static void setup(F& f, double cut, double pot, double bite, double clip, double sharp, double warm, bool hifi, double tension = 0.0)
{
  f.setSampleRate(SR); f.setHiFi(hifi); f.setTension(tension); f.setInputDrive(0.0); f.setSaturationBias(0.0);
  f.setFeedbackHighpass(F::FEEDBACK_DEFAULT); f.setCutoff(cut);
  f.setResonance((1.0 - std::exp(-3.0 * pot)) / (1.0 - std::exp(-3.0)));
  f.setBite(bite); f.setClipThreshold(clip); f.setSharpness(sharp); f.setWarmth(warm); f.reset();
}
int main(int argc, char** argv)
{
  if (argc > 1 && strcmp(argv[1], "identity") == 0)
  {
    double worst = 0.0;
    for (int hifi = 0; hifi < 2; ++hifi)
     for (double ten : { -1.0, 0.0, 1.0 })
      for (double clip : { 0.0, 0.25, 0.75, 1.0 })
        for (double pot : { 0.0, 0.6, 0.95 })
          for (double amp : { 1.0, 3.25, 10.0 })
          {
            const double s = std::pow(2.0, 2.0 * clip - 1.0);
            F a, b; setup(a, 1000, pot, 0, clip, 0.5, 0, hifi, ten); setup(b, 1000, pot, 0, 0.5, 0.5, 0, hifi, ten);
            double ref = 1e-30, err = 0.0;
            for (int n = 0; n < 40000; ++n)
            {
              const double x = amp / 2.6109 * std::sin(2 * M_PI * 220.0 * n / SR);
              const double ya = a.getSample(x), yb = s * b.getSample(x / s);
              ref = std::fmax(ref, std::fabs(yb)); err = std::fmax(err, std::fabs(ya - yb));
            }
            worst = std::fmax(worst, err / ref);
          }
    printf("Clip identity: worst |y(x; clip) - s y(x / s)| / peak = %.3g\n", worst);
    return worst > 1e-9;
  }
  if (argc < 10) { fprintf(stderr, "usage: character <cutoff> <pot> <amp> <freq> <secs> <bite> <clip> <sharp> <warm> [hifi]\n"); return 1; }
  F f; setup(f, atof(argv[1]), atof(argv[2]), atof(argv[6]), atof(argv[7]), atof(argv[8]), atof(argv[9]), argc > 10 ? atoi(argv[10]) : 1);
  if (argc > 12)   // diagnostics: Bite's Q12 / Q21 parts separately (fractions of Bite 1)
  {
    f.o12 = F::BITE_Q12 * atof(argv[11]); f.o21 = F::BITE_Q21 * atof(argv[12]);
    const double b12 = atof(argv[11]), b21 = atof(argv[12]);
    f.setBite(b21);                                     // Q21's part (and its self-centred offset) ...
    f.o12 = F::BITE_Q12 * b12;                          // ... then Q12's
    f.q12Plain = (f.o12 == 0.0 && f.clip_ == 0.5 && f.q12Rho == 0.0); f.reset();
  }
  const double A = atof(argv[3]) / 2.6109, fr = atof(argv[4]);
  const long N = (long)(atof(argv[5]) * SR);
  for (long n = 0; n < N; ++n)
  {
    float y = (float)f.getSample(A * std::sin(2 * M_PI * fr * n / SR));
    fwrite(&y, sizeof(float), 1, stdout);
  }
  return 0;
}
