#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// ============================================================================
//  Dali303 DSP core — shared utilities.
//  Everything under Source/DSP is plain C++17 with NO JUCE dependency, so the
//  engine can be unit-tested / rendered offline (see Tools/DaliRender.cpp).
// ============================================================================
namespace dali::dsp
{
constexpr float  kPi    = 3.14159265358979323846f;
constexpr double kPiD   = 3.14159265358979323846;
constexpr float  kTwoPi = 2.0f * kPi;

inline float clampf (float x, float lo, float hi) noexcept { return std::min (hi, std::max (lo, x)); }
inline float lerpf (float a, float b, float t) noexcept     { return a + (b - a) * t; }
inline float midiToHz (float note) noexcept                  { return 440.0f * std::exp2 ((note - 69.0f) * (1.0f / 12.0f)); }
inline float dbToGain (float db) noexcept                    { return std::pow (10.0f, db * 0.05f); }

/** Time constant (seconds) -> coefficient for  y = target + c * (y - target). */
inline float tauCoef (float seconds, double sampleRate) noexcept
{
    if (seconds <= 0.0f) return 0.0f;
    return (float) std::exp (-1.0 / ((double) seconds * sampleRate));
}

/** Corner frequency -> increment for a one-pole lowpass  y += a * (x - y). */
inline float onePoleAlpha (float hz, double sampleRate) noexcept
{
    return 1.0f - (float) std::exp (-2.0 * kPiD * (double) hz / sampleRate);
}

// ----------------------------------------------------------------------------
//  Deterministic hashing. LIFE and DISCOVER never use time-seeded randomness:
//  same pattern + same preset + same position  ==> same result.
// ----------------------------------------------------------------------------
inline uint32_t hash32 (uint32_t x) noexcept
{
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
inline uint32_t hashCombine (uint32_t a, uint32_t b) noexcept
{
    return hash32 (a ^ (hash32 (b) + 0x9e3779b9U + (a << 6) + (a >> 2)));
}
inline float hashUnit (uint32_t x) noexcept     { return (float) (hash32 (x) >> 8) * (1.0f / 16777216.0f); }
inline float hashBipolar (uint32_t x) noexcept  { return hashUnit (x) * 2.0f - 1.0f; }

/** One-pole parameter smoother (exponential glide to target). */
class Smoother
{
public:
    void prepare (double sampleRate, float timeSeconds) noexcept { coef = tauCoef (timeSeconds, sampleRate); }
    void reset (float v) noexcept        { y = target = v; }
    void setTarget (float t) noexcept    { target = t; }
    float getTarget() const noexcept     { return target; }
    float next() noexcept                { y = target + coef * (y - target); return y; }
    float current() const noexcept       { return y; }

private:
    float coef = 0.0f, y = 0.0f, target = 0.0f;
};

/** Gentle DC blocker (~6 Hz). Needed after asymmetric saturation. */
class DcBlocker
{
public:
    void prepare (double sampleRate) noexcept { r = (float) std::exp (-2.0 * kPiD * 6.0 / sampleRate); reset(); }
    void reset() noexcept                     { x1 = y1 = 0.0f; }
    float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x; y1 = y;
        return y;
    }

private:
    float r = 0.999f, x1 = 0.0f, y1 = 0.0f;
};
} // namespace dali::dsp
