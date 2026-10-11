// Offline solver for the Diode Native feedback law (dfl_DiodeLadderFilter.h, calculateCoefficients, KNT / KNC).
// For one cutoff it finds the self-oscillation threshold K_crit (impulse growth test) at the three Filter Feedback
// nodes (57.5 / 115 / 268 Hz: knob 1.0 in Native / default / knob 0) and, for each pot position at the default,
// the K whose response ratio |H_K| / |H_0| over 0.4..2.4 x cutoff best matches the SPICE model of the real VCF
// (least squares in dB, with the resonant boost weighted 4x; coarse scan, then golden section). Input lines from spice/native_targets.py (gen_native_tables.py runs both):
//   cutoff pot boost peakHz r_0 .. r_24      (r_j at f_j = 0.4 c * 6^(j/24))
// usage: native_solver <cutoff> < curves.txt
#define private public
#define protected public
#include "dfl_DiodeLadderFilter.h"
#undef private
#undef protected
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <complex>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
using D = dfl::DiodeLadderFilter;
static const double SR = 4.0 * 44100.0;
static const int NJ = 25;
static double mag(D& f, double fr, double amp = 1e-4, double settle = 0.25)
{
  f.reset(); int S = (int)(SR * settle), N = (int)(SR * 0.08); std::complex<double> a = 0; double w = 2 * M_PI * fr / SR;
  for (int n = 0; n < S + N; n++) { double y = f.getSample(amp * sin(w * n)); if (n >= S) a += y * std::polar(1.0, -w * n); }
  return 20 * log10(2 * std::abs(a) / N / amp + 1e-30);
}
static void mk(D& d, double c, double K, double fh = 115.0)
{
  d.setSampleRate(SR); d.setResponseMode(D::RESPONSE_LP); d.setInputDrive(0.0);
  d.setFeedbackHighpass(fh); d.setCutoff(c); d.setResonance(0.0);
  d.K = K; d.reset();
}
// Not growing: the ring at 5-6 s is below the one at 2-3 s. The kick is tiny so the loop stays linear (a growing mode
// would otherwise level off in the input tanh). A decay criterion is wrong here: with a low Filter Feedback corner the
// coupling network's ~7 Hz mode stays stable up to a much higher K but rings for seconds near its threshold.
static bool stable(double c, double K, double fh)
{
  D d; mk(d, c, K, fh); d.getSample(1e-9); int N = (int)(SR * 6.0); double early = 0, late = 0;
  for (int i = 0; i < N; i++)
  {
    double y = fabs(d.getSample(0.0));
    if (!std::isfinite(y)) return false;
    if (i > (int)(SR * 2.0) && i < (int)(SR * 3.0)) early = fmax(early, y);
    if (i > (int)(SR * 5.0)) late = fmax(late, y);
  }
  return late < early && late < 1e-7;
}
struct Tgt { double pot, boost, peakHz; double r[NJ]; };
int main(int argc, char** argv)
{
  double c = atof(argv[1]); std::vector<Tgt> T; std::string line;
  while (std::getline(std::cin, line)) { std::istringstream is(line); double cc; Tgt t; is >> cc >> t.pot >> t.boost >> t.peakHz; for (double& x : t.r) is >> x; if (fabs(cc - c) < 0.5) T.push_back(t); }
  double KcF[3]; const double FH[3] = { 57.5, 115.0, 268.0 };
  for (int h = 0; h < 3; h++) { double lo = 10, hi = 60; for (int i = 0; i < 15; i++) { double m = 0.5 * (lo + hi); if (stable(c, m, FH[h])) lo = m; else hi = m; } KcF[h] = lo; }
  const double Kc = KcF[1], Kmax = 0.99 * Kc;
  double fj[NJ]; for (int j = 0; j < NJ; j++) fj[j] = 0.4 * c * pow(6.0, j / double(NJ - 1));
  D d0; mk(d0, c, 0.0); double h0[NJ]; for (int j = 0; j < NJ; j++) h0[j] = mag(d0, fj[j]);
  auto err = [&](const Tgt& t, double K, double* bOut) {
    D d; mk(d, c, K); double e = 0, best = -1e9, bf = 0;
    for (int j = 0; j < NJ; j++) { double h = mag(d, fj[j]); e += pow(h - h0[j] - t.r[j], 2); if (h > best) { best = h; bf = fj[j]; } }
    for (double fr = bf / 1.08; fr <= bf * 1.08; fr *= 1.01) { if (fr < 0.4 * c || fr > 2.4 * c) continue; double h = mag(d, fr); if (h > best) { best = h; bf = fr; } }
    double b = best - mag(d0, bf); if (bOut) *bOut = b;
    return e / NJ + 4.0 * pow(b - t.boost, 2);
  };
  printf("cut %6.0f | K_crit %.3f %.3f %.3f |", c, KcF[0], KcF[1], KcF[2]);
  for (auto& t : T)
  {
    // coarse scan first (the error is not unimodal at low pot positions), then golden-section around the best
    int bi = 0; double be = 1e30;
    for (int i = 0; i <= 24; i++) { double e = err(t, Kmax * i / 24.0, 0); if (e < be) { be = e; bi = i; } }
    double a = Kmax * std::max(bi - 1, 0) / 24.0, b = Kmax * std::min(bi + 1, 24) / 24.0;
    const double g = 0.618034; double x1 = b - g * (b - a), x2 = a + g * (b - a), f1 = err(t, x1, 0), f2 = err(t, x2, 0);
    for (int i = 0; i < 22; i++) { if (f1 < f2) { b = x2; x2 = x1; f2 = f1; x1 = b - g * (b - a); f1 = err(t, x1, 0); } else { a = x1; x1 = x2; f1 = f2; x2 = a + g * (b - a); f2 = err(t, x2, 0); } }
    double K = 0.5 * (a + b), bo; double e = err(t, K, &bo);
    printf("  a%.1f: K %.3f (spice %.1f dsp %.1f dB, rms %.2f)", t.pot, K, t.boost, bo, sqrt(e));
  }
  printf("\n"); return 0;
}
