#pragma once

#include "DspCommon.h"

namespace dali::dsp
{
/**
    SlideEngine — RC-style glide in the pitch domain, plus motion information
    that the engine uses to make slides part of the acid sound:
      deviation() : how far the pitch still is from the target (semitones)
      motion()    : normalised glide speed (0..1)
      longness()  : 0 for short, snappy slides … 1 for long, liquid slides
    Short slides kick the resonance; long slides drag the filter along with
    the pitch (see AcidEngine), so they genuinely feel different.
*/
class SlideEngine
{
public:
    void prepare (double sampleRate) noexcept { fs = (float) sampleRate; setTime (time); }

    void setTime (float seconds) noexcept
    {
        time = std::max (0.002f, seconds);
        coef = tauCoef (time, fs);
        longness = clampf ((time - 0.02f) / 0.2f, 0.0f, 1.0f);
    }

    void jumpTo (float note) noexcept  { current = target = note; velocity = 0.0f; }
    void slideTo (float note) noexcept { target = note; }

    float process() noexcept
    {
        const float prev = current;
        current = target + coef * (current - target);
        if (std::abs (current - target) < 1.0e-4f) current = target;
        velocity = (current - prev) * fs;   // semitones per second
        return current;
    }

    float deviation() const noexcept   { return current - target; }
    float motion() const noexcept      { return clampf (std::abs (velocity) / 150.0f, 0.0f, 1.0f); }
    float getLongness() const noexcept { return longness; }

private:
    float fs = 44100.0f, time = 0.05f, coef = 0.0f, longness = 0.2f;
    float current = 36.0f, target = 36.0f, velocity = 0.0f;
};
} // namespace dali::dsp
