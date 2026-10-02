// Stability sweep for the zero-delay diode ladder near its self-oscillation threshold.
// Kick the filter with a small impulse, then run in silence: every case must decay (the tail
// after 1.5 s must be below -60 dB re the early ring). Covers the Diode Octave feedback law at
// resonance 0.97/0.99/1.0 over cutoff 200-20000 Hz, all feedback-HP corners the plugin can set
// (76.7-268 Hz plus margin), and the drive range (setInputDrive clamps to [-12, +9] dB, so the
// 12/14/16 dB entries also test the clamp).
// Exit code 0 = every case decays.
#include "dfl_DiodeLadderFilter.h"
#include <cstdio>
#include <cmath>
using D = dfl::DiodeLadderFilter;
static const double SR = 4.0 * 44100.0;   // the plugin runs the filter at 4x oversampling

int main()
{
  int n = 0, bad = 0; double worst = -400, worstC = 0, worstF = 0, worstD = 0;
  for (double fh : { 76.7, 90.0, 100.0, 122.0, 160.0, 200.0, 268.0 })
    for (double drive : { -12.0, -9.0, -6.0, -3.0, 0.0, 4.5, 9.0, 12.0, 14.0, 16.0 })
      for (int i = 0; i < 40; i++)
      {
        double c = 200 * pow(100.0, i / 39.0);
        for (double res : { 0.97, 0.99, 1.0 })
        {
          D d; d.setSampleRate(SR); d.setOctaveMode(true); d.setResponseMode(D::RESPONSE_LP);
          d.setInputDrive(drive); d.setPassbandCompensation(0.0);
          d.setFeedbackHighpass(fh); d.setCutoff(c); d.setResonance(res); d.reset();
          d.getSample(1e-3);
          int N = (int)(SR * 1.5); double pk = 0, tail = 0;
          for (int k = 0; k < N; k++)
          {
            double y = fabs(d.getSample(0.0));
            if (!std::isfinite(y)) { tail = 1e9; break; }
            if (k < (int)(SR * 0.3)) pk = fmax(pk, y);
            if (k > N - (int)(SR * 0.2)) tail = fmax(tail, y);
          }
          double rel = 20 * log10(tail / pk + 1e-300); n++;
          if (rel > -60) { bad++; printf("  FAIL cutoff %.0f fh %.1f drive %.1f res %.2f tail %.0f dB\n", c, fh, drive, res, rel); }
          if (rel > worst) { worst = rel; worstC = c; worstF = fh; worstD = drive; }
        }
      }
  printf("stability sweep: %d cases, %d not decaying below -60 dB; worst tail %.0f dB (cutoff %.0f, fh %.1f, drive %.1f)\n", n, bad, worst, worstC, worstF, worstD);
  return bad ? 1 : 0;
}
