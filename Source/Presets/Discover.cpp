#include "Discover.h"
#include "DSP/DspCommon.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace dali
{
using dsp::hashCombine;
using dsp::hashUnit;
using dsp::hashBipolar;

uint32_t SoundValues::hash() const noexcept
{
    const float v[] = { tune, cutoff, resonance, envMod, decay, accent, slide, drive, life, outputDb, (float) wave };
    uint32_t h = 0x5eed303u;
    for (float f : v)
        h = hashCombine (h, (uint32_t) (int32_t) std::lround (f * 1000.0f));
    return h;
}

const char* discoverDirectionName (DiscoverDirection d) noexcept
{
    switch (d)
    {
        case DiscoverDirection::Deeper:     return "Deeper";
        case DiscoverDirection::Screamier:  return "Screamier";
        case DiscoverDirection::Hypnotic:   return "Hypnotic";
        case DiscoverDirection::Aggressive: return "Aggressive";
        case DiscoverDirection::Liquid:     return "Liquid";
        default: break;
    }
    return "";
}

namespace
{
    /** Move towards a target direction with a bounded, seed-shaped amount.
        The push fades out near the bound it is heading for, so repeated
        presses in one direction converge instead of pinning every knob. */
    float nudge (float v, float delta, float jitter, uint32_t seed, float lo = 0.0f, float hi = 1.0f) noexcept
    {
        const float room = delta > 0.0f ? (hi - v) : (v - lo);
        const float fade = std::min (1.0f, std::max (0.0f, room / ((hi - lo) * 0.35f)));
        const float amt = delta * fade * (0.6f + 0.8f * hashUnit (seed)) + jitter * hashBipolar (seed ^ 0xa5a5u);
        return std::min (hi, std::max (lo, v + amt));
    }

    int rootNote (const Pattern& p) noexcept
    {
        std::array<int, 128> count {};
        for (int i = 0; i < p.length; ++i)
            if (p.steps[(size_t) i].gate) ++count[p.steps[(size_t) i].note];
        int best = p.steps[0].note, bestCount = -1;
        for (int n = 0; n < 128; ++n)
            if (count[(size_t) n] > bestCount || (count[(size_t) n] == bestCount && n < best && count[(size_t) n] > 0))
            { best = n; bestCount = count[(size_t) n]; }
        return best;
    }

    /** Pitch classes present in the pattern, relative to the root. Always contains 0, 7, 12-friendly tones. */
    std::array<bool, 12> scaleOf (const Pattern& p, int root) noexcept
    {
        std::array<bool, 12> s {};
        for (int i = 0; i < p.length; ++i)
            if (p.steps[(size_t) i].gate) s[(size_t) (((p.steps[(size_t) i].note - root) % 12 + 12) % 12)] = true;
        s[0] = true;
        // If the pattern is very sparse harmonically, allow the minor-pentatonic
        // tones that fit any acid line (m3, 4th, 5th, m7) as extra choices.
        int used = 0; for (bool b : s) used += b ? 1 : 0;
        if (used < 4) { s[3] = true; s[5] = true; s[7] = true; s[10] = true; }
        return s;
    }

    int pickScaleNote (int root, const std::array<bool, 12>& scale, uint32_t seed, bool preferOctave) noexcept
    {
        if (preferOctave && hashUnit (seed ^ 0x77u) < 0.5f)
            return root + (hashUnit (seed ^ 0x99u) < 0.7f ? 12 : 0);

        int choices[24]; int n = 0;
        for (int off = 0; off < 19; ++off)                      // root .. root + a sixth above the octave
            if (scale[(size_t) (off % 12)]) choices[n++] = off;
        if (n == 0) return root;
        return root + choices[(size_t) (hashUnit (seed) * (float) n) % (size_t) n];
    }
}

Pattern mutatePattern (const Pattern& in, DiscoverDirection dir, uint32_t seed) noexcept
{
    Pattern p = in;
    const int len = std::max (1, p.length);
    const int root = rootNote (in);
    const auto scale = scaleOf (in, root);

    int gated = 0;
    for (int i = 0; i < len; ++i) gated += p.steps[(size_t) i].gate ? 1 : 0;

    // How many steps change: 2..4 — enough to feel new, few enough to keep identity.
    const int numEdits = 2 + (int) (hashUnit (seed ^ 0x1234u) * 3.0f);

    for (int e = 0; e < numEdits; ++e)
    {
        const uint32_t s = hashCombine (seed, (uint32_t) e * 7919u + 1u);
        const int i = (int) (hashUnit (s) * (float) len) % len;
        auto& st = p.steps[(size_t) i];
        const float r = hashUnit (s ^ 0xbeefu);

        switch (dir)
        {
            case DiscoverDirection::Deeper:      // fewer, lower notes; more space
                if (r < 0.45f)      st.note = (uint8_t) root;
                else if (r < 0.7f && gated > 9 && i % 4 != 0) { if (st.gate) { st.gate = false; --gated; } }
                else                st.slide = ! st.slide;
                break;

            case DiscoverDirection::Screamier:   // accents on high notes, octave jumps
                if (r < 0.5f)      { st.note = (uint8_t) (root + 12 + (root < 36 && hashUnit (s ^ 3u) < 0.25f ? 12 : 0)); st.accent = true; }
                else if (r < 0.8f)   st.accent = ! st.accent;
                else                 st.note = (uint8_t) pickScaleNote (root, scale, s, false);
                if (! st.gate) { st.gate = true; ++gated; }
                break;

            case DiscoverDirection::Hypnotic:    // repetition + slides: the root pulls harder
                if (r < 0.55f)      { st.note = (uint8_t) root; st.gate = true; }
                else if (r < 0.85f)   st.slide = true;
                else                  st.accent = ! st.accent;
                break;

            case DiscoverDirection::Aggressive:  // more accents, denser, shorter
                if (r < 0.5f)        st.accent = true;
                else if (r < 0.75f) { if (! st.gate) { st.gate = true; ++gated; } st.slide = false; }
                else                  st.note = (uint8_t) pickScaleNote (root, scale, s, true);
                break;

            case DiscoverDirection::Liquid:      // melodic: slides and scale movement
                if (r < 0.5f)        st.slide = true;
                else if (r < 0.85f)  st.note = (uint8_t) pickScaleNote (root, scale, s, false);
                else                 st.accent = false;
                if (! st.gate) { st.gate = true; ++gated; }
                break;

            default: break;
        }
        st.note = (uint8_t) std::min (64, std::max (24, (int) st.note));
    }

    // Keep the downbeat as an anchor: step 1 always plays.
    if (! p.steps[0].gate) p.steps[0].gate = true;
    return p;
}

DiscoverResult discover (const SoundValues& in, const Pattern& pattern, uint32_t seed) noexcept
{
    DiscoverResult r;
    r.direction = (DiscoverDirection) (int) (hashUnit (seed) * (float) DiscoverDirection::kCount);
    if ((int) r.direction >= (int) DiscoverDirection::kCount) r.direction = DiscoverDirection::Liquid;
    auto s = in;
    uint32_t k = hashCombine (seed, 0x303u);
    auto next = [&k] { k = hashCombine (k, 0x9e37u); return k; };

    switch (r.direction)
    {
        case DiscoverDirection::Deeper:
            s.cutoff    = nudge (s.cutoff,    -0.07f, 0.02f, next(), 0.08f, 0.9f);
            s.decay     = nudge (s.decay,      0.08f, 0.03f, next());
            s.resonance = nudge (s.resonance, -0.03f, 0.03f, next(), 0.35f, 0.97f);
            s.envMod    = nudge (s.envMod,    -0.05f, 0.03f, next(), 0.2f);
            s.life      = nudge (s.life,       0.05f, 0.03f, next());
            break;
        case DiscoverDirection::Screamier:
            s.resonance = nudge (s.resonance,  0.07f, 0.02f, next(), 0.35f, 0.97f);
            s.envMod    = nudge (s.envMod,     0.08f, 0.03f, next());
            s.accent    = nudge (s.accent,     0.08f, 0.03f, next());
            s.drive     = nudge (s.drive,      0.07f, 0.03f, next(), 0.0f, 0.9f);
            s.decay     = nudge (s.decay,     -0.04f, 0.03f, next(), 0.1f);
            break;
        case DiscoverDirection::Hypnotic:
            s.slide     = nudge (s.slide,      0.08f, 0.03f, next());
            s.decay     = nudge (s.decay,      0.05f, 0.04f, next());
            s.life      = nudge (s.life,       0.08f, 0.03f, next());
            s.cutoff    = nudge (s.cutoff,     0.00f, 0.04f, next(), 0.08f, 0.9f);
            break;
        case DiscoverDirection::Aggressive:
            s.drive     = nudge (s.drive,      0.10f, 0.03f, next(), 0.0f, 0.95f);
            s.decay     = nudge (s.decay,     -0.06f, 0.03f, next(), 0.08f);
            s.accent    = nudge (s.accent,     0.07f, 0.03f, next());
            s.envMod    = nudge (s.envMod,     0.05f, 0.03f, next());
            break;
        case DiscoverDirection::Liquid:
            s.slide     = nudge (s.slide,      0.10f, 0.03f, next());
            s.cutoff    = nudge (s.cutoff,     0.05f, 0.03f, next(), 0.08f, 0.9f);
            s.drive     = nudge (s.drive,     -0.06f, 0.03f, next());
            s.life      = nudge (s.life,       0.06f, 0.03f, next());
            s.decay     = nudge (s.decay,      0.05f, 0.03f, next());
            break;
        default: break;
    }

    // Loudness guard: the engine already level-compensates drive, but more
    // drive + resonance still feels louder — pull the output a little.
    const float heat = (s.drive - in.drive) * 6.0f + (s.resonance - in.resonance) * 3.0f;
    s.outputDb = std::min (6.0f, std::max (-24.0f, s.outputDb - std::max (-2.0f, std::min (2.0f, heat))));

    r.sound = s;
    r.pattern = mutatePattern (pattern, r.direction, hashCombine (seed, 0x5eedu));
    return r;
}
} // namespace dali
