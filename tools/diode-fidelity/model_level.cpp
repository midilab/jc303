// Global level between filter models (Open303::FILTER_LEVEL): each model vs TeeBee, full synth, saw, resonance 0, RMS over
// a 0.4 s note, mean over C2 / C3 / C4 and cutoff 400 / 1000 / 2500 Hz (env mod 60). Prints the level at resonance
// 0 / 50 / 100 as well (only resonance 0 is matched).
// usage: model_level      Exit 0 if the resonance-0 level is within LIMIT_DB.
#include "rosic_Open303.h"
#include <cmath>
#include <cstdio>
static const double SR = 44100.0, LIMIT_DB = 0.25;
static double rmsDb(dfl::FilterType ft, double cutoff, double res, int note)
{
  rosic::Open303 s; s.setSampleRate(SR); s.setFilterType(ft); s.setCutoff(cutoff); s.setResonance(res);
  s.setEnvMod(60.0); s.setDecay(400.0); s.setAccent(80.0); s.setVolume(-12.0); s.setWaveform(0.0);
  s.noteOn(note, 64, 0.0);
  const int n = (int)(0.4 * SR); double acc = 0.0;
  for (int i = 0; i < n; i++) { double y = s.getSample(); if (i > n / 4) acc += y * y; }
  return 10.0 * log10(acc / (0.75 * n) + 1e-24);
}
int main()
{
  const int notes[3] = { 36, 48, 60 }; const double cuts[3] = { 400.0, 1000.0, 2500.0 };
  const char* names[dfl::NUM_FILTER_TYPES] = { "TeeBee", "Spicy", "MissBehave", "JuneBug", "Wasabi", "BitterSweet", "Hornet", "Oddity" };
  bool ok = true;
  for (int ft = dfl::FILTER_DIODE; ft < dfl::NUM_FILTER_TYPES; ++ft)
  {
    double at0 = 0.0;
    for (double res : { 0.0, 50.0, 100.0 })
    {
      double d = 0.0;
      for (int note : notes) for (double c : cuts)
        d += rmsDb((dfl::FilterType) ft, c, res, note) - rmsDb(dfl::FILTER_TEEBEE, c, res, note);
      d /= 9.0; if (res == 0.0) at0 = d;
      printf("%s vs TeeBee at resonance %3.0f: %+.2f dB\n", names[ft], res, d);
    }
    ok = ok && std::fabs(at0) <= LIMIT_DB;
  }
  printf("%s (resonance 0 within %.2f dB)\n", ok ? "ok" : "FAIL", LIMIT_DB);
  return ok ? 0 : 1;
}
