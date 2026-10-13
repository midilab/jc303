// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Lowenfels

#ifndef dfl_ResonanceMap_h
#define dfl_ResonanceMap_h

// Resonance knob -> circuit pot, per filter model, so the circuit-native models resonate alike: the same small-signal
// peak height above the passband at the same knob position (+6 dB at 0.45, +12 dB at 0.70, +20 dB at 0.82, +24 dB at
// 0.86) and self-oscillation from 0.90; the knob's last tenth maps linearly onto each circuit's pot from its onset to
// the end, so each keeps its own behaviour past the threshold. The Wasps and the SEM do not self-oscillate at 1 kHz:
// they follow the targets to +20 dB at 0.82, then the rest of their pot (EDP ~+26 dB, A-124 ~+38 dB, SEM ~+41 dB; the
// A-124 does oscillate at low cutoffs). Each filter still receives a plain pot rotation.
// TeeBee and Spicy are not mapped (their knob is the 303's). Breakpoints fitted at a 1 kHz cutoff (the laws hold to
// ~0.01 from 300 Hz to 3 kHz) by tools/resonance-map (measure, fit.py); checked by tools/resonance-map/check.

#include <algorithm>
#include <cmath>
#include "dfl_FilterType.h"

namespace dfl
{

  struct ResonanceMap
  {
    static constexpr int N = 11;
    double k[N], p[N];   // knob, pot (both 0..1, increasing)
  };

  // knob -> pot breakpoints (tools/resonance-map/fit.py)
  inline constexpr ResonanceMap RESONANCE_MAP_MS20     = { { 0.00, 0.15, 0.30, 0.45, 0.58, 0.70, 0.76, 0.82, 0.86, 0.90, 1.00 },
                                                            { 0.0000, 0.2239, 0.2993, 0.3846, 0.4668, 0.5566, 0.6155, 0.6695, 0.7037, 0.7271, 1.0000 } };
  inline constexpr ResonanceMap RESONANCE_MAP_JUNO106  = { { 0.00, 0.15, 0.30, 0.45, 0.58, 0.70, 0.76, 0.82, 0.86, 0.90, 1.00 },
                                                            { 0.0000, 0.2528, 0.3310, 0.4178, 0.5031, 0.6055, 0.6831, 0.7636, 0.8117, 0.8936, 1.0000 } };
  inline constexpr ResonanceMap RESONANCE_MAP_WASP     = { { 0.00, 0.15, 0.30, 0.45, 0.58, 0.70, 0.76, 0.82, 0.88, 0.94, 1.00 },
                                                            { 0.0000, 0.0119, 0.1355, 0.2954, 0.4746, 0.6934, 0.8326, 0.9391, 0.9594, 0.9797, 1.0000 } };
  // the SEM (the map measures LP, its MODE pot's start): its damper gives almost no peak over the pot's first 60 %
  inline constexpr ResonanceMap RESONANCE_MAP_SEM      = { { 0.00, 0.15, 0.30, 0.45, 0.58, 0.70, 0.76, 0.82, 0.88, 0.94, 1.00 },
                                                            { 0.0000, 0.5962, 0.7018, 0.7893, 0.8529, 0.9090, 0.9413, 0.9680, 0.9786, 0.9893, 1.0000 } };

  // the A-124: no self-oscillation at 1 kHz (it does at low cutoffs), ~+38 dB at full pot; its own Q already gives
  // ~+2.5 dB at pot 0, so the knob's first sixth stays at the pot's start
  inline constexpr ResonanceMap RESONANCE_MAP_WASP_A124 = { { 0.00, 0.15, 0.30, 0.45, 0.58, 0.70, 0.76, 0.82, 0.88, 0.94, 1.00 },
                                                            { 0.0000, 0.0000, 0.0667, 0.2071, 0.3702, 0.5884, 0.7450, 0.8745, 0.9163, 0.9582, 1.0000 } };
  // the ARP 4075 (Odyssey Mk III): no peak at pot 0 (its cascade), onset at pot 0.84 at 1 kHz
  inline constexpr ResonanceMap RESONANCE_MAP_ARP4075  = { { 0.00, 0.15, 0.30, 0.45, 0.58, 0.70, 0.76, 0.82, 0.86, 0.90, 1.00 },
                                                            { 0.0000, 0.2076, 0.3318, 0.4631, 0.5700, 0.6691, 0.7274, 0.7763, 0.8005, 0.8396, 1.0000 } };

  /** Monotone cubic (Fritsch-Carlson) through the map's breakpoints. */
  inline double evalResonanceMap(const ResonanceMap& m, double knob)
  {
    const double x = std::clamp(knob, 0.0, 1.0);
    int i = 0;
    while (i < ResonanceMap::N - 2 && x > m.k[i + 1]) ++i;
    auto secant = [&](int j) { return (m.p[j + 1] - m.p[j]) / (m.k[j + 1] - m.k[j]); };
    auto tangent = [&](int j)
    {
      if (j == 0) return secant(0);
      if (j == ResonanceMap::N - 1) return secant(ResonanceMap::N - 2);
      const double d0 = secant(j - 1), d1 = secant(j);
      if (d0 * d1 <= 0.0) return 0.0;
      const double h0 = m.k[j] - m.k[j - 1], h1 = m.k[j + 1] - m.k[j];
      const double w1 = 2.0 * h1 + h0, w2 = h1 + 2.0 * h0;
      return (w1 + w2) / (w1 / d0 + w2 / d1);
    };
    const double h = m.k[i + 1] - m.k[i], t = (x - m.k[i]) / h;
    const double t2 = t * t, t3 = t2 * t;
    return std::clamp((2 * t3 - 3 * t2 + 1) * m.p[i] + (t3 - 2 * t2 + t) * h * tangent(i)
                      + (-2 * t3 + 3 * t2) * m.p[i + 1] + (t3 - t2) * h * tangent(i + 1), 0.0, 1.0);
  }

  /** The circuit pot (0..1) for a resonance knob position (0..1) on the given model; unmapped models get the knob. */
  inline double resonancePot(FilterType type, double knob)
  {
    switch (type)
    {
      case FILTER_MS20:     return evalResonanceMap(RESONANCE_MAP_MS20, knob);
      case FILTER_JUNO106:  return evalResonanceMap(RESONANCE_MAP_JUNO106, knob);
      case FILTER_WASP:     return evalResonanceMap(RESONANCE_MAP_WASP, knob);
      case FILTER_WASP_A124: return evalResonanceMap(RESONANCE_MAP_WASP_A124, knob);
      case FILTER_ARP4075:  return evalResonanceMap(RESONANCE_MAP_ARP4075, knob);
      case FILTER_SEM_NOTCH: return evalResonanceMap(RESONANCE_MAP_SEM, knob);
      default:              return std::clamp(knob, 0.0, 1.0);
    }
  }

} // end namespace dfl

#endif // dfl_ResonanceMap_h
