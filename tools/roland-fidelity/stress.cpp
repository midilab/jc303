// Stress / stability / CPU test of the IR3109 filter. Exit code 0 when every case passes.
//  * RES 0..1, every mode, drive -3..+24 dB, filter FM, the character controls at their extremes, saturation bias 0 / 1, fast exponential cutoff sweeps 20 Hz..20 kHz, saw input, at
//    4 x and 1 x 44.1 kHz: no non-finite sample, peak below a bound, and after the input stops at RES below the
//    oscillation threshold the output's ring (second difference, 1.6 s after) is below 1e-7
//  * CPU: ns per sample at 4 x 44.1 kHz on a resonant saw
#include "dfl_RolandFilter.h"
#include <cstdio>
#include <cmath>
#include <chrono>
using F = dfl::RolandFilter;
static int fails = 0, cases = 0;
static double gBias = 0.0;   // saturation bias for the current pass
static void run(double sr, double pot, double mode, double drive, double fm, double bite, double clip,
                double warm, double sharp)
{
  F f; f.setSampleRate(sr); f.setResonance((1.0 - exp(-3.0 * pot)) / 0.950213); f.setMode(mode);
  f.setInputDrive(drive); f.setFilterFmDepth(fm); f.setBite(bite); f.setClipThreshold(clip); f.setWarmth(warm);
  f.setSharpness(sharp); f.setSaturationBias(gBias); f.setCutoff(1000); f.reset();
  double ph = 0.0, peak = 0.0, tail = 0.0, yPrev = 0.0, dPrev = 0.0; const long N = (long)(2.0 * sr), stop = (long)(0.4 * sr);
  for (long n = 0; n < N; ++n)
  {
    double t = n / sr;
    f.setCutoff(20.0 * pow(1000.0, 0.5 + 0.5 * sin(2 * M_PI * 3.0 * t)));   // 20 Hz..20 kHz, 3 Hz sweep
    double x = n < stop ? 2.0 * ph - 1.0 : 0.0; ph += 87.3 / sr; if (ph >= 1.0) ph -= 1.0;
    double y = f.getSample(x);
    if (!std::isfinite(y)) { peak = INFINITY; break; }
    peak = std::max(peak, std::fabs(y));
    const double d = y - yPrev; if (n > N - (long)(0.05 * sr)) tail = std::max(tail, std::fabs(d - dPrev)); dPrev = d; yPrev = y;
  }
  ++cases;
  // above the threshold (RES ~0.89) the filter rings on (it is an oscillator); require only boundedness there
  const double thr = 0.88;
  bool ok = std::isfinite(peak) && peak < 100.0 && (pot >= thr || tail < 1e-7);
  if (!ok)
  {
    ++fails;
    printf("FAIL sr %.0f pot %.2f mode %.2f drive %.0f fm %.1f bite %.1f clip %.1f warm %.1f sharp %.1f bias %.1f: peak %g tail %g\n",
           sr, pot, mode, drive, fm, bite, clip, warm, sharp, gBias, peak, tail);
  }
}
int main()
{
  for (double bias : { 0.0, 1.0 })
    for (double sr : { 4 * 44100.0, 44100.0 })
      for (double pot : { 0.0, 0.5, 0.7, 0.85, 0.95, 1.0 })
        for (double mode : { 0.0, 0.25, 0.5, 0.75, 1.0 })
          for (double drive : { -3.0, 0.0, 16.0, 24.0 })
            for (int c = 0; c < 4; ++c)   // character: the circuit, each extreme pair, all at once
            {
              const double ch[4][4] = { { 0, 0.5, 0, 0.5 }, { 1, 0, 1, 0 }, { 0, 1, 0, 1 }, { 1, 1, 1, 1 } };
              gBias = bias;
              run(sr, pot, mode, drive, c == 1 ? 1.0 : 0.0, ch[c][0], ch[c][1], ch[c][2], ch[c][3]);
            }
  printf("%d of %d cases failed\n", fails, cases);
  {
    F f; f.setSampleRate(4 * 44100.0); f.setCutoff(800); f.setResonance(0.8); f.reset();
    const long N = 4 * 44100 * 2; double ph = 0.0, acc = 0.0;
    auto t0 = std::chrono::steady_clock::now();
    for (long n = 0; n < N; ++n) { acc += f.getSample(2.0 * ph - 1.0); ph += 110.0 / (4 * 44100.0); if (ph >= 1.0) ph -= 1.0; }
    double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / N;
    printf("%.0f ns / sample at 4x (%.1f%% of one core at 44.1 kHz) [%g]\n", ns, ns * 4 * 44100 * 1e-7, acc * 0);
  }
  return fails ? 1 : 0;
}
