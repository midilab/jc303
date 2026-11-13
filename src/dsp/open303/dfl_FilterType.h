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
    FILTER_DIODE,           // Diode ladder calibrated to a SPICE model of the real circuit, LP / BP / HP mode morph
    FILTER_MS20,            // Korg MS-20 Korg35 (early MS-20 / MS-10), solved from the circuit, LP / BP / HP injection morph
    FILTER_JUNO106,         // Roland Juno-106 IR3109 with BA662 resonance (80017A), solved from the circuit, LP / BP / HP stage-tap morph
    FILTER_WASP,            // EDP Wasp CMOS-inverter / CA3080 state-variable filter, solved from the circuit, LP / BP / HP output morph
    FILTER_WASP_A124,       // Doepfer A-124 Wasp (+12 V, R13 1M), solved from the circuit, LP / BP / HP output morph
    FILTER_SEM,             // Oberheim SEM CA3080 / JFET state-variable filter, solved from the circuit, LP / BP / HP output morph
    FILTER_ARP4075,         // ARP 4075 (Odyssey Mk III) four-pole cascade with its limiter, solved from the circuit, LP / BP / HP stage-tap morph
    NUM_FILTER_TYPES
  };
}

#endif
