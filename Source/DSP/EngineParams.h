#pragma once

#include <cmath>

namespace dali::dsp
{
/** Developer / DSP Test Mode: isolate parts of the engine to find what is responsible for a sound. */
enum class DevMode : int
{
    Full = 0,        // normal engine
    OscillatorOnly,  // raw oscillator through the VCA (no filter / circuit)
    FilterClean,     // filter with tiny input level, no post saturation (≈ linear filter response)
    NoDrive,         // drive forced to 0, post saturation bypassed
    NoAccent,        // accent ignored
    NoSlide,         // slides become hard note changes
    NoLife           // LIFE forced to 0
};
constexpr int kNumDevModes = 7;

inline const char* devModeName (DevMode m) noexcept
{
    switch (m)
    {
        case DevMode::Full:           return "Full engine";
        case DevMode::OscillatorOnly: return "Oscillator only";
        case DevMode::FilterClean:    return "Filter (clean)";
        case DevMode::NoDrive:        return "No drive";
        case DevMode::NoAccent:       return "No accent";
        case DevMode::NoSlide:        return "No slide";
        case DevMode::NoLife:         return "No LIFE";
    }
    return "?";
}

/** Front-panel state, set once per audio block. All knob values normalised 0..1 unless noted. */
struct EngineParams
{
    float tune      = 0.0f;   // semitones, -12..+12
    float cutoff    = 0.33f;
    float resonance = 0.72f;
    float envMod    = 0.55f;
    float decay     = 0.38f;
    float accent    = 0.65f;
    float slide     = 0.35f;
    float drive     = 0.25f;
    float life      = 0.25f;
    float outputDb  = -3.0f;  // dB
    int   wave      = 0;      // 0 = saw, 1 = square
    int   oversampling = 2;   // requested factor: 1, 2 or 4
};

/** Realtime performance controllers (MIDI). */
struct PerformanceControls
{
    float pitchBend   = 0.0f; // semitones
    float modWheel    = 0.0f; // 0..1  -> opens the filter
    float ccCutoff    = 0.0f; // -1..1 (CC74, centred)
    float ccResonance = 0.0f; // -1..1 (CC71, centred)
};

/** Knob -> physical mappings, shared by engine and UI text display. */
namespace mapping
{
    inline float cutoffHz (float v) noexcept     { return 40.0f * std::exp2 (v * 8.2f); }       // 40 Hz .. ~11.8 kHz (before env / accent)
    inline float decaySeconds (float v) noexcept { return 0.03f * std::pow (100.0f, v); }       // 30 ms .. 3 s
    inline float slideSeconds (float v) noexcept { return 0.012f * std::pow (37.5f, v); }       // 12 ms .. 450 ms (glide time constant)
}
} // namespace dali::dsp
