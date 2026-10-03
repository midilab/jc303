// Renders one note through rosic::Open303 to raw 32-bit float mono (44.1 kHz) on stdout.
// usage: render_note <filterType> <cutoffHz> <res0to100> <envMod0to100> <decayMs> <accent0/1> <gateOffSec> <totalSec>
//                    <driveDb> <hpbpMorph0to1> <midiNote> [feedbackHpHz] [bias0to1]
// filterType: 0 TeeBee, 1 Diode Octave, 2 Diode, 3 Diode BP, 4 Diode HP. Saw wave, volume -12 dB.
#include "rosic_Open303.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
int main(int argc, char** argv)
{
  if (argc < 12) { fprintf(stderr, "usage: render_note <filterType> <cutoffHz> <res> <envMod> <decayMs> <accent> <gateOffSec> <totalSec> <driveDb> <morph> <midiNote> [feedbackHpHz]\n"); return 2; }
  int ft = atoi(argv[1]); double cutoff = atof(argv[2]), res = atof(argv[3]), envMod = atof(argv[4]), decay = atof(argv[5]);
  int accent = atoi(argv[6]); double offSec = atof(argv[7]), totSec = atof(argv[8]), drive = atof(argv[9]), morph = atof(argv[10]); int note = atoi(argv[11]);
  const double sr = 44100.0;
  rosic::Open303 s; s.setSampleRate(sr); s.setFilterType((rosic::FilterType)ft); s.setCutoff(cutoff); s.setResonance(res);
  s.setEnvMod(envMod); s.setDecay(decay); s.setAccent(80.0); s.setVolume(-12.0); s.setFilterDrive(drive);
  s.setPassbandCompensation(morph); s.setWaveform(0.0);
  if (argc > 12 && atof(argv[12]) > 0.0) s.setFeedbackHighpass(atof(argv[12]));
  if (argc > 13) s.setFilterBias(atof(argv[13]));
  int total = (int)(totSec * sr), off = (int)(offSec * sr); std::vector<float> buf(total);
  s.noteOn(note, accent ? 127 : 64, 0.0);
  for (int i = 0; i < total; i++) { if (i == off) s.noteOn(note, 0, 0.0); buf[i] = (float)s.getSample(); }
  fwrite(buf.data(), sizeof(float), buf.size(), stdout);
  return 0;
}
