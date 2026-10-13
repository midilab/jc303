// Resonance law of the circuit-native models (MS-20 Korg35, Juno-106, Wasp, SEM, ARP 4075) against their own pot: the
// small-signal peak height above the passband (LP, a 10 mV-scale sine swept around the cutoff, 1 kHz by default) at pot 0..1, and the
// self-oscillation onset (bisection on a kick: the ring still alive 1.5 s later). Prints a table per model; `fit`
// turns it into dfl_ResonanceMap.h's breakpoints.
// usage: measure [cutoffHz] [probe amplitude, default 0.01] [wasp | hornet | sem | arp: that model only]
#include "resonance_common.h"
#include <cstdio>
#include <cstdlib>
#include <string>

double probeAmp = 1e-2;   // above the Korg35 solver's ~3e-7 noise floor, still small-signal

template <class F, class Make> static void run(const char* name, Make make, double fc)
{
  printf("%s\n", name);
  for (int i = 0; i <= 100; ++i)
  {
    const double p = i / 100.0;
    auto g = make(); g->setSampleRate(SR); g->setCutoff(fc); g->setResonance(resFor<F>(p)); g->reset();
    if (oscillates(*g)) { printf("  pot %.2f oscillates\n", p); break; }
    printf("  pot %.2f peak %7.3f dB\n", p, peakDb<F>(make, p, fc));
  }
  double lo = 0.0, hi = 1.0;
  for (int it = 0; it < 14; ++it)
  {
    const double p = 0.5 * (lo + hi);
    auto g = make(); g->setSampleRate(SR); g->setCutoff(fc); g->setResonance(resFor<F>(p)); g->reset();
    (oscillates(*g) ? hi : lo) = p;
  }
  printf("  onset %.4f\n", 0.5 * (lo + hi));
}

int main(int argc, char** argv)
{
  const double fc = argc > 1 ? atof(argv[1]) : 1000.0;
  if (argc > 2) probeAmp = atof(argv[2]);
  using M = dfl::MS20Filter; using R = dfl::RolandFilter; using W = dfl::WaspFilter; using S = dfl::SEMFilter; using A = dfl::ARP4075Filter;
  const std::string only = argc > 3 ? argv[3] : "";
  // the SEM: the map measures LP (its MODE pot's start)
  if (only == "arp") { run<A>("Oddity", [] { return std::make_unique<A>(); }, fc); return 0; }
  if (only == "sem") { run<S>("BitterSweet", [] { return std::make_unique<S>(); }, fc); return 0; }
  if (only == "hornet") { run<W>("Hornet", [] { return std::make_unique<W>(W::Revision::A124); }, fc); return 0; }
  if (!only.empty()) { run<W>("Wasabi", [] { return std::make_unique<W>(W::Revision::EDP); }, fc); return 0; }
  run<M>("MissBehave", [] { return std::make_unique<M>(); }, fc);
  run<R>("JuneBug",    [] { return std::make_unique<R>(); }, fc);
  run<W>("Wasabi",     [] { return std::make_unique<W>(W::Revision::EDP); }, fc);
  run<S>("BitterSweet", [] { return std::make_unique<S>(); }, fc);
  run<W>("Hornet",     [] { return std::make_unique<W>(W::Revision::A124); }, fc);
  run<A>("Oddity",     [] { return std::make_unique<A>(); }, fc);
}
