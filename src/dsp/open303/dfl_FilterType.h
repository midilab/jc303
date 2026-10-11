// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Lowenfels

#ifndef dfl_FilterType_h
#define dfl_FilterType_h

namespace dfl
{
  /** Filter types available for selection. The values are the "filterType" parameter's choice indices, which
      presets and sessions store. */
  enum FilterType
  {
    FILTER_TEEBEE = 0,      // Original TB-303 transistor ladder
    FILTER_DIODE,           // Diode ladder calibrated to a SPICE model of the real circuit
    FILTER_DIODE_BP,        // Diode, bandpass response
    FILTER_DIODE_HP,        // Diode, highpass response
    NUM_FILTER_TYPES
  };
}

#endif
