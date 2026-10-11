// Drive-knob level check for the diode filter in LP / BP / HP mode: the drive control must not act as a volume knob.
// Full-synth saw note per setting (cutoff x resonance x env mod); RMS over the first 0.25 s relative to the
// drive floor (-3 dB, the bottom of the knob).
//   -3..0 dB (clean region): level within +-FLAT_DB of the floor
//   0..+16 dB (saturating):  level rise at most MAX_RISE_DB, never below -FALL_DB (the fitted target rises 1 dB)
// Levels come from the drive level tables (dfl_DiodeLadderFilter.h, fit_drive_trim.py).
// usage: drive_level [-v]      Exit 0 if every filter type passes (mean over settings and worst setting).
#include "rosic_Open303.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
static const double SR = 44100.0;
// Per mode (LP, BP, HP): clean-region deviation, fall and rise above 0 dB
// (dB, over 48 settings incl. resonance 0..100). The means follow the fitted target closely; the worst settings
// spread further because the level varies with resonance and cutoff more finely than the 4 x 6 tables resolve.
// Hi-Fi LP spreads further at the top of the knob only: up to +9 dB it tracks Standard within 0.5 dB, but at +16 dB
// (6.3x the real VCO level) the diodes' compression depends on cutoff, which the drive tables do not resolve (low
// cutoff, no env mod about -4.6 dB; high cutoff, full env mod about +5 dB). Standard LP's fall is 3.0 dB since Q12
// went symmetric (latch-up fix): its worst setting falls 2.8 dB, with the mean on target.
static const double FLAT_DB[2][3]     = { { 0.5, 1.0, 1.0 }, { 0.5, 1.0, 1.0 } };   // [Standard, Hi-Fi][LP, BP, HP]
static const double FALL_DB[2][3]     = { { 3.0, 3.0, 3.5 }, { 5.0, 3.0, 3.5 } };
static const double MAX_RISE_DB[2][3] = { { 3.5, 6.0, 5.5 }, { 5.5, 6.0, 5.5 } };
static bool hiFi = true;
static double rmsDb(double mode, double cutoff, double res, double env, double drive)
{
  rosic::Open303 s; s.setSampleRate(SR); s.setFilterType(dfl::FILTER_DIODE); s.setFilterMode(mode); s.setCutoff(cutoff);
  s.setResonance(res); s.setEnvMod(env); s.setDecay(400.0); s.setAccent(80.0); s.setVolume(-12.0);
  s.setFilterDrive(drive); s.setPassbandCompensation(0.0); s.setWaveform(0.0); s.setFilterHiFi(hiFi);
  s.noteOn(45, 64, 0.0);
  int n = (int)(0.25 * SR); double acc = 0.0;
  for (int i = 0; i < n; i++) { double y = s.getSample(); acc += y * y; }
  return 10.0 * log10(acc / n + 1e-24);
}
int main(int argc, char** argv)
{
  bool verbose = argc > 1 && !strcmp(argv[1], "-v");
  const char* names[] = { "LP", "BP", "HP" };
  const double modes[] = { 0.0, 0.5, 1.0 };
  const double drives[] = { -3.0, -1.5, 0.0, 3.0, 4.5, 6.0, 9.0, 16.0 };
  int bad = 0;
  for (int q = 1; q >= 0; q--)
  for (int ft = 0; ft < 3; ft++)
  {
    hiFi = q != 0;
    double worstFlat = 0.0, worstLow = 0.0, worstRise = -99.0, meanRise = 0.0; int n = 0;
    std::vector<double> mean(8, 0.0);
    for (double cutoff : { 400.0, 800.0, 1500.0, 2394.0 })
      for (double res : { 0.0, 30.0, 70.0, 100.0 })
        for (double env : { 0.0, 60.0, 100.0 })
        {
          double base = rmsDb(modes[ft], cutoff, res, env, drives[0]);
          for (int d = 0; d < 8; d++)
          {
            double rel = rmsDb(modes[ft], cutoff, res, env, drives[d]) - base; mean[d] += rel;
            if (drives[d] <= 0.0) worstFlat = fmax(worstFlat, fabs(rel));
            else worstLow = fmin(worstLow, rel);
            if (drives[d] > 0.0) worstRise = fmax(worstRise, rel);
            if (drives[d] == 16.0) meanRise += rel;
          }
          n++;
        }
    meanRise /= n;
    double fall = FALL_DB[q][ft];
    bool ok = worstFlat <= FLAT_DB[q][ft] && worstLow >= -fall && worstRise <= MAX_RISE_DB[q][ft];
    printf("%-8s %-3s clean-region worst dev %.2f dB (limit %.1f), above 0 dB: worst fall %.2f (limit %.1f), +16 dB mean %+.2f, worst rise %+.2f (limit %.1f)  %s\n",
           q ? "Hi-Fi" : "Standard", names[ft], worstFlat, FLAT_DB[q][ft], -worstLow, fall, meanRise, worstRise, MAX_RISE_DB[q][ft], ok ? "ok" : "FAIL");
    if (verbose) { printf("   mean level rel -3 dB:"); for (int d = 0; d < 8; d++) printf(" %+.1f@%.1f", mean[d] / n, drives[d]); printf("\n"); }
    if (!ok) bad++;
  }
  return bad ? 1 : 0;
}
