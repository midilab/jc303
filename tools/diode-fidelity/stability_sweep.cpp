// Stability sweep for the zero-delay diode ladder near its self-oscillation threshold.
// Kick the filter with a small impulse, then run in silence: every case must decay (the tail
// after 1.5 s must be below -60 dB re the early ring, or, for a slow subsonic ring, not growing over 6 s). Covers the Diode Octave feedback law at
// resonance 0.97/0.99/1.0 over cutoff 200-20000 Hz, all feedback-HP corners the plugin can set
// (Native 31.25-268 Hz, Octave 76.7-268 Hz, plus margin), and the drive range (setInputDrive clamps to [-12, +9] dB, so the
// 12/14/16 dB entries also test the clamp). Runs Diode Octave and Diode Native (whose Filter Feedback knob
// scales the dominant pole of its coupling network).
// Exit code 0 = every case decays.
#include "dfl_DiodeLadderFilter.h"
#include <cstdio>
#include <cmath>
using D = dfl::DiodeLadderFilter;
static const double SR = 4.0 * 44100.0;   // the plugin runs the filter at 4x oversampling

static bool growing(D& d, bool native, double c, double fh, double drive, double res)
{
  d.setSampleRate(SR); d.setOctaveMode(true); d.setNativeMode(native); d.setResponseMode(D::RESPONSE_LP);
  d.setInputDrive(drive); d.setPassbandCompensation(0.0);
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

int main()
{
  int n = 0, bad = 0, slow = 0; double worst = -400, worstC = 0, worstF = 0, worstD = 0;
  for (bool native : { false, true })
  for (double fh : { 31.25, 45.0, 60.0, 76.7, 90.0, 100.0, 122.0, 160.0, 200.0, 268.0 })
    if (native || fh >= 76.7)   // the corners below 76.7 exist only in Native (Open303's knob mapping)
    for (double drive : { -12.0, -9.0, -6.0, -3.0, 0.0, 4.5, 9.0, 12.0, 14.0, 16.0 })
      for (int i = 0; i < 40; i++)
      {
        double c = 200 * pow(100.0, i / 39.0);
        for (double res : { 0.97, 0.99, 1.0 })
        {
          D d; d.setSampleRate(SR); d.setOctaveMode(true); d.setNativeMode(native); d.setResponseMode(D::RESPONSE_LP);
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
          // A slow ring is not a failure: near its threshold the coupling network's ~7 Hz mode (low Filter Feedback
          // corner, Native) decays over seconds. Fail only if it is also growing (tiny kick, so the loop stays linear).
          if (rel > -60 && std::isfinite(tail) && !growing(d, native, c, fh, drive, res)) slow++;
          else if (rel > -60) { bad++; printf("  FAIL %s cutoff %.0f fh %.1f drive %.1f res %.2f tail %.0f dB\n", native ? "native" : "octave", c, fh, drive, res, rel); }
          if (rel > worst) { worst = rel; worstC = c; worstF = fh; worstD = drive; }
        }
      }
  printf("stability sweep: %d cases, %d failing (not below -60 dB and growing), %d slow but decaying; worst tail %.0f dB (cutoff %.0f, fh %.1f, drive %.1f)\n", n, bad, slow, worst, worstC, worstF, worstD);
  return bad ? 1 : 0;
}
