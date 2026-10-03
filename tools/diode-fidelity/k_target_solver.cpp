// Offline solver for the Diode Octave feedback law (dfl_DiodeLadderFilter.h, calculateCoefficients).
// For one cutoff and feedback-HP setting it finds, by bisection:
//   * K_crit: the diode's own self-oscillation threshold (impulse growth test), and
//   * for TeeBee resonance 20/40/60/80/100: the feedback K that makes the diode's resonant boost
//     equal the TeeBee's boost at the same cutoff parameter.
// K and Kcomp are overridden directly after setup (hence the access hack), at drive 0 dB where
// K == the drive-independent effective feedback. Run tools via gen_k_tables.py, which sweeps the
// cutoff / HP grid and prints the table arrays to paste into the header.
// usage: k_target_solver <cutoffParam> <teebeeFbHp> <diodeFbHp>
#define private public
#define protected public
#include "rosic_TeeBeeFilter.h"
#include "dfl_DiodeLadderFilter.h"
#undef private
#undef protected
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <complex>
using D = dfl::DiodeLadderFilter;
static const double SR = 4.0 * 44100.0;
template <class F> static double mag(F& f, double fr, double amp = 1e-4, double settle = 0.25)
{
  f.reset(); int S = (int)(SR * settle), N = (int)(SR * 0.08); std::complex<double> a = 0; double w = 2 * M_PI * fr / SR;
  for (int n = 0; n < S + N; n++) { double y = f.getSample(amp * sin(w * n)); if (n >= S) a += y * std::polar(1.0, -w * n); }
  return 20 * log10(2 * std::abs(a) / N / amp + 1e-30);
}
static void mkD(D& d, double c, double K, double fh)
{
  d.setSampleRate(SR); d.setOctaveMode(true); d.setResponseMode(D::RESPONSE_LP); d.setInputDrive(0.0);
  d.setPassbandCompensation(0.0); d.setFeedbackHighpass(fh); d.setCutoff(c); d.setResonance(0.5);
  d.K = K; d.Kcomp = K; d.reset();   // override: the real code derives these from the resonance
}
static double boostDO(double c, double K, double fh)
{
  D d, d0; mkD(d, c, K, fh); mkD(d0, c, 0.001, fh);
  double best = -1e9, bf = 0;
  for (double fr = 0.6 * c; fr <= 1.6 * c; fr *= 1.05) { double m = mag(d, fr); if (m > best) { best = m; bf = fr; } }
  for (double fr = bf / 1.05; fr <= bf * 1.05; fr *= 1.012) { double m = mag(d, fr); if (m > best) { best = m; bf = fr; } }
  return best - mag(d0, bf);
}
static bool stableDO(double c, double K, double fh)
{
  D d; mkD(d, c, K, fh); d.getSample(1e-3); int N = (int)(SR * 1.2); double pk = 0, tail = 0;
  for (int i = 0; i < N; i++) { double y = fabs(d.getSample(0.0)); if (i < (int)(SR * 0.3)) pk = fmax(pk, y); if (i > N - (int)(SR * 0.1)) tail = fmax(tail, y); }
  return tail < pk * 1e-3;
}
int main(int argc, char** argv)
{
  if (argc < 4) { fprintf(stderr, "usage: k_target_solver <cutoffParam> <teebeeFbHp> <diodeFbHp>\n"); return 2; }
  double c = atof(argv[1]), fhT = atof(argv[2]), fhD = atof(argv[3]);
  double lo = 12, hi = 30; for (int i = 0; i < 14; i++) { double m = 0.5 * (lo + hi); if (stableDO(c, m, fhD)) lo = m; else hi = m; }
  double Kc = lo;
  printf("cut %6.0f fhT %.0f fhD %.1f | K_crit %.2f |", c, fhT, fhD, Kc);
  for (double res : { 20.0, 40.0, 60.0, 80.0, 100.0 })
  {
    rosic::TeeBeeFilter t, t0;
    for (rosic::TeeBeeFilter* x : { &t, &t0 }) { x->setSampleRate(SR); x->setFeedbackHighpassCutoff(fhT); x->setCutoff(c); x->setResonance(x == &t ? res : 0.0); x->reset(); }
    double best = -1e9, bf = 0;
    for (double fr = 0.6 * c; fr <= 1.6 * c; fr *= 1.05) { double m = mag(t, fr); if (m > best) { best = m; bf = fr; } }
    for (double fr = bf / 1.05; fr <= bf * 1.05; fr *= 1.012) { double m = mag(t, fr); if (m > best) { best = m; bf = fr; } }
    double Btb = best - mag(t0, bf);
    double a = 0.5, b = Kc * 0.9995, Bmax = boostDO(c, b, fhD), Kt;
    if (Btb >= Bmax) Kt = -b;   // TeeBee is closer to oscillation than the diode can safely get: flagged with *
    else { for (int i = 0; i < 14; i++) { double m = 0.5 * (a + b); if (boostDO(c, m, fhD) < Btb) a = m; else b = m; } Kt = 0.5 * (a + b); }
    printf("  r%.0f: TB %.1f dB -> K %.2f (%.3f Kc)", res, Btb, fabs(Kt), fabs(Kt) / Kc); if (Kt < 0) printf("*");
  }
  printf("\n"); return 0;
}
