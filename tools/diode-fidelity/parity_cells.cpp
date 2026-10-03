// Small-signal parity of Diode Octave against TeeBee, cell by cell: for each (cutoff, resonance,
// feedback-HP knob position) compare the resonant boost (peak / res-0 response at the same
// frequency), the peak frequency relative to the cutoff, and the passband level. Both filters get
// the same cutoff parameter, as in the plugin; the diode's feedback HP is TeeBee's x 115/150.
// usage: parity_cells [driveDb=-3] [-v]      Exit 0 if the summary scores are within the limits below.
#include "rosic_TeeBeeFilter.h"
#include "dfl_DiodeLadderFilter.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <complex>
#include <vector>
using D = dfl::DiodeLadderFilter;
static const double SR = 4.0 * 44100.0;
static double skew(double p) { double r = 0.01 * p; return (1.0 - exp(-3.0 * r)) / (1.0 - exp(-3.0)); }
template <class F> static double mag(F& f, double fr, double amp = 1e-3, double settle = 0.2, double meas = 0.1)
{
  f.reset(); int S = (int)(SR * settle), N = (int)(SR * meas); std::complex<double> a = 0; double w = 2 * M_PI * fr / SR;
  for (int n = 0; n < S + N; n++) { double y = f.getSample(amp * sin(w * n)); if (n >= S) a += y * std::polar(1.0, -w * n); }
  return 20 * log10(2 * std::abs(a) / N / amp + 1e-30);
}
struct Cell { double boost, ratio, pass; };
// the peak is only ~2% wide at 35-45 dB, so refine the coarse scan (a 4% grid misses it by several dB)
template <class F> static Cell cellOf(F& f, F& f0, double c)
{
  double best = -1e9, bf = 0;
  for (double fr = 0.35 * c; fr <= 2.0 * c; fr *= 1.04) { double m = mag(f, fr); if (m > best) { best = m; bf = fr; } }
  for (double fr = bf / 1.04; fr <= bf * 1.04; fr *= 1.008) { double m = mag(f, fr); if (m > best) { best = m; bf = fr; } }
  return { best - mag(f0, bf), bf / c, mag(f, 0.2 * bf) };
}
int main(int argc, char** argv)
{
  double drive = -3; bool verbose = false;
  for (int i = 1; i < argc; i++) { if (!strcmp(argv[i], "-v")) verbose = true; else drive = atof(argv[i]); }
  const double cuts[] = { 500, 1500, 3000, 6000, 10000 }, ress[] = { 30, 60, 100 };
  const double fhs[2][2] = { { 159, 122 }, { 100, 76.7 } };   // {TeeBee, diode}: knob default, knob 1.0
  double sb = 0, sr = 0, n = 0, worstB = 0, worstR = 0; std::vector<double> pass;
  for (int h = 0; h < 2; h++) for (double c : cuts) for (double res : ress)
  {
    rosic::TeeBeeFilter t, t0;
    for (rosic::TeeBeeFilter* x : { &t, &t0 }) { x->setSampleRate(SR); x->setFeedbackHighpassCutoff(fhs[h][0]); x->setCutoff(c); x->setResonance(x == &t ? res : 0.0); x->reset(); }
    D d, d0;
    for (D* x : { &d, &d0 }) { x->setSampleRate(SR); x->setOctaveMode(true); x->setResponseMode(D::RESPONSE_LP); x->setInputDrive(drive); x->setPassbandCompensation(0.0); x->setFeedbackHighpass(fhs[h][1]); x->setCutoff(c); x->setResonance(skew(x == &d ? res : 0.0)); x->reset(); }
    Cell a = cellOf(t, t0, c), b = cellOf(d, d0, c);
    double db = b.boost - a.boost, dr = 20 * log10(b.ratio / a.ratio), dp = b.pass - a.pass;
    sb += pow(db / 2.0, 2); sr += dr * dr; n++; pass.push_back(dp); worstB = fmax(worstB, fabs(db)); worstR = fmax(worstR, fabs(dr));
    if (verbose) printf(" fh%d cut %5.0f res %3.0f: dBoost %+5.1f  peak/cut TB %.2f DO %.2f (%+.1f%%)  dPass %+5.1f\n", h, c, res, db, a.ratio, b.ratio, 100 * (b.ratio / a.ratio - 1), dp);
  }
  double mp = 0; for (double v : pass) mp += v; mp /= pass.size(); double sp = 0; for (double v : pass) sp += (v - mp) * (v - mp);
  double ss = 0; int cnt = 0;   // stopband shape at resonance 0 (relative to the passband), default knob
  for (double c : { 800.0, 2000.0, 5000.0 })
  {
    rosic::TeeBeeFilter t; t.setSampleRate(SR); t.setFeedbackHighpassCutoff(159); t.setCutoff(c); t.setResonance(0); t.reset();
    D d; d.setSampleRate(SR); d.setOctaveMode(true); d.setResponseMode(D::RESPONSE_LP); d.setInputDrive(drive); d.setPassbandCompensation(0.0); d.setFeedbackHighpass(122); d.setCutoff(c); d.setResonance(0); d.reset();
    for (double m : { 0.25, 0.5, 1.0, 2.0, 4.0 }) { double e = (mag(d, m * c) - mag(d, 0.05 * c)) - (mag(t, m * c) - mag(t, 0.05 * c)); ss += e * e; cnt++; }
  }
  ss /= cnt; sb /= n; sr /= n; sp /= pass.size();
  printf("parity (drive %.1f dB): boost term %.2f (worst %.1f dB)  peak-freq term %.2f (worst %.1f dB)  passband spread %.2f (mean offset %+.2f dB)  res0 shape %.2f\n", drive, sb, worstB, sr, worstR, sp, mp, ss);
  printf("limits: boost term <= 0.30, worst boost <= 4.0 dB, peak-freq term <= 0.10, res0 shape <= 1.0\n");
  return (sb <= 0.30 && worstB <= 4.0 && sr <= 0.10 && ss <= 1.0) ? 0 : 1;
}
