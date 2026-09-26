#pragma once

#include "DspCommon.h"
#include "NoteContext.h"

namespace dali::dsp
{
/** Per-note offsets produced by LIFE / context-aware synthesis. Neutral by default. */
struct NoteMods
{
    float cutoffOct   = 0.0f;
    float envScale    = 1.0f;
    float decayScale  = 1.0f;
    float resOffset   = 0.0f;
    float driveOffset = 0.0f;
    float pitchCents  = 0.0f;
    float satBias     = 0.0f;
    float accentScale = 1.0f;
};

/**
    LifeEngine — controlled, deterministic "aliveness".

    LIFE = 0   : classic, stable behaviour (only ~0.25 cent of slow analog drift).
    LIFE > 0   : three layers fade in together
      1. Per-note variation  — hashed from (position, note, flags). Same pattern,
                               same preset, same position ⇒ same result. Always.
      2. Context rules        — musical reactions to what came before
                               (after an accent the circuit "exhales", a slid-into
                               note blooms, accent runs build intensity, …).
      3. Circuit heat         — a slow state charged by notes/accents that pushes
                               saturation bias and drive; decays in ~2 s.
    Heat and drift are reset on transport start so playback is repeatable.
*/
class LifeEngine
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        heatCoef = tauCoef (1.8f, sampleRate);
        reset();
    }

    void reset() noexcept
    {
        p1 = 0.11f; p2 = 0.53f; p3 = 0.87f;
        drift = 0.0f; heat = 0.0f; counter = 0;
    }

    NoteMods onNote (const NoteContext& c, float life) noexcept
    {
        heat = std::min (1.5f, heat + (c.accent ? 0.35f : 0.06f));

        NoteMods m;
        if (life <= 0.0f)
            return m;

        // 1. deterministic per-note variation
        const uint32_t h = hashCombine (c.positionKey * 2654435761U + (uint32_t) (c.stepIndex + 1),
                                        (uint32_t) c.note * 131U + (c.accent ? 7U : 0U) + (c.slide ? 13U : 0U));
        m.cutoffOct  = life * 0.16f  * hashBipolar (h + 1);
        m.envScale   = 1.0f + life * 0.14f * hashBipolar (h + 2);
        m.decayScale = 1.0f + life * 0.20f * hashBipolar (h + 3);
        m.pitchCents = life * 2.0f   * hashBipolar (h + 4);
        m.resOffset  = life * 0.035f * hashBipolar (h + 5);

        // 2. context rules
        if (c.prevAccent && ! c.accent)            { m.cutoffOct -= 0.12f * life; m.decayScale *= 1.0f - 0.10f * life; } // exhale after accent
        if (c.slide)                               { m.resOffset += 0.05f * life; m.driveOffset += 0.07f * life; }       // slide arrival bloom
        if (c.prevSlide && ! c.slide)              { m.envScale *= 1.0f + 0.08f * life; }                               // re-attack after a slide phrase
        if (c.prevNote >= 0)                       { m.cutoffOct += 0.08f * life * clampf ((float) (c.note - c.prevNote) / 12.0f, -1.0f, 1.0f); }
        if (c.stepIndex >= 0 && (c.stepIndex & 3) == 0) { m.envScale *= 1.0f + 0.07f * life; }                         // downbeats speak a bit more
        if (c.accentRun >= 2)                      { m.accentScale = 1.0f + 0.08f * life * (float) std::min (c.accentRun - 1, 4); }
        if (c.gateSteps >= 0.0f && c.gateSteps < 0.5f) { m.decayScale *= 1.0f - 0.10f * life; }

        // 3. circuit heat
        const float h1 = std::min (heat, 1.0f);
        m.satBias      = 0.10f * life * h1;
        m.driveOffset += 0.06f * life * h1;
        return m;
    }

    void tick() noexcept
    {
        heat *= heatCoef;
        if (++counter >= kUpdateInterval)
        {
            counter = 0;
            const float dt = (float) kUpdateInterval / fs;
            p1 = wrap (p1 + 0.137f * dt);
            p2 = wrap (p2 + 0.291f * dt);
            p3 = wrap (p3 + 0.0613f * dt);
            drift = 0.5f * std::sin (kTwoPi * p1) + 0.3f * std::sin (kTwoPi * p2) + 0.2f * std::sin (kTwoPi * p3);
        }
    }

    float driftCents (float life) const noexcept { return drift * (0.25f + 4.0f * life); }
    float getHeat() const noexcept                { return heat; }

private:
    static float wrap (float p) noexcept { return p >= 1.0f ? p - 1.0f : p; }
    static constexpr int kUpdateInterval = 16;

    float fs = 44100.0f, heatCoef = 0.0f;
    float p1 = 0.0f, p2 = 0.0f, p3 = 0.0f, drift = 0.0f, heat = 0.0f;
    int counter = 0;
};
} // namespace dali::dsp
