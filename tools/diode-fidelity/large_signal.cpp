// Large-signal check against SPICE (spice/large_targets.txt, from spice/large_targets.py): sine at the VCF input on a
// grid of cutoff x pot x frequency x amplitude, compression (fundamental gain re small-signal, dB) and THD (harmonics
// 2-5, %) at the output, for both filter qualities. The input is scaled so the Q12 tanh's argument equals the circuit's
// (amp / 5.2217 at 0 dB drive: the 220k / 2.2k input divider and 2 Vt).
// usage: large_signal      Exit 0 if every quality is within its limits (rms over the grid, per amplitude).
#include "dfl_DiodeLadderFilter.h"
#include <cmath>
#include <complex>
#include <cstdio>
#include <string>
#include <vector>
using D = dfl::DiodeLadderFilter;
static const double SR = 4.0 * 44100.0;
struct Pt { double cut, pot, f, amp, comp, thd; };
static std::vector<double> harm(bool hiFi, const Pt& p, double A, int nh = 5)
{
  D d; d.setSampleRate(SR); d.setHiFi(hiFi); d.setInputDrive(0.0); d.setSaturationBias(0.0);
  d.setFeedbackHighpass(D::FEEDBACK_DEFAULT); d.setCutoff(p.cut);
  d.setResonance((1.0 - std::exp(-3.0 * p.pot)) / (1.0 - std::exp(-3.0))); d.reset();
  const double tail = std::max(0.1, 10.0 / p.f); const int nc = (int)std::lround(tail * p.f);
  const int N = (int)std::lround(nc * SR / p.f), S = (int)SR - N; const double w = 2 * M_PI * p.f / SR;
  std::vector<std::complex<double>> acc(nh);
  for (int n = 0; n < S + N; n++)
  {
    const double y = d.getSample(A * std::sin(w * n));
    if (n >= S) for (int k = 0; k < nh; k++) acc[k] += y * std::polar(1.0, -(k + 1) * w * n);
  }
  std::vector<double> h(nh); for (int k = 0; k < nh; k++) h[k] = 2 * std::abs(acc[k]) / N; return h;
}
int main()
{
  const std::string dir = std::string(__FILE__).substr(0, std::string(__FILE__).find_last_of('/') + 1);
  FILE* fp = fopen((dir + "spice/large_targets.txt").c_str(), "r");
  if (!fp) { printf("spice/large_targets.txt not found\n"); return 1; }
  std::vector<Pt> pts; Pt p; double x[4];
  while (fscanf(fp, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf", &p.cut, &p.pot, &p.f, &p.amp, &x[0], &x[1], &x[2], &x[3], &p.comp, &p.thd) == 10) pts.push_back(p);
  fclose(fp);
  // rms limits [quality][amp index: 1.625 / 3.25, 6.5] = { comp dB, THD points }
  const double LIM[2][2][2] = { { { 0.3, 0.6 }, { 0.6, 2.0 } }, { { 0.15, 0.25 }, { 0.25, 0.5 } } };
  int bad = 0;
  for (int q = 1; q >= 0; q--)
  {
    const double amps[3] = { 1.625, 3.25, 6.5 };
    for (double amp : amps)
    {
      double ec = 0, et = 0, wc = 0; int n = 0;
      for (const Pt& t : pts)
      {
        if (t.amp != amp) continue;
        const double A = amp / 2.6109;
        auto hs = harm(q, t, 1e-4), hl = harm(q, t, A);
        double thd = 0; for (size_t k = 1; k < hl.size(); k++) thd += hl[k] * hl[k];
        const double comp = 20 * std::log10((hl[0] / A) / (hs[0] / 1e-4)), th = 100 * std::sqrt(thd) / hl[0];
        ec += (comp - t.comp) * (comp - t.comp); et += (th - t.thd) * (th - t.thd); wc = std::max(wc, std::fabs(comp - t.comp)); n++;
      }
      ec = std::sqrt(ec / n); et = std::sqrt(et / n);
      const auto& L = LIM[q][amp > 4.0 ? 1 : 0];
      const bool ok = ec <= L[0] && et <= L[1]; bad += !ok;
      printf("%-8s amp %5.3f V: compression %.2f dB rms (worst %.2f, limit %.2f), THD %.2f pts rms (limit %.2f)  %s\n",
             q ? "Hi-Fi" : "Standard", amp, ec, wc, L[0], et, L[1], ok ? "ok" : "FAIL");
    }
  }
  return bad ? 1 : 0;
}
