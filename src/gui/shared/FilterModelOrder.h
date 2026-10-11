#pragma once

// The order the UI steps through the "filterType" choices. The parameter's choice indices stay fixed (presets and
// sessions store them, and they map onto rosic::FilterType); Diode Native takes Diode Octave's place in the sequence
// and Diode Octave is left out. A preset that still selects Diode Octave loads it, and stepping from it continues as
// if it were Diode Native.
namespace FilterModelOrder
{
    // choice indices: 0 TeeBee, 1 Diode Octave, 2 Diode, 3 Diode BP, 4 Diode HP, 5 Diode Native
    constexpr int order[] = { 0, 5, 2, 3, 4 };
    constexpr int size = sizeof(order) / sizeof(order[0]);

    constexpr int positionOf(int index)
    {
        for (int i = 0; i < size; ++i)
            if (order[i] == index)
                return i;
        return index == 1 ? 1 : 0;   // Diode Octave sits where Diode Native is
    }

    // The choice index delta steps away from index; clamped at the ends, or wrapping around.
    constexpr int step(int index, int delta, bool wrap)
    {
        int pos = positionOf(index) + delta;
        if (wrap)
            pos = ((pos % size) + size) % size;
        else
            pos = pos < 0 ? 0 : (pos >= size ? size - 1 : pos);
        return order[pos];
    }

    // The choice index at a 0..1 position along the sequence (value knobs), and back.
    constexpr int fromFraction(float fraction)
    {
        int pos = (int) (fraction * (size - 1) + 0.5f);
        return order[pos < 0 ? 0 : (pos >= size ? size - 1 : pos)];
    }

    constexpr float toFraction(int index) { return (float) positionOf(index) / (float) (size - 1); }

    static_assert(step(0, 1, false) == 5 && step(5, 1, false) == 2 && step(4, 1, false) == 4, "forward, clamped");
    static_assert(step(0, -1, false) == 0 && step(2, -1, false) == 5, "backward, clamped");
    static_assert(step(4, 1, true) == 0 && step(0, -1, true) == 4, "wrapping");
    static_assert(step(1, 1, false) == 2 && step(1, -1, false) == 0, "Diode Octave steps like Diode Native");
    static_assert(fromFraction(0.0f) == 0 && fromFraction(0.25f) == 5 && fromFraction(1.0f) == 4, "knob positions");
    static_assert(fromFraction(toFraction(3)) == 3, "round trip");
}
