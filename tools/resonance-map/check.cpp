// Checks dfl_ResonanceMap.h: through the knob -> pot map, each circuit-native model must reach +6 / +12 / +20 dB of
// small-signal peak at knob 0.45 / 0.70 / 0.82 (within 0.03) and self-oscillate from 0.88..0.92 (the EDP Wasp: not at
// all). The Wasp's resonance law moves with the cutoff (its CMOS inverters' loop gain), so its crossings are held only
// at the 1 kHz the map was fitted at and reported elsewhere. Exit 0 if all pass.
// usage: check [cutoffHz]
#include "resonance_common.h"
#include "dfl_ResonanceMap.h"
#include <cstdio>
#include <cstdlib>

double probeAmp = 1e-2;

static bool ok = true;

// lowOsc: a model that does not self-oscillate at the fit cutoff (1 kHz) but does at low cutoffs (the A-124): its "no self-oscillation" is held at 1 kHz only and reported elsewhere
template <class F, class Make> static void check(const char* name, dfl::FilterType type, Make make, double fc,
                                                  bool oscillator = true, double amp = 1e-2, bool lowOsc = false)
{
  probeAmp = amp;
  auto potAt = [&](double knob) { return dfl::resonancePot(type, knob); };
  printf("%-10s", name);
  const double targets[3][2] = { { 6.0, 0.45 }, { 12.0, 0.70 }, { 20.0, 0.82 } };
  for (auto& t : targets)
  {
    double lo = 0.0, hi = 0.88;   // knob where the peak crosses the target
    for (int it = 0; it < 12; ++it) { const double k = 0.5 * (lo + hi); (peakDb<F>(make, potAt(k), fc) >= t[0] ? hi : lo) = k; }
    const double k = 0.5 * (lo + hi), err = k - t[1];
    if (oscillator || std::fabs(fc - 1000.0) < 1.0) ok = ok && std::fabs(err) <= 0.03;
    printf("  +%2.0f dB at %.3f (%+.3f)", t[0], k, err);
  }
  if (!oscillator)
  {
    auto g = make(); g->setSampleRate(SR); g->setCutoff(fc); g->setResonance(resFor<F>(potAt(1.0))); g->reset();
    const bool osc = oscillates(*g);
    const bool held = !lowOsc || std::fabs(fc - 1000.0) < 1.0;
    ok = ok && (!osc || !held);
    printf("  %s\n", osc ? (held ? "OSCILLATES (expected none)" : "self-oscillates at full RES (expected at low cutoffs)")
                          : "no self-oscillation");
    return;
  }
  double lo = 0.5, hi = 1.0;
  for (int it = 0; it < 12; ++it)
  {
    const double k = 0.5 * (lo + hi);
    auto g = make(); g->setSampleRate(SR); g->setCutoff(fc); g->setResonance(resFor<F>(potAt(k))); g->reset();
    (oscillates(*g) ? hi : lo) = k;
  }
  const double onset = 0.5 * (lo + hi);
  ok = ok && onset >= 0.88 && onset <= 0.92;
  printf("  onset %.3f\n", onset);
}

int main(int argc, char** argv)
{
  const double fc = argc > 1 ? atof(argv[1]) : 1000.0;
  using M = dfl::MS20Filter; using R = dfl::RolandFilter; using W = dfl::WaspFilter; using S = dfl::SEMFilter; using A = dfl::ARP4075Filter;
  check<M>("MissBehave", dfl::FILTER_MS20,     [] { return std::make_unique<M>(); }, fc);
  check<R>("JuneBug",    dfl::FILTER_JUNO106,  [] { return std::make_unique<R>(); }, fc);
  // the Wasp's peak compresses early (0.25 V costs 7 dB at full RES), so it is probed smaller
  check<W>("Wasabi",     dfl::FILTER_WASP,     [] { return std::make_unique<W>(W::Revision::EDP); }, fc, false, 1e-3);
  // the SEM rings but does not sustain; its D11 / D12 compress the peak from a few volts of BP, so probed small too
  check<S>("BitterSweet", dfl::FILTER_SEM_NOTCH, [] { return std::make_unique<S>(); }, fc, false, 1e-3);
  // the A-124: higher resonance, oscillating at low cutoffs; its peak compresses like the EDP's
  check<W>("Hornet",     dfl::FILTER_WASP_A124, [] { return std::make_unique<W>(W::Revision::A124); }, fc, false, 1e-3, true);
  // the 4075's limiter compresses from ~10 mV at its input: probed small
  check<A>("Oddity",     dfl::FILTER_ARP4075,   [] { return std::make_unique<A>(); }, fc, true, 1e-3);
  // with the 2k2 tail mod (the same map: its law barely moves)
  check<A>("Oddity 2k2", dfl::FILTER_ARP4075,   [] { auto f = std::make_unique<A>(); f->setTailMod(true); return f; }, fc, true, 1e-3);
  printf("%s\n", ok ? "ok" : "FAIL");
  return ok ? 0 : 1;
}
