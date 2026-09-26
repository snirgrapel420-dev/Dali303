#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace dali
{
/** One sequencer step. */
struct Step
{
    uint8_t note   = 36;    // MIDI note
    bool    gate   = true;  // false = rest
    bool    accent = false;
    bool    slide  = false; // hold into the next step and glide to it
};

/** A 16-step acid pattern. Plain data: safe to copy on the audio thread. */
struct Pattern
{
    static constexpr int kMaxSteps = 16;
    std::array<Step, kMaxSteps> steps {};
    int length = kMaxSteps;

    uint32_t hash() const noexcept;
};

// Lock-free packing for PatternStore
uint32_t packStep (const Step& s) noexcept;
Step     unpackStep (uint32_t v) noexcept;

// Human-readable text format, used by presets and state:
//   tokens separated by spaces:  NOTE[A][S]  or  NOTE R  (rest keeps its note)  or  "-"  (rest, inherits previous note)
//   NOTE = C, C#, Db, D ... B followed by octave (C2 = MIDI 36).  Example: "A1 A1S C2A - E2AS D2"
std::string patternToString (const Pattern& p);
bool        patternFromString (const std::string& text, Pattern& out);
std::string noteName (int midiNote);
} // namespace dali
