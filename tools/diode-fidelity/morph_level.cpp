// BP / HP morph level check: at morph 1 the Diode BP and Diode HP types must be the Diode Native lowpass without bass
// comp (same level), so the morph knob does not double as a volume control at its LP end. Full-synth saw note per
// setting (cutoff x resonance x env mod x drive); RMS over the first 0.25 s, Diode BP / HP minus Diode Native (dB).
// Also prints the pure BP / HP (morph 0) level against Native at the typical resonant setting the static output gains
// target (resonance 50..70, cutoff 800..1500 Hz).
// usage: morph_level      Exit 0 if every morph-1 setting is within LIMIT_DB.
#include "rosic_Open303.h"
#include <cmath>
#include <cstdio>
static const double SR = 44100.0, LIMIT_DB = 0.5;
static double rmsDb(rosic::FilterType ft, double cutoff, double res, double env, double drive, double morph)
{
  rosic::Open303 s; s.setSampleRate(SR); s.setFilterType(ft); s.setCutoff(cutoff);
  s.setResonance(res); s.setEnvMod(env); s.setDecay(400.0); s.setAccent(80.0); s.setVolume(-12.0);
  s.setFilterDrive(drive); s.setPassbandCompensation(morph); s.setWaveform(0.0);
  s.noteOn(45, 64, 0.0);
  int n = (int)(0.25 * SR); double acc = 0.0;
  for (int i = 0; i < n; i++) { double y = s.getSample(); acc += y * y; }
  return 10.0 * log10(acc / n + 1e-24);
}
int main()
{
  int bad = 0;
  for (rosic::FilterType ft : { rosic::FILTER_DIODE_BP, rosic::FILTER_DIODE_HP })
  {
    const char* name = ft == rosic::FILTER_DIODE_BP ? "Diode BP" : "Diode HP";
    double worst = 0.0, mean = 0.0; int n = 0;
    for (double cutoff : { 400.0, 800.0, 1500.0, 2394.0 })
      for (double res : { 0.0, 30.0, 70.0, 100.0 })
        for (double env : { 0.0, 60.0 })
          for (double drive : { -3.0, 4.5, 16.0 })
          {
            double d = rmsDb(ft, cutoff, res, env, drive, 1.0) - rmsDb(rosic::FILTER_DIODE_NATIVE, cutoff, res, env, drive, 0.0);
            worst = fmax(worst, fabs(d)); mean += d; n++;
          }
    bool ok = worst <= LIMIT_DB;
    double pure = 0.0; int np = 0;
    for (double cutoff : { 800.0, 1100.0, 1500.0 })
      for (double res : { 50.0, 60.0, 70.0 })
        for (double env : { 0.0, 60.0 })
        { pure += rmsDb(ft, cutoff, res, env, -3.0, 0.0) - rmsDb(rosic::FILTER_DIODE_NATIVE, cutoff, res, env, -3.0, 0.0); np++; }
    printf("%-9s morph 0 vs Diode Native at resonance 50..70, cutoff 800..1500: mean %+.2f dB\n", name, pure / np);
    printf("%-9s morph 1 vs Diode Native: mean %+.2f dB, worst |dev| %.2f dB (limit %.1f)  %s\n", name, mean / n, worst, LIMIT_DB, ok ? "ok" : "FAIL");
    if (!ok) bad++;
  }
  return bad ? 1 : 0;
}
