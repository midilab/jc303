// Backbone curves: the resonance's pitch versus its amplitude (the Duffing measurement). The loop gain is set to r
// times the oscillation threshold (past the knob's cap), so the filter self-oscillates at a level the nonlinearities
// set; each r gives one point (level, pitch). A burst at the cutoff starts the audio-band mode (just above threshold
// the coupling network's ~7 Hz mode would win). Pitch is in cents re linear rungs at the same r (Hi-Fi at
// Probe::LINEAR), so Q12's small share drops out; the rung swing is the largest |tanh argument| across a rung.
// usage: backbone          table per quality / tension / cutoff
//        backbone --csv    quality,tension,cutoff,r,level_db,swing,cents
#include "dfl_DiodeLadderFilter.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
static const double SR = 4.0 * 44100.0;
struct Probe : dfl::DiodeLadderFilter
{
  static constexpr double LINEAR = 1.0 / (1.0 + TENSION_STIFF_MAX);   // the tension where the rungs are linear
  void setLoopGain(double r) { K *= r / K_MARGIN; }   // at pot 1, nominal trim, K is K_MARGIN x the threshold
  double swing() const { const double* y = yPrev; return std::max({ std::fabs(y[0] - y[1]), std::fabs(y[1] - y[2]), std::fabs(y[2] - y[3]), std::fabs(y[3]) }) * rungScale; }
};
struct Osc { double level, f, swing; };

static Osc oscillate(bool hiFi, double tension, double cutoff, double r)
{
  Probe d; d.setSampleRate(SR); d.setHiFi(hiFi); d.setTension(tension); d.setInputDrive(0.0); d.setSaturationBias(0.0);
  d.setFeedbackHighpass(Probe::FEEDBACK_DEFAULT); d.setCutoff(cutoff); d.setResonance(1.0); d.reset(); d.setLoopGain(r);
  const int N = (int)(3.0 * SR), M = (int)(0.5 * SR);   // measure the last M samples
  double prev = 0, first = -1, last = -1, pk = 0, sw = 0; int n0 = 0;
  for (int n = 0; n < N; n++)
  {
    const double y = d.getSample(n < 0.02 * SR ? 0.3 * std::sin(2 * M_PI * cutoff * n / SR) : 0.0);
    if (n >= N - M)
    {
      pk = std::max(pk, std::fabs(y)); sw = std::max(sw, d.swing());
      if (prev < 0.0 && y >= 0.0) { const double t = n - 1 + prev / (prev - y); if (first < 0) first = t; else n0++; last = t; }
    }
    prev = y;
  }
  return { 20 * std::log10(pk), n0 > 0 ? SR * n0 / (last - first) : 0.0, sw };
}

int main(int argc, char** argv)
{
  const bool csv = argc > 1 && !strcmp(argv[1], "--csv");
  const double cutoffs[] = { 300, 1000, 3000 };
  const double tensions[] = { -1.0, -0.5, 0.0, Probe::LINEAR, 0.5, 1.0 };
  const double rs[] = { 1.1, 1.2, 1.5, 2.0, 3.0, 5.0 };
  if (csv) printf("quality,tension,cutoff,r,level_db,swing,cents\n");
  for (double c : cutoffs)
  {
    std::vector<double> fLin;
    for (double r : rs) fLin.push_back(oscillate(true, Probe::LINEAR, c, r).f);
    if (!csv)
    {
      printf("\ncutoff %.0f Hz, linear-rung oscillation", c); for (double f : fLin) printf(" %.0f", f);
      printf(" Hz\ncents / rung swing at r =\n%-9s %7s", "quality", "tension");
      for (double r : rs) printf(" %11.2f", r); printf("\n");
    }
    for (int q = 0; q < 2; q++)
      for (double t : tensions)
      {
        if (q == 0 && t != 0.0) continue;   // tension is Hi-Fi only
        if (!csv) printf("%-9s %7.2f", q ? "Hi-Fi" : "Standard", t);
        for (size_t i = 0; i < std::size(rs); i++)
        {
          const double r = rs[i];
          const Osc o = oscillate(q, t, c, r);
          const double cents = o.f > 0 ? 1200 * std::log2(o.f / fLin[i]) : NAN;
          if (csv) printf("%s,%g,%g,%g,%.3f,%.3f,%.2f\n", q ? "HiFi" : "Standard", t, c, r, o.level, o.swing, cents);
          else printf(" %+5.0f/%5.2f", cents, o.swing);
        }
        if (!csv) printf("\n");
      }
  }
  return 0;
}
