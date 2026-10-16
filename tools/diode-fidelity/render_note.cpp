// Renders one note through rosic::Open303 to raw 32-bit float mono (44.1 kHz) on stdout.
// usage: render_note <filterType> <cutoffHz> <res0to100> <envMod0to100> <decayMs> <accent0/1> <gateOffSec> <totalSec>
//                    <driveDb> <bassComp0to1> <midiNote> [feedbackHpHz] [bias0to1] [resTrim] [mode0to1] [hiFi0/1] [tension] [rail]
// filterType: 0 TeeBee, 1 Diode. resTrim: diode loop-gain scale (1 = nominal, 0.85..1.15). mode: 0 LP, 0.5 BP, 1 HP.
// hiFi: diode filter quality (default 1, Hi-Fi). tension: -1..1 (0 = the circuit's diodes). rail: 0..1 (1 = unlimited, the default).
// Saw wave, volume -12 dB.
#include "rosic_Open303.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
int main(int argc, char** argv)
{
  if (argc < 12) { fprintf(stderr, "usage: render_note <filterType> <cutoffHz> <res> <envMod> <decayMs> <accent> <gateOffSec> <totalSec> <driveDb> <bassComp> <midiNote> [feedbackHpHz] [bias] [resTrim] [mode] [hiFi] [tension] [rail]\n"); return 2; }
  int ft = atoi(argv[1]); double cutoff = atof(argv[2]), res = atof(argv[3]), envMod = atof(argv[4]), decay = atof(argv[5]);
  int accent = atoi(argv[6]); double offSec = atof(argv[7]), totSec = atof(argv[8]), drive = atof(argv[9]), bassComp = atof(argv[10]); int note = atoi(argv[11]);
  const double sr = 44100.0;
  rosic::Open303 s; s.setSampleRate(sr); s.setFilterType((dfl::FilterType)ft); s.setCutoff(cutoff); s.setResonance(res);
  s.setEnvMod(envMod); s.setDecay(decay); s.setAccent(80.0); s.setVolume(-12.0); s.setFilterDrive(drive);
  s.setPassbandCompensation(bassComp); s.setWaveform(0.0);
  if (argc > 12 && atof(argv[12]) > 0.0) s.setFeedbackHighpass(atof(argv[12]));
  if (argc > 13) s.setFilterBias(atof(argv[13]));
  if (argc > 14) s.setResonanceTrim(atof(argv[14]));
  if (argc > 15) s.setFilterMode(atof(argv[15]));
  if (argc > 16) s.setFilterHiFi(atoi(argv[16]) != 0);
  if (argc > 17) s.setFilterTension(atof(argv[17]));
  if (argc > 18) s.setFilterRail(atof(argv[18]));
  int total = (int)(totSec * sr), off = (int)(offSec * sr); std::vector<float> buf(total);
  s.noteOn(note, accent ? 127 : 64, 0.0);
  for (int i = 0; i < total; i++) { if (i == off) s.noteOn(note, 0, 0.0); buf[i] = (float)s.getSample(); }
  fwrite(buf.data(), sizeof(float), buf.size(), stdout);
  return 0;
}
