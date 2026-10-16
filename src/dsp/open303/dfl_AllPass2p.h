// dfl_AllPass2p.h
// Second-order all-pass filter
//
// Extracted from pedalevite by Laurent de Soras (WTFPL license)
// Adapted for superfreak by DFL

#ifndef dfl_AllPass2p_h
#define dfl_AllPass2p_h

#include <array>
#include <cassert>

namespace dfl
{

/**
 * Second-order all-pass filter.
 *
 * Transfer function:
 *              -1       -2
 *     b0 + b1.z   +    z
 *     ----------------------
 *              -1       -2
 *     1  + b1.z   + b0.z
 *
 * Time equation:
 *   y[t] = b0*x[t] + b1*x[t-1] + x[t-2] - b1*y[t-1] - b0*y[t-2]
 */
class AllPass2p
{
public:
    AllPass2p()
    {
        _b = {{ 0.0f, 0.0f }};
        _memX = {{ 0.0f, 0.0f }};
        _memY = {{ 0.0f, 0.0f }};
        _memPos = 0;
    }

    /**
     * Set the z-domain coefficients.
     * @param b0 Coefficient for z^-2 term
     * @param b1 Coefficient for z^-1 term
     */
    inline void setCoef(float b0, float b1) noexcept
    {
        _b[0] = b0;
        _b[1] = b1;
    }

    inline void getCoef(float& b0, float& b1) const noexcept
    {
        b0 = _b[0];
        b1 = _b[1];
    }

    inline void copyCoefFrom(const AllPass2p& other) noexcept
    {
        _b[0] = other._b[0];
        _b[1] = other._b[1];
    }

    /**
     * Process a single sample.
     */
    inline float process(float x) noexcept
    {
        const int altPos = 1 - _memPos;
        const float y = _b[0] * (x             - _memY[altPos])
                      + _b[1] * (_memX[_memPos] - _memY[_memPos])
                      +          _memX[altPos];

        _memX[altPos] = x;
        _memY[altPos] = y;
        _memPos       = altPos;

        return y;
    }

    /**
     * Process a single sample with coefficient increments.
     */
    inline float process(float x, float incB0, float incB1) noexcept
    {
        const float y = process(x);
        _b[0] += incB0;
        _b[1] += incB1;
        return y;
    }

    void processBlock(float* dst, const float* src, int numSamples) noexcept
    {
        assert(dst != nullptr);
        assert(src != nullptr);
        assert(numSamples > 0);

        for (int i = 0; i < numSamples; ++i)
        {
            dst[i] = process(src[i]);
        }
    }

    void clearBuffers() noexcept
    {
        _memX = {{ 0.0f, 0.0f }};
        _memY = {{ 0.0f, 0.0f }};
        _memPos = 0;
    }

private:
    std::array<float, 2> _b;       // Coefficients [b0, b1]
    std::array<float, 2> _memX;    // Input ring buffer
    std::array<float, 2> _memY;    // Output ring buffer
    int _memPos;                   // Write position (0 or 1)
};

} // namespace dfl

#endif // dfl_AllPass2p_h
