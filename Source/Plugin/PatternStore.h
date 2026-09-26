#pragma once

#include "Sequencer/Pattern.h"
#include <array>
#include <atomic>

namespace dali
{
/**
    Lock-free hand-off of the sequencer pattern between GUI / preset code
    (message thread) and the audio thread. Each step is one atomic word; the
    audio thread copies the whole pattern at the start of every block.
    A version counter lets the GUI notice changes made elsewhere (presets,
    DISCOVER, host state restore).
*/
class PatternStore
{
public:
    PatternStore() { write (Pattern {}); }

    void write (const Pattern& p) noexcept
    {
        for (int i = 0; i < Pattern::kMaxSteps; ++i)
            steps[(size_t) i].store (packStep (p.steps[(size_t) i]), std::memory_order_relaxed);
        length.store (p.length, std::memory_order_relaxed);
        version.fetch_add (1, std::memory_order_release);
    }

    Pattern read() const noexcept
    {
        (void) version.load (std::memory_order_acquire);
        Pattern p;
        for (int i = 0; i < Pattern::kMaxSteps; ++i)
            p.steps[(size_t) i] = unpackStep (steps[(size_t) i].load (std::memory_order_relaxed));
        p.length = length.load (std::memory_order_relaxed);
        return p;
    }

    uint32_t getVersion() const noexcept { return version.load (std::memory_order_acquire); }

private:
    std::array<std::atomic<uint32_t>, Pattern::kMaxSteps> steps {};
    std::atomic<int> length { Pattern::kMaxSteps };
    std::atomic<uint32_t> version { 0 };
};
} // namespace dali
