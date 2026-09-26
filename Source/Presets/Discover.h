#pragma once

#include "Sequencer/Pattern.h"
#include <cstdint>

namespace dali
{
/**
    DISCOVER — musical, deterministic evolution of a sound + pattern.

    Not a randomiser: each press picks one of five DIRECTIONS and moves the
    sound a bounded distance that way, then mutates a few steps of the
    pattern while keeping its root note, its scale (pitch-class set) and its
    groove density. Everything is derived from a hash seed, so the same
    starting point and the same press count always give the same result.

    Plain data, no JUCE: tested offline by DaliRender.
*/
struct SoundValues
{
    float tune = 0, cutoff = .33f, resonance = .72f, envMod = .55f, decay = .38f,
          accent = .65f, slide = .35f, drive = .25f, life = .25f, outputDb = -3.0f;
    int wave = 0;

    uint32_t hash() const noexcept;
};

enum class DiscoverDirection { Deeper, Screamier, Hypnotic, Aggressive, Liquid, kCount };
const char* discoverDirectionName (DiscoverDirection d) noexcept;

struct DiscoverResult
{
    DiscoverDirection direction;
    SoundValues sound;
    Pattern pattern;
};

DiscoverResult discover (const SoundValues& sound, const Pattern& pattern, uint32_t seed) noexcept;

/** Pattern-only mutation (exposed for testing). */
Pattern mutatePattern (const Pattern& in, DiscoverDirection dir, uint32_t seed) noexcept;
} // namespace dali
