// Switches the filter model mid-note and reports the largest sample-to-sample step around the switch, relative to the
// largest step of the steady signal before it. A ratio near 1 means the switch is click-free.
// usage: switch_click [cutoffHz] [res0to100]
#include "rosic_Open303.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
int main(int argc, char** argv)
{
  const double sr = 44100.0, cutoff = argc > 1 ? atof(argv[1]) : 800.0, res = argc > 2 ? atof(argv[2]) : 60.0;
  const int switchAt = (int)(0.3 * sr), total = (int)(0.6 * sr), back = (int)(0.45 * sr);
  double worst = 0.0;
  for (int from = 0; from < dfl::NUM_FILTER_TYPES; from++)
  {
    rosic::Open303 s; s.setSampleRate(sr); s.setFilterType((dfl::FilterType)from); s.setCutoff(cutoff);
    s.setResonance(res); s.setEnvMod(0.0); s.setDecay(2000.0); s.setVolume(-12.0); s.setWaveform(0.0);
    s.noteOn(36, 64, 0.0);
    std::vector<double> y(total);
    for (int i = 0; i < total; i++)
    {
      if (i == switchAt) s.setFilterType((dfl::FilterType)(from == 0 ? 1 : 0));
      if (i == back)     s.setFilterType((dfl::FilterType)from);
      y[i] = s.getSample();
    }
    // steady reference: the step size just before the switch; the switches: 20 ms after each
    auto maxStep = [&](int a, int b) { double m = 0.0; for (int i = a; i < b; i++) m = fmax(m, fabs(y[i] - y[i - 1])); return m; };
    double ref = maxStep(switchAt - (int)(0.1 * sr), switchAt);
    double refTo = maxStep(back - (int)(0.1 * sr), back);
    double r1 = maxStep(switchAt, switchAt + (int)(0.02 * sr)) / ref;
    double r2 = maxStep(back, back + (int)(0.02 * sr)) / refTo;
    printf("%d -> %d: step ratio %.2f   %d -> %d: step ratio %.2f\n", from, from == 0 ? 1 : 0, r1, from == 0 ? 1 : 0, from, r2);
    worst = fmax(worst, fmax(r1, r2));
  }
  printf("worst %.2f\n", worst);
  return worst > 1.5 ? 1 : 0;
}
