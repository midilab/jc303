// BP / HP morph level check: at morph 1 the Diode BP and Diode HP types must be the plain Diode lowpass (same
// level), so the morph knob does not double as a volume control at its LP end. Full-synth saw note per setting
// (cutoff x resonance x env mod x drive); RMS over the first 0.25 s, Diode BP / HP at morph 1 minus Diode LP (dB).
// usage: morph_level      Exit 0 if every setting is within LIMIT_DB.
#include "rosic_Open303.h"
#include <cmath>
#include <cstdio>
static const double SR = 44100.0, LIMIT_DB = 0.5;
static double rmsDb(int ft, double cutoff, double res, double env, double drive, double morph)
{
  rosic::Open303 s; s.setSampleRate(SR); s.setFilterType((rosic::FilterType)ft); s.setCutoff(cutoff);
  s.setResonance(res); s.setEnvMod(env); s.setDecay(400.0); s.setAccent(80.0); s.setVolume(-12.0);
  s.setFilterDrive(drive); s.setPassbandCompensation(morph); s.setWaveform(0.0);
  s.noteOn(45, 64, 0.0);
  int n = (int)(0.25 * SR); double acc = 0.0;
  for (int i = 0; i < n; i++) { double y = s.getSample(); acc += y * y; }
  return 10.0 * log10(acc / n + 1e-24);
}
int main()
{
  const char* names[] = { "", "", "", "Diode BP", "Diode HP" };
  int bad = 0;
  for (int ft = 3; ft <= 4; ft++)
  {
    double worst = 0.0, mean = 0.0; int n = 0;
    for (double cutoff : { 400.0, 800.0, 1500.0, 2394.0 })
      for (double res : { 0.0, 30.0, 70.0, 100.0 })
        for (double env : { 0.0, 60.0 })
          for (double drive : { -3.0, 4.5, 16.0 })
          {
            double d = rmsDb(ft, cutoff, res, env, drive, 1.0) - rmsDb(2, cutoff, res, env, drive, 0.0);
            worst = fmax(worst, fabs(d)); mean += d; n++;
          }
    bool ok = worst <= LIMIT_DB;
    printf("%-9s morph 1 vs Diode LP: mean %+.2f dB, worst |dev| %.2f dB (limit %.1f)  %s\n", names[ft], mean / n, worst, LIMIT_DB, ok ? "ok" : "FAIL");
    if (!ok) bad++;
  }
  return bad ? 1 : 0;
}
