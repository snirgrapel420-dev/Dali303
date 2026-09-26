#pragma once

#include <cstddef>

namespace dali
{
/**
    Factory preset table. Plain data (no JUCE) so the offline test tool can
    render every factory preset and check levels / patterns.

    Knob values are the normalised 0..1 parameter values (tune in semitones,
    output in dB) — exactly what the plugin's parameters store.
*/
struct FactoryPreset
{
    const char* name;
    const char* category;
    float tune, cutoff, resonance, envMod, decay, accent, slide, drive, life, outputDb;
    int wave;               // 0 = saw, 1 = square
    const char* pattern;    // Pattern text format (see Pattern.h)
};

const FactoryPreset* getFactoryPresets() noexcept;
std::size_t getNumFactoryPresets() noexcept;

/** Category order used by the preset browser. */
const char* const* getPresetCategories() noexcept;
std::size_t getNumPresetCategories() noexcept;
} // namespace dali
