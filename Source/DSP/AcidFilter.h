#pragma once

#include "DspCommon.h"

namespace dali::dsp
{
/**
    AcidFilter — nonlinear 4-pole zero-delay-feedback lowpass.

    Topology: four TPT (trapezoidal) one-pole stages in a loop, with the whole
    loop solved implicitly each sample (Newton-Raphson, warm-started), so the
    resonance stays tuned and stable right up to self-oscillation.

    The nonlinearity sits at the loop's summing node:
        u = T( x - k * HP(y4) ),     T(z) = tanh(z + a) - tanh(a)
    This single node is what makes the filter behave like a circuit instead of
    an equaliser:
      * input level (DRIVE) and resonance fight for the same headroom -> hard
        input squashes the resonance, loud resonance compresses the input;
      * self-oscillation amplitude is bounded by the curve, not by clipping;
      * the asymmetry 'a' adds even harmonics that grow with drive;
      * a highpass in the feedback path (~45 Hz) keeps the bass from being
        cancelled by the loop and thins resonance at very low cutoffs — a trait
        of classic acid filters.
    Output taps: 4-pole plus an adjustable blend of the 3rd stage, which softens
    the slope toward the "not quite 24 dB" feel of acid-style ladders.

    Runs at the OVERSAMPLED rate (see AcidCircuit).
*/
class AcidFilter
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        piOverFs = kPi / fs;
        maxCutoff = std::min (0.40f * fs, 24000.0f);
        fbHpAlpha = onePoleAlpha (kFeedbackHpHz, sampleRate);
        reset();
    }

    void reset() noexcept { s[0] = s[1] = s[2] = s[3] = 0.0f; fbLow = 0.0f; y4 = 0.0f; }

    /** @param x       input (already scaled by the circuit's input gain)
        @param cutoffHz cutoff frequency
        @param k       loop gain; ~4 = self-oscillation threshold
        @param asym    summing-node bias (even harmonics)
        @param blend   0 = pure 4-pole output, 1 = 3rd-stage output            */
    float process (float x, float cutoffHz, float k, float asym, float blend) noexcept
    {
        const float fc = clampf (cutoffHz, 8.0f, maxCutoff);
        const float g  = std::tan (fc * piOverFs);
        const float G  = g / (1.0f + g);
        const float H  = 1.0f - G;
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const float S  = H * (G3 * s[0] + G2 * s[1] + G * s[2] + s[3]);
        const float ta = std::tanh (asym);

        // Solve  y = G4 * T(x - k*(y - fbLow)) + S   for the loop output y.
        float y = y4;
        for (int it = 0; it < 2; ++it)
        {
            const float th = std::tanh (x - k * (y - fbLow) + asym);
            const float f  = y - G4 * (th - ta) - S;
            const float fp = 1.0f + G4 * k * (1.0f - th * th);
            y -= f / fp;
        }
        const float u = std::tanh (x - k * (y - fbLow) + asym) - ta;

        float v;
        v = (u  - s[0]) * G; const float y1 = v + s[0]; s[0] = y1 + v;
        v = (y1 - s[1]) * G; const float y2 = v + s[1]; s[1] = y2 + v;
        v = (y2 - s[2]) * G; const float y3 = v + s[2]; s[2] = y3 + v;
        v = (y3 - s[3]) * G; y4 = v + s[3];            s[3] = y4 + v;

        fbLow += fbHpAlpha * (y4 - fbLow);
        return y4 + blend * (y3 - y4);
    }

private:
    static constexpr float kFeedbackHpHz = 45.0f;
    float fs = 88200.0f, piOverFs = 0.0f, maxCutoff = 20000.0f, fbHpAlpha = 0.0f;
    float s[4] = {}, fbLow = 0.0f, y4 = 0.0f;
};
} // namespace dali::dsp
