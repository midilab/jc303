// Stability sweep for the zero-delay diode ladder near its self-oscillation threshold.
// Kick the filter with a small impulse, then run in silence: every case must decay (the tail
// after 1.5 s must be below -60 dB re the early ring, or, for a slow subsonic ring, not growing over 6 s). Covers the feedback law at
// resonance 0.97/0.99/1.0 over cutoff 200-20000 Hz, the Filter Feedback corners the plugin can set (57.5-268 Hz, whose
// knob scales the dominant pole of the coupling network), and the drive range (setInputDrive clamps to [-12, +16] dB).
// The response mode only re-mixes the ladder taps, so the loop covers every mode.
// usage: stability_sweep [--standard] [--tension T] [--kick A]
//   --tension: the Hi-Fi rungs' tension (-1..1, default 0). --kick: a 20 ms sine burst of amplitude A at the cutoff
//   instead of the 1e-3 impulse, so the rungs ring loud (the tension acts only on large signals); a tail within 20 dB of
//   the peak then fails too.
// Exit code 0 = every case decays.
#include "dfl_DiodeLadderFilter.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <string>
using D = dfl::DiodeLadderFilter;
static const double SR = 4.0 * 44100.0;   // the plugin runs the filter at 4x oversampling
static bool HIFI = true;                   // --standard: the Standard quality
static double TENSION = 0.0, KICK = 0.0;

static bool growing(D& d, double c, double fh, double drive, double res)
{
  d.setSampleRate(SR);
  d.setHiFi(HIFI); d.setTension(TENSION); d.setInputDrive(drive); d.setPassbandCompensation(0.0);
  d.setFeedbackHighpass(fh); d.setCutoff(c); d.setResonance(res); d.reset();
  d.getSample(1e-9);
  int N = (int)(SR * 6.0); double early = 0, late = 0;
  for (int k = 0; k < N; k++)
  {
    double y = fabs(d.getSample(0.0));
    if (!std::isfinite(y)) return true;
    if (k > (int)(SR * 2.0) && k < (int)(SR * 3.0)) early = fmax(early, y);
    if (k > (int)(SR * 5.0)) late = fmax(late, y);
  }
  return late >= early || late >= 1e-7;
}

int main(int argc, char** argv)
{
  for (int i = 1; i < argc; i++)
  {
    const std::string a = argv[i];
    if (a == "--standard") HIFI = false;
    else if (a == "--tension" && i + 1 < argc) TENSION = atof(argv[++i]);
    else if (a == "--kick" && i + 1 < argc) KICK = atof(argv[++i]);
  }
  int n = 0, bad = 0, slow = 0; double worst = -400, worstC = 0, worstF = 0, worstD = 0;
  for (double fh : { 57.5, 65.0, 76.7, 90.0, 100.0, 122.0, 160.0, 200.0, 268.0 })
    for (double drive : { -12.0, -9.0, -6.0, -3.0, 0.0, 4.5, 9.0, 12.0, 14.0, 16.0 })
      for (int i = 0; i < 40; i++)
      {
        double c = 200 * pow(100.0, i / 39.0);
        for (double res : { 0.97, 0.99, 1.0 })
        {
          D d; d.setSampleRate(SR); d.setHiFi(HIFI); d.setTension(TENSION);
          d.setInputDrive(drive); d.setPassbandCompensation(0.0);
          d.setFeedbackHighpass(fh); d.setCutoff(c); d.setResonance(res); d.reset();
          const int burst = KICK > 0.0 ? (int)(0.02 * SR) : 1;
          int N = (int)(SR * 1.5); double pk = 0, tail = 0;
          for (int k = 0; k < N; k++)
          {
            const double x = KICK > 0.0 ? (k < burst ? KICK * sin(2 * M_PI * c * k / SR) : 0.0) : (k == 0 ? 1e-3 : 0.0);
            double y = fabs(d.getSample(x));
            if (!std::isfinite(y)) { tail = 1e9; break; }
            if (k < (int)(SR * 0.3)) pk = fmax(pk, y);
            if (k > N - (int)(SR * 0.2)) tail = fmax(tail, y);
          }
          double rel = 20 * log10(tail / pk + 1e-300); n++;
          // A slow ring is not a failure: near its threshold the coupling network's ~7 Hz mode (low Filter Feedback
          // corner) decays over seconds. Fail only if it is also growing (tiny kick, so the loop stays linear), or, after a
          // loud kick, if it is still within 20 dB of its peak (a large-signal ring the small-signal check cannot see).
          const bool latched = KICK > 0.0 && rel > -20;
          if (rel > -60 && !latched && std::isfinite(tail) && !growing(d, c, fh, drive, res)) slow++;
          else if (rel > -60) { bad++; printf("  FAIL cutoff %.0f fh %.1f drive %.1f res %.2f tail %.0f dB\n", c, fh, drive, res, rel); }
          if (rel > worst) { worst = rel; worstC = c; worstF = fh; worstD = drive; }
        }
      }
  printf("stability sweep (%s, tension %g, kick %g): %d cases, %d failing (not below -60 dB and growing), %d slow but decaying; worst tail %.0f dB (cutoff %.0f, fh %.1f, drive %.1f)\n", HIFI ? "Hi-Fi" : "Standard", TENSION, KICK, n, bad, slow, worst, worstC, worstF, worstD);
  return bad ? 1 : 0;
}
