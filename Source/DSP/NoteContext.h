#pragma once

#include <cstdint>

namespace dali::dsp
{
/**
    Musical context of a note — the input to Context-Aware Synthesis.
    Built by the engine from the incoming event plus its own memory of what
    happened before. Everything here is deterministic.
*/
struct NoteContext
{
    int      note       = 36;
    float    velocity   = 0.8f;
    bool     accent     = false;
    bool     slide      = false;   // this note ARRIVES by sliding (legato)
    float    gateSteps  = 0.5f;    // expected gate length in 16th steps (sequencer), last known for MIDI
    int      prevNote   = -1;
    bool     prevAccent = false;
    bool     prevSlide  = false;   // previous note arrived by slide
    int      accentRun  = 0;       // consecutive accented notes including this one
    int      stepIndex  = -1;      // position in the 16-step grid (pattern or host bar), -1 unknown
    uint32_t positionKey = 0;      // absolute 16th-note position (identity for deterministic variation)
};
} // namespace dali::dsp
