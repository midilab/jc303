// Full-synth stress test: every diode filter type x drive x cutoff x env-mod x Devilfish FM,
// at maximum resonance with accent 100, full bass comp (Diode) or pure BP / HP, and the Res Trim extremes. Fails on any non-finite sample,
// on a sustained tail after gate-off (> -100 dB), or an absurd peak (> 20).
#include "rosic_Open303.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
int main()
{
  struct Case { dfl::FilterType type; double trim, morph; const char* name; };
  const Case cases[] = {
    { dfl::FILTER_DIODE,    1.0,  1.0, "Diode" },
    { dfl::FILTER_DIODE,    0.85, 1.0, "Diode trim 0.85" },
    { dfl::FILTER_DIODE,    1.15, 1.0, "Diode trim 1.15" },
    { dfl::FILTER_DIODE_BP, 1.15, 0.0, "BP trim 1.15" },
    { dfl::FILTER_DIODE_HP, 1.15, 0.0, "HP trim 1.15" } };
  int bad = 0, n = 0; double worstTail = -400, worstPeak = 0;
  for (const Case& c : cases)
    for (double drive : { -3.0, 0.0, 4.5, 9.0, 16.0 })
      for (double cut : { 314.0, 1200.0, 2394.0 })
        for (double env : { 0.0, 60.0, 100.0 })
          for (double fm : { 0.0, 1.0 })
          {
            rosic::Open303 s; s.setSampleRate(44100); s.setFilterType(c.type); s.setResonanceTrim(c.trim);
            s.setCutoff(cut); s.setResonance(100); s.setEnvMod(env); s.setDecay(400); s.setAccent(100);
            s.setVolume(-12); s.setFilterDrive(drive); s.setPassbandCompensation(c.morph); s.setFilterFmDepth(fm); s.setWaveform(0.0);
            s.noteOn(36, 127, 0.0);
            double tail = 0, peak = 0; int N = 44100 * 2; bool finite = true;
            for (int i = 0; i < N; i++)
            {
              if (i == 44100 / 4) s.noteOn(36, 0, 0.0);
              double y = s.getSample();
              if (!std::isfinite(y)) { finite = false; break; }
              peak = fmax(peak, fabs(y)); if (i > N - 4410) tail = fmax(tail, fabs(y));
            }
            n++; double t = 20 * log10(tail + 1e-300);
            if (!finite || t > -100 || peak > 20) { bad++; printf("  FAIL %s drive %.1f cutoff %.0f env %.0f fm %.0f: finite=%d tail %.0f dB peak %.2f\n", c.name, drive, cut, env, fm, (int)finite, t, peak); }
            worstTail = fmax(worstTail, t); worstPeak = fmax(worstPeak, peak);
          }
  printf("stress test: %d runs, %d failures; worst tail %.0f dB, worst peak %.2f\n", n, bad, worstTail, worstPeak);
  return bad ? 1 : 0;
}
