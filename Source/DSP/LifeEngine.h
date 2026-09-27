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

    LIFE = 0     : classic, stable behaviour (only ~0.25 cent of slow analog drift).
    LIFE 0..50%  : subtle — a player's feel, the line breathes a little.
    LIFE 50..100%: clearly audible — the circuit plays with you.

    Four layers fade in together, all scaled by strength (LIFE through a curve):
      1. Per-note character — hashed from (position, note, flags). Same pattern,
                              same preset, same song position => same result. Always.
      2. Context rules      — musical reactions to what came before
                              (after an accent the circuit "exhales", a slid-into
                              note blooms, accent runs build, jumps open the filter).
      3. Phrase arc         — a slow "hand on the cutoff": the filter and envelope
                              swell and settle over a 4-bar cycle, locked to the song
                              position.
      4. Circuit heat       — a slow state charged by notes/accents that pushes
                              saturation, drive and resonance; cools in ~2 s.
    Heat and drift are reset on transport start so playback is repeatable.
*/
class LifeEngine
{
public:
    /** LIFE knob -> effect strength. Linear-ish up to ~40 %, then rises to 2.8x at 100 %. */
    static float strength (float life) noexcept
    {
        const float l = clampf (life, 0.0f, 1.0f);
        const float t = clampf ((l - 0.35f) / 0.65f, 0.0f, 1.0f);
        const float s = t * t * (3.0f - 2.0f * t);
        return l * (1.0f + 1.8f * s);
    }

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
        heat = std::min (1.5f, heat + (c.accent ? 0.35f : 0.07f) + (c.slide ? 0.05f : 0.0f));

        NoteMods m;
        if (life <= 0.0f)
            return m;

        const float L = strength (life);

        // 1. deterministic per-note character
        const uint32_t h = hashCombine (c.positionKey * 2654435761U + (uint32_t) (c.stepIndex + 1),
                                        (uint32_t) c.note * 131U + (c.accent ? 7U : 0U) + (c.slide ? 13U : 0U));
        m.cutoffOct  = L * 0.17f  * hashBipolar (h + 1);          // up to ~ +/-0.5 oct
        m.envScale   = 1.0f + L * 0.13f * hashBipolar (h + 2);    // up to ~ +/-36 %
        m.decayScale = 1.0f + L * 0.17f * hashBipolar (h + 3);    // up to ~ +/-48 %
        m.pitchCents = life * 2.5f * hashBipolar (h + 4);         // pitch stays gentle: detune is not "alive"
        m.resOffset  = L * 0.03f  * hashBipolar (h + 5);

        // 2. context rules
        if (c.prevAccent && ! c.accent)  { m.cutoffOct -= 0.14f * L; m.decayScale *= 1.0f - 0.10f * L; } // exhale after accent
        if (c.slide)                     { m.resOffset += 0.05f * L; m.driveOffset += 0.08f * L; }        // slide arrival bloom
        if (c.prevSlide && ! c.slide)    { m.envScale *= 1.0f + 0.08f * L; }                              // re-attack after a slide phrase
        if (c.prevNote >= 0)             { m.cutoffOct += 0.10f * L * clampf ((float) (c.note - c.prevNote) / 12.0f, -1.0f, 1.0f); }
        if (c.stepIndex >= 0 && (c.stepIndex & 3) == 0) { m.envScale *= 1.0f + 0.08f * L; }                // downbeats speak more
        if (c.accentRun >= 2)            { m.accentScale = 1.0f + 0.08f * L * (float) std::min (c.accentRun - 1, 4); }
        if (c.gateSteps >= 0.0f && c.gateSteps < 0.5f) { m.decayScale *= 1.0f - 0.10f * L; }

        // 3. phrase arc: 64 sixteenths = 4 bars, locked to the song position
        const float ph = (float) (c.positionKey % 64u) / 64.0f;
        const float arc = 0.7f * std::sin (kTwoPi * ph - 1.2f) + 0.3f * std::sin (2.0f * kTwoPi * ph + 0.8f);
        m.cutoffOct += 0.12f * L * arc;                                         // up to ~ +/-0.34 oct over 4 bars
        m.envScale  *= 1.0f + 0.07f * L * arc;

        // 4. circuit heat
        const float h1 = std::min (heat, 1.0f);
        m.satBias      = 0.09f * L * h1;
        m.driveOffset += 0.07f * L * h1;
        m.resOffset   += 0.02f * L * h1;

        m.decayScale = std::max (0.4f, m.decayScale);
        m.envScale   = std::max (0.4f, m.envScale);
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
