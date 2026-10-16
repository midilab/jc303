// Stress / stability / CPU test of the MS-20 filter. Exit code 0 when every case passes.
//  * every knob at its extremes (the saturation bias 0 / 1), PEAK 0..1, fast exponential cutoff sweeps 20 Hz..20 kHz, saw input with drive,
//    at 4 x and 1 x 44.1 kHz: no non-finite sample, peak below a bound, and after the input stops at PEAK below the
//    oscillation threshold the output's ring (second difference, 1.6 s after) is below 1e-7
//  * Newton lock-up at the op amp's rails: BP, knob 0.95, drive +16 dB, saturation bias 0.5, full-scale saw, a 3 Hz
//    cutoff sweep and fixed 6 / 10 kHz cutoffs: peak below 4 (normal 1.5..2.2) and under 1 % of samples non-converged
//  * CPU: ns per sample at 4 x 44.1 kHz on a resonant saw
#include "dfl_MS20Filter.h"
#include "dfl_ResonanceMap.h"
#include <cstdio>
#include <cmath>
#include <chrono>
using F = dfl::MS20Filter;
static int fails = 0, cases = 0;
static long gridNonConv = 0, gridSamples = 0;   // Newton solves that did not converge over the knob grid
static double gBias = 0.0;   // saturation bias for the current pass
static void run(double sr, double pot, double mode, double drive, double sharp, double warm,
                double clip, double bite, double fm)
{
  F f; f.setSampleRate(sr); f.setResonance((1.0 - exp(-3.0 * pot)) / 0.950213); f.setMode(mode);
  f.setInputDrive(drive); f.setSharpness(sharp); f.setWarmth(warm); f.setClipThreshold(clip); f.setBite(bite);
  f.setFilterFmDepth(fm); f.setSaturationBias(gBias); f.setCutoff(1000); f.reset();
  double ph = 0.0, peak = 0.0, tail = 0.0, yPrev = 0.0, dPrev = 0.0; const long N = (long)(2.0 * sr), stop = (long)(0.4 * sr);
  for (long n = 0; n < N; ++n)
  {
    double t = n / sr;
    f.setCutoff(20.0 * pow(1000.0, 0.5 + 0.5 * sin(2 * M_PI * 3.0 * t)));   // 20 Hz..20 kHz, 3 Hz sweep
    double x = n < stop ? 2.0 * ph - 1.0 : 0.0; ph += 87.3 / sr; if (ph >= 1.0) ph -= 1.0;
    double y = f.getSample(x);
    if (!std::isfinite(y)) { peak = INFINITY; break; }
    peak = std::max(peak, std::fabs(y));
    if (!f.isConverged()) ++gridNonConv;
    ++gridSamples;
    // ring: second difference (removes the coupling capacitors' slow DC drift, as the circuit has)
    const double d = y - yPrev; if (n > N - (long)(0.05 * sr)) tail = std::max(tail, std::fabs(d - dPrev)); dPrev = d; yPrev = y;
  }
  ++cases;
  // above the threshold the filter rings on (it is an oscillator); require only boundedness there
  const double thr = 0.7;
  // bound: PEAK 1 in BP with Clip / Bite at their extremes self-oscillates near the op amp's rails (~+35 dB re the
  // passband), which is bounded but loud
  bool ok = std::isfinite(peak) && peak < 100.0 && (pot >= thr || tail < 1e-7);
  if (!ok)
  {
    ++fails;
    printf("FAIL sr %.0f pot %.2f mode %.2f drive %.0f sharp %.1f warm %.1f clip %.1f bite %.1f fm %.1f bias %.1f: peak %g tail %g\n",
           sr, pot, mode, drive, sharp, warm, clip, bite, fm, gBias, peak, tail);
  }
}
// sweepHz > 0: 20 Hz..20 kHz exponential sweep at that rate, else a fixed cutoff
static void lockup(double sr, double sweepHz, double cut)
{
  F f; f.setSampleRate(sr); f.setCutoff(cut);
  f.setResonance((1.0 - exp(-3.0 * dfl::resonancePot(dfl::FILTER_MS20, 0.95))) / 0.950213);
  f.setMode(0.5); f.setInputDrive(16.0); f.setSaturationBias(0.5); f.setClipThreshold(0.5); f.reset();
  const long N = (long)(1.0 * sr); double ph = 0.0, peak = 0.0; long nc = 0;
  for (long n = 0; n < N; ++n)
  {
    if (sweepHz > 0.0) f.setCutoff(20.0 * pow(1000.0, 0.5 + 0.5 * sin(2 * M_PI * sweepHz * n / sr)));
    double x = 2.0 * ph - 1.0; ph += 110.0 / sr; if (ph >= 1.0) ph -= 1.0;
    double y = f.getSample(x);
    if (!std::isfinite(y)) { peak = INFINITY; break; }
    peak = std::max(peak, std::fabs(y));
    if (!f.isConverged()) ++nc;
  }
  ++cases;
  const bool ok = std::isfinite(peak) && peak < 4.0 && nc < N / 100;
  if (!ok) ++fails;
  printf("%s lock-up sr %.0f %s %.0f: peak %.2f, non-converged %ld of %ld\n", ok ? "ok  " : "FAIL", sr,
         sweepHz > 0.0 ? "sweep Hz" : "cutoff", sweepHz > 0.0 ? sweepHz : cut, peak, nc, N);
}
int main()
{
  for (double sr : { 4 * 44100.0, 44100.0 })
  {
    lockup(sr, 3.0, 1000.0);
    lockup(sr, 0.0, 6000.0);
    lockup(sr, 0.0, 10000.0);
  }
  for (double bias : { 0.0, 1.0 })
    for (double sr : { 4 * 44100.0, 44100.0 })
      for (double pot : { 0.0, 0.6, 1.0 })
        for (double mode : { 0.0, 0.5, 1.0 })
          for (double drive : { -3.0, 0.0, 9.0, 16.0 })
            for (double sharp : { 0.0, 1.0 })
              for (double warm : { 0.0, 1.0 })
                for (double clip : { 0.0, 1.0 })
                {
                  gBias = bias;
                  run(sr, pot, mode, drive, sharp, warm, clip, clip, drive > 0 ? 1.0 : 0.0);
                }
  printf("%d of %d cases failed; non-converged samples over the grid: %ld of %ld\n", fails, cases, gridNonConv, gridSamples);
  {
    F f; f.setSampleRate(4 * 44100.0); f.setCutoff(800); f.setResonance(0.8); f.reset();
    const long N = 4 * 44100 * 2; double ph = 0.0, acc = 0.0;
    auto t0 = std::chrono::steady_clock::now();
    for (long n = 0; n < N; ++n) { acc += f.getSample(2.0 * ph - 1.0); ph += 110.0 / (4 * 44100.0); if (ph >= 1.0) ph -= 1.0; }
    double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / N;
    printf("%.0f ns / sample at 4x (%.1f%% of one core at 44.1 kHz) [%g]\n",
           ns, ns * 4 * 44100 * 1e-7, acc * 0);
  }
  return fails ? 1 : 0;
}
