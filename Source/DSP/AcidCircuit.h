#pragma once

#include "AcidFilter.h"
#include "Oversampler.h"

namespace dali::dsp
{
/** Per-sample control frame for the circuit, computed at base rate by the engine. */
struct CircuitFrame
{
    float cutoffHz = 1000.0f;
    float k        = 0.0f;   // filter loop gain
    float inGain   = 0.4f;   // level into the filter's nonlinear node (DRIVE lives here first)
    float asym     = 0.02f;  // summing-node asymmetry
    float blend    = 0.25f;  // 3-pole tap blend
    float level    = 1.0f;   // loop-gain / drive level compensation
    float postGain = 1.0f, postBias = 0.0f, postTanhBias = 0.0f, postNorm = 1.0f;
    bool  postEnabled = true;
};

/**
    DALI ACID CIRCUIT — the nonlinear heart of the voice.

        osc ─► [ upsample ] ─► in-gain ─► AcidFilter (nonlinear loop) ─► level
                                            ─► asymmetric saturator ─► [ downsample ]

    DRIVE is not a distortion pedal at the end: it first raises the level into
    the filter's own nonlinear node (changing how resonance and input interact),
    then feeds a gentle asymmetric stage that shares the same oversampled domain.
    One knob therefore walks continuously Clean → Warm → Acid → Aggressive → Screaming.

    Only this block is oversampled. Cutoff and loop gain are interpolated across
    the sub-samples to avoid zipper steps at high oversampling ratios.
*/
class AcidCircuit
{
public:
    void prepare (double baseSampleRate)
    {
        baseRate = baseSampleRate;
        os.prepare();
        setOversampling (2);
    }

    /** Safe on the audio thread (no allocation). Resets filter state. */
    void setOversampling (int factor) noexcept
    {
        os.setFactor (factor);
        filter.prepare (baseRate * os.getFactor());
    }

    int getOversampling() const noexcept   { return os.getFactor(); }
    int getLatencySamples() const noexcept { return os.getLatencySamples(); }
    void reset() noexcept                  { os.reset(); filter.reset(); }

    float process (float x, const CircuitFrame& a, const CircuitFrame& b) noexcept
    {
        float buf[4];
        const int n = os.upsample (x, buf);
        const float invN = 1.0f / (float) n;

        for (int i = 0; i < n; ++i)
        {
            const float t  = (float) (i + 1) * invN;
            const float fc = lerpf (a.cutoffHz, b.cutoffHz, t);
            const float k  = lerpf (a.k, b.k, t);

            float y = filter.process (buf[i] * b.inGain, fc, k, b.asym, b.blend) * b.level;
            if (b.postEnabled)
                y = (std::tanh (b.postGain * y + b.postBias) - b.postTanhBias) * b.postNorm;
            buf[i] = y;
        }
        return os.downsample (buf);
    }

private:
    double baseRate = 44100.0;
    Oversampler os;
    AcidFilter filter;
};
} // namespace dali::dsp
