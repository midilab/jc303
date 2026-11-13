// Diode mode-morph checks (LP -> BP -> HP, dfl_DiodeLadderFilter.h getSample).
//  1. Level: full-synth saw note, RMS over the first 0.25 s, at mode 0 / 0.25 / 0.5 / 0.75 / 1 relative to mode 0 (LP),
//     mean over the typical resonant setting the static output gains target (resonance 50..70, cutoff 800..1500 Hz).
//  2. Shape: small-signal response of the filter alone across the mode sweep (21 steps) at resonance 0 and 0.7. A notch
//     would show as a dip between its neighbours, so every response must stay within NOTCH_DB of the larger of its
//     values one third of an octave either side.
//  3. Slopes: pure HP falls at least HP_SLOPE_DB from its peak at cutoff / 8; pure BP at least BP_SLOPE_DB at cutoff / 8
//     and x 8 (the ladder's lowest pole sits near cutoff / 14, so with no resonance the bandpass is broad).
// usage: mode_level      Exit 0 if the level spread and the shape checks pass.
#include "rosic_Open303.h"
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>
static const double SR = 44100.0, LEVEL_DB = 1.5, NOTCH_DB = 3.0, HP_SLOPE_DB = 30.0, BP_SLOPE_DB = 12.0;
static double rmsDb(double cutoff, double res, double env, double drive, double mode)
{
  rosic::Open303 s; s.setSampleRate(SR); s.setFilterType(dfl::FILTER_DIODE); s.setCutoff(cutoff);
  s.setResonance(res); s.setEnvMod(env); s.setDecay(400.0); s.setAccent(80.0); s.setVolume(-12.0);
  s.setFilterDrive(drive); s.setFilterMode(mode); s.setWaveform(0.0);
  s.noteOn(45, 64, 0.0);
  int n = (int)(0.25 * SR); double acc = 0.0;
  for (int i = 0; i < n; i++) { double y = s.getSample(); acc += y * y; }
  return 10.0 * log10(acc / n + 1e-24);
}
static double respDb(double mode, double cutoff, double res, double f)
{
  const double fs = 4.0 * SR, amp = 1e-3, w = 2.0 * M_PI * f / fs;
  dfl::DiodeLadderFilter d; d.setSampleRate(fs); d.setInputDrive(-12.0); d.setCutoff(cutoff); d.setResonance(res); d.setMode(mode);
  const int settle = (int)(fs * std::max(0.3, 20.0 / f)), N = (int)(fs * std::max(0.1, 10.0 / f));
  std::complex<double> a = 0.0;
  for (int n = 0; n < settle + N; n++) { double y = d.getSample(amp * sin(w * n)); if (n >= settle) a += y * std::polar(1.0, -w * n); }
  return 20.0 * log10(2.0 * std::abs(a) / N / amp + 1e-30);
}
int main()
{
  int bad = 0;
  const double modes[] = { 0.25, 0.5, 0.75, 1.0 };
  printf("level vs LP at resonance 50..70, cutoff 800..1500:");
  for (double m : modes)
  {
    double sum = 0.0; int n = 0;
    for (double cutoff : { 800.0, 1100.0, 1500.0 })
      for (double res : { 50.0, 60.0, 70.0 })
        for (double env : { 0.0, 60.0 })
        { sum += rmsDb(cutoff, res, env, -3.0, m) - rmsDb(cutoff, res, env, -3.0, 0.0); n++; }
    const double d = sum / n; if (fabs(d) > LEVEL_DB) bad++;
    printf("  %.2f: %+.2f dB", m, d);
  }
  printf("  (limit %.1f)\n", LEVEL_DB);

  const double cutoff = 1000.0;
  std::vector<double> fr; for (int i = -15; i <= 15; i++) fr.push_back(cutoff * pow(2.0, i / 3.0));   // 1/32 .. 32 x cutoff
  double worstDip = 0.0, worstAt = 0.0, worstF = 0.0;
  for (double res : { 0.0, 0.7 })
    for (int step = 0; step <= 20; step++)
    {
      const double m = step / 20.0;
      std::vector<double> r; for (double f : fr) r.push_back(f < 20000.0 ? respDb(m, cutoff, res, f) : NAN);
      for (size_t i = 1; i + 1 < r.size(); i++)
        if (std::isfinite(r[i + 1]))
        {
          const double dip = std::min(r[i - 1], r[i + 1]) - r[i];   // below both neighbours
          if (dip > worstDip) { worstDip = dip; worstAt = m; worstF = fr[i]; }
        }
      if (step == 10 || step == 20)
      {
        size_t pk = 0; for (size_t i = 0; i < r.size(); i++) if (std::isfinite(r[i]) && r[i] > r[pk]) pk = i;
        const double lo = r[pk] - r[6], hi = step == 10 ? r[pk] - r[24] : HP_SLOPE_DB;   // cutoff / 8, x 8
        const double lim = step == 10 ? BP_SLOPE_DB : HP_SLOPE_DB;
        if (lo < lim || hi < lim) bad++;
        printf("%s res %.1f: peak at %.0f Hz, %.1f dB down at cutoff/8%s", step == 10 ? "BP" : "HP", res, fr[pk], lo,
               step == 10 ? "" : "\n");
        if (step == 10) printf(", %.1f dB down at cutoff x8\n", r[pk] - r[24]);
      }
    }
  if (worstDip > NOTCH_DB) bad++;
  printf("mode sweep: worst dip below both 1/3-octave neighbours %.2f dB (mode %.2f, %.0f Hz; limit %.1f)\n", worstDip, worstAt, worstF, NOTCH_DB);
  printf("%s\n", bad ? "FAIL" : "ok");
  return bad ? 1 : 0;
}
