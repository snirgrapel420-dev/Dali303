#pragma once

#include "DspCommon.h"

namespace dali::dsp
{
/**
    Analog-style exponential envelope (RC charge/discharge behaviour).
    - Attack charges towards an overshoot target (1.3) like a capacitor, so the
      rise is fast and slightly convex.
    - noteOn() restarts the attack FROM THE CURRENT VALUE: no clicks on retrigger.
*/
class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Release };

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        setAttack (attackS); setDecay (decayS); setRelease (releaseS);
        reset();
    }
    void reset() noexcept { value = 0.0f; stage = Stage::Idle; }

    void setAttack (float s) noexcept  { attackS = std::max (s, 1.0e-4f);  attackCoef  = tauCoef (attackS / 1.466f, fs); }
    void setDecay (float s) noexcept   { decayS = std::max (s, 1.0e-3f);   decayCoef   = tauCoef (decayS / 4.6f, fs); }
    void setRelease (float s) noexcept { releaseS = std::max (s, 1.0e-4f); releaseCoef = tauCoef (releaseS / 4.6f, fs); }
    void setSustain (float lvl) noexcept { sustain = clampf (lvl, 0.0f, 1.0f); }

    void noteOn() noexcept  { stage = Stage::Attack; }
    void noteOff() noexcept { if (stage != Stage::Idle) stage = Stage::Release; }

    bool  isIdle() const noexcept    { return stage == Stage::Idle; }
    Stage getStage() const noexcept  { return stage; }
    float getValue() const noexcept  { return value; }

    float process() noexcept
    {
        switch (stage)
        {
            case Stage::Attack:
                value = kAttackTarget + attackCoef * (value - kAttackTarget);
                if (value >= 1.0f) { value = 1.0f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
                value = sustain + decayCoef * (value - sustain);
                break;
            case Stage::Release:
                value *= releaseCoef;
                if (value < 1.0e-5f) { value = 0.0f; stage = Stage::Idle; }
                break;
            case Stage::Idle:
                break;
        }
        return value;
    }

private:
    static constexpr float kAttackTarget = 1.3f;
    double fs = 44100.0;
    float value = 0.0f, sustain = 0.0f;
    float attackS = 0.003f, decayS = 0.5f, releaseS = 0.01f;
    float attackCoef = 0.0f, decayCoef = 0.0f, releaseCoef = 0.0f;
    Stage stage = Stage::Idle;
};
} // namespace dali::dsp
