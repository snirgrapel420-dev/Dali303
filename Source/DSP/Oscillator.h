#pragma once

#include "DspCommon.h"

namespace dali::dsp
{
/**
    AcidOscillator — band-limited VCO with hardware-like character.

    - Free-running phase: like an analog VCO it never resets on note-on, so every
      note starts at a slightly different point of the cycle (phase behaviour).
    - Saw: gently curved ramp (harmonic asymmetry). The curve term is identical on
      both sides of the wrap, so the discontinuity stays exactly 2 and PolyBLEP
      remains correct.
    - Square: derived pulse with slightly off-centre duty, drooping tops and
      softened edges — it is "made from" the ramp, not a perfect digital square.
    - Pitch instability is NOT generated here. It is injected as deterministic
      pitch modulation by LifeEngine, so it stays controllable and repeatable.
*/
class AcidOscillator
{
public:
    enum class Wave { Saw = 0, Square = 1 };

    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        mix.prepare (sampleRate, 0.006f);
        mix.reset (wave == Wave::Square ? 1.0f : 0.0f);
        edgeAlpha = onePoleAlpha (std::min (13500.0f, 0.4f * fs), sampleRate);
        reset();
    }

    void reset() noexcept { phase = 0.2371f; edge = 0.0f; }

    void setWave (Wave w) noexcept   { wave = w; mix.setTarget (w == Wave::Square ? 1.0f : 0.0f); }
    void setCurve (float c) noexcept { curve = clampf (c, 0.0f, 0.3f); }

    float process (float hz) noexcept
    {
        const float dt = clampf (hz / fs, 1.0e-7f, 0.45f);
        phase += dt;
        if (phase >= 1.0f) phase -= 1.0f;
        const float t = phase;

        // --- curved saw ---
        float saw = 2.0f * t - 1.0f;
        saw += curve * (saw * saw - (1.0f / 3.0f));
        saw -= polyBlep (t, dt);

        // --- derived square ---
        constexpr float pw = 0.485f;
        float sq = t < pw ? 1.0f : -1.0f;
        sq += polyBlep (t, dt);
        float t2 = t - pw;
        if (t2 < 0.0f) t2 += 1.0f;
        sq -= polyBlep (t2, dt);
        sq += 0.10f * saw;                  // drooping tops
        edge += edgeAlpha * (sq - edge);    // softened edges

        const float m = mix.next();
        return saw + m * (0.92f * edge - saw);
    }

private:
    static float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)         { t /= dt;          return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt)  { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    float fs = 44100.0f, phase = 0.0f, edge = 0.0f, edgeAlpha = 0.5f, curve = 0.06f;
    Wave wave = Wave::Saw;
    Smoother mix;
};
} // namespace dali::dsp
