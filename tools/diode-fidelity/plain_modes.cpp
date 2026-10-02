// Self-oscillation onset of the plain Diode / BP / HP modes: bisection on the resonance knob (%)
// at which an impulse no longer decays. Design: onset at about 87-92% for every cutoff, drive and
// mode (the knob spans 0..0.8 of the old range, so the old 70-74% / 0.8); no oscillation at 80%; sustained oscillation at 100%. Exit 0 = within limits.
#include "dfl_DiodeLadderFilter.h"
#include <cstdio>
#include <cmath>
using D = dfl::DiodeLadderFilter;
static const double SR = 4.0 * 44100.0;
static double skew(double p) { double r = 0.01 * p; return (1.0 - exp(-3.0 * r)) / (1.0 - exp(-3.0)); }
static bool osc(D::ResponseMode mode, double c, double res, double drive)
{
  D d; d.setSampleRate(SR); d.setOctaveMode(false); d.setResponseMode(mode); d.setInputDrive(drive);
  d.setPassbandCompensation(0.0); d.setCutoff(c); d.setResonance(skew(res)); d.reset(); d.getSample(1e-3);
  int N = (int)(SR * 1.5); double pk = 0, tail = 0;
  for (int i = 0; i < N; i++) { double y = fabs(d.getSample(0.0)); if (i < (int)(SR * 0.2)) pk = fmax(pk, y); if (i > N - (int)(SR * 0.2)) tail = fmax(tail, y); }
  return tail > pk * 1e-3;
}
int main()
{
  int bad = 0; double lo_all = 100, hi_all = 0;
  printf("resonance (%%) at which self-oscillation starts\n cutoff |  plain LP   |   BP        |   HP        (drive -6 / +4.5)\n");
  for (double c : { 300.0, 800.0, 3000.0, 8000.0, 15000.0 })
  {
    printf(" %6.0f |", c);
    for (D::ResponseMode m : { D::RESPONSE_LP, D::RESPONSE_BP, D::RESPONSE_HP })
    {
      for (double dr : { -6.0, 4.5 })
      {
        double lo = 40, hi = 100; for (int i = 0; i < 12; i++) { double mid = 0.5 * (lo + hi); if (osc(m, c, mid, dr)) hi = mid; else lo = mid; }
        printf(" %5.1f", hi); lo_all = fmin(lo_all, hi); hi_all = fmax(hi_all, hi);
        if (hi < 83 || hi > 95) bad++;
        if (osc(m, c, 80, dr) || !osc(m, c, 100, dr)) bad++;
      }
      printf(" |");
    }
    printf("\n");
  }
  printf("onset range %.1f - %.1f %%, %d violations of [83, 95] / no oscillation at 80 / oscillation at 100\n", lo_all, hi_all, bad);
  return bad ? 1 : 0;
}
