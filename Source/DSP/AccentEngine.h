#pragma once

#include "Envelope.h"

namespace dali::dsp
{
/**
    AccentEngine — accent as a circuit, not a volume switch.

    An accented note fires a short envelope into an "accent sweep" capacitor
    (asymmetric charge / discharge). Resonance loads that capacitor:
      low resonance  -> fast, snappy accent bumps;
      high resonance -> slower charge, longer discharge, so consecutive accents
                        STACK and the cutoff climbs — the machine reacts to the
                        phrase, not only to the single note.
    The engine then routes the sweep into cutoff, resonance, drive, saturation
    bias and VCA (see AcidEngine).
*/
class AccentEngine
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        env.prepare (sampleRate);
        env.setAttack (0.0012f);
        env.setDecay (0.20f);
        env.setSustain (0.0f);
        setResonance (0.5f);
        reset();
    }

    void reset() noexcept { env.reset(); sweep = 0.0f; }

    void setResonance (float res) noexcept
    {
        res = clampf (res, 0.0f, 1.0f);
        chargeCoef    = tauCoef (lerpf (0.002f, 0.030f, res), fs);
        dischargeCoef = tauCoef (lerpf (0.090f, 0.420f, res), fs);
        norm = 1.0f + 0.9f * res;
    }

    void trigger (float decaySeconds) noexcept
    {
        env.setDecay (decaySeconds);
        env.noteOn();
    }

    /** Returns the current sweep voltage (~0..1.5). */
    float process() noexcept
    {
        const float e = env.process();
        const float c = e > sweep ? chargeCoef : dischargeCoef;
        sweep = e + c * (sweep - e);
        return sweep * norm;
    }

private:
    double fs = 44100.0;
    Envelope env;
    float sweep = 0.0f, chargeCoef = 0.0f, dischargeCoef = 0.0f, norm = 1.0f;
};
} // namespace dali::dsp
