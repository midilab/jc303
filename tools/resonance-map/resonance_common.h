// Shared by measure / check: the small-signal peak height and the self-oscillation test.
#pragma once
#include "dfl_MS20Filter.h"
#include "dfl_RolandFilter.h"
#include "dfl_WaspFilter.h"
#include "dfl_SEMFilter.h"
#include "dfl_ARP4075Filter.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <memory>

extern double probeAmp;   // sine amplitude (normalised input units), defined by each tool

static const double SR = 4 * 44100.0;

template <class F> static double resFor(double pot) { return (1.0 - std::exp(-3.0 * pot)) / 0.950213; }

// small-signal |H| at f: a steady sine (lock-in over whole cycles after the resonance has settled); robust to the
// solvers' noise floor, which an impulse response picks up
template <class F, class Make> static double gainAt(Make make, double pot, double fc, double fr)
{
  auto f = make(); f->setSampleRate(SR); f->setCutoff(fc); f->setResonance(resFor<F>(pot)); f->reset();
  const double amp = probeAmp;
  const long settle = (long)(0.08 * SR), cycles = std::max(4L, (long)(0.04 * fr));
  const long len = (long)std::lround(cycles * SR / fr);
  std::complex<double> acc = 0.0;
  for (long n = 0; n < settle + len; ++n)
  {
    const double ph = 2.0 * M_PI * fr * n / SR, y = f->getSample(amp * std::sin(ph));
    if (n >= settle) acc += y * std::polar(1.0, -ph);
  }
  return 2.0 * std::abs(acc) / len / amp;
}

template <class F, class Make> static double peakDb(Make make, double pot, double fc)
{
  const double pass = gainAt<F>(make, pot, fc, 0.1 * fc);
  double best = 0.0, bx = 1.0;
  for (double x = 0.5; x <= 1.6; x *= 1.03) { const double g = gainAt<F>(make, pot, fc, fc * x); if (g > best) { best = g; bx = x; } }
  for (double x = bx / 1.03; x <= bx * 1.03; x *= 1.003) best = std::max(best, gainAt<F>(make, pot, fc, fc * x));
  return 20.0 * std::log10(std::max(best, pass) / pass);
}

template <class F> static bool oscillates(F& f)
{
  double early = 0.0, late = 0.0; const long N = (long)(1.6 * SR);
  for (long n = 0; n < N; ++n)
  {
    const double y = f.getSample(n < 20 ? 0.5 : 0.0);
    if (n > 0.2 * SR && n < 0.3 * SR) early = std::max(early, std::fabs(y));
    if (n > 1.5 * SR) late = std::max(late, std::fabs(y));
  }
  return late > 1e-3 && late > 0.3 * early;
}

