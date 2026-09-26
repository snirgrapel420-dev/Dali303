#pragma once

#include "DspCommon.h"
#include <vector>

namespace dali::dsp
{
/**
    Linear-phase half-band FIR (Kaiser-windowed), polyphase implementation.
    Only every second coefficient is non-zero, and the odd phase is a pure delay,
    so a 2x up- or down-sample costs ~N/2 multiply-adds.
    All memory is allocated in design() (prepare time), never on the audio thread.
*/
class HalfbandFIR
{
public:
    /** numTaps must be 4m + 3. */
    void design (int numTaps, double kaiserBeta)
    {
        N = numTaps;
        c = (N - 1) / 2;
        L = (N + 1) / 2;
        centreDelay = (c - 1) / 2;

        taps.assign ((size_t) L, 0.0f);
        const double i0b = besselI0 (kaiserBeta);
        double sum = 0.0;
        for (int i = 0; i < L; ++i)
        {
            const int j = 2 * i;
            const int m = j - c;                                    // always odd
            const double sinc = std::sin (kPiD * m / 2.0) / (kPiD * m);
            const double r = 2.0 * j / (N - 1) - 1.0;
            const double w = besselI0 (kaiserBeta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
            taps[(size_t) i] = (float) (sinc * w);
            sum += sinc * w;
        }
        for (auto& t : taps) t = (float) (t * 0.5 / sum);          // polyphase DC gain 0.5 (+ centre tap 0.5)

        bufA.assign ((size_t) (2 * L), 0.0f);
        bufB.assign ((size_t) (2 * L), 0.0f);
        reset();
    }

    void reset() noexcept
    {
        std::fill (bufA.begin(), bufA.end(), 0.0f);
        std::fill (bufB.begin(), bufB.end(), 0.0f);
        pos = 0;
    }

    /** 1 sample in -> 2 samples out (at twice the rate). */
    void upsample (float x, float& out0, float& out1) noexcept
    {
        pos = (pos + 1 == L) ? 0 : pos + 1;
        bufA[(size_t) pos] = bufA[(size_t) (pos + L)] = x;
        const float* p = bufA.data() + pos + L;
        float acc = 0.0f;
        for (int i = 0; i < L; ++i) acc += taps[(size_t) i] * p[-i];
        out0 = 2.0f * acc;
        out1 = p[-centreDelay];
    }

    /** 2 samples in -> 1 sample out (at half the rate). */
    float downsample (float in0, float in1) noexcept
    {
        pos = (pos + 1 == L) ? 0 : pos + 1;
        bufA[(size_t) pos] = bufA[(size_t) (pos + L)] = in0;
        bufB[(size_t) pos] = bufB[(size_t) (pos + L)] = in1;
        const float* e = bufA.data() + pos + L;
        const float* o = bufB.data() + pos + L;
        float acc = 0.0f;
        for (int i = 0; i < L; ++i) acc += taps[(size_t) i] * o[-i];
        return acc + 0.5f * e[-centreDelay];
    }

    int getGroupDelayHighRate() const noexcept { return c; }

private:
    static double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 64; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
            if (term < 1.0e-12 * sum) break;
        }
        return sum;
    }

    int N = 3, c = 1, L = 2, centreDelay = 0, pos = 0;
    std::vector<float> taps, bufA, bufB;
};

/**
    1x / 2x / 4x oversampler used ONLY around the nonlinear part of the voice
    (filter loop + saturation). Oscillator, envelopes and modulation run at the
    base rate — they are already band-limited or sub-audio.
    Stage 1 (base <-> 2x): 79 taps, ~85 dB stopband, passband to ~0.40 * fs.
    Stage 2 (2x <-> 4x):   23 taps, ~70 dB stopband (wide transition is fine there).
*/
class Oversampler
{
public:
    void prepare()
    {
        up1.design (79, 8.4); dn1.design (79, 8.4);
        up2.design (23, 6.8); dn2.design (23, 6.8);
        reset();
    }

    void setFactor (int f) noexcept { factor = (f >= 4) ? 4 : (f >= 2 ? 2 : 1); reset(); }
    int  getFactor() const noexcept { return factor; }

    void reset() noexcept { up1.reset(); dn1.reset(); up2.reset(); dn2.reset(); }

    /** Latency in base-rate samples (rounded). */
    int getLatencySamples() const noexcept
    {
        if (factor == 1) return 0;
        const double s1 = up1.getGroupDelayHighRate();              // 2 x c1 at 2x rate = c1 base samples
        const double s2 = factor == 4 ? up2.getGroupDelayHighRate() * 0.5 : 0.0;
        return (int) std::lround (s1 + s2);
    }

    int upsample (float x, float* out) noexcept
    {
        if (factor == 1) { out[0] = x; return 1; }
        float a, b;
        up1.upsample (x, a, b);
        if (factor == 2) { out[0] = a; out[1] = b; return 2; }
        up2.upsample (a, out[0], out[1]);
        up2.upsample (b, out[2], out[3]);
        return 4;
    }

    float downsample (const float* in) noexcept
    {
        if (factor == 1) return in[0];
        if (factor == 2) return dn1.downsample (in[0], in[1]);
        const float a = dn2.downsample (in[0], in[1]);
        const float b = dn2.downsample (in[2], in[3]);
        return dn1.downsample (a, b);
    }

private:
    int factor = 2;
    HalfbandFIR up1, dn1, up2, dn2;
};
} // namespace dali::dsp
