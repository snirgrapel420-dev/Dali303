#pragma once

#include <array>

namespace dali
{
/** Fixed-size last-note-priority stack for monophonic MIDI playing (no allocation). */
class MonoNoteStack
{
public:
    bool empty() const noexcept { return count == 0; }
    int  top() const noexcept   { return count > 0 ? notes[(size_t) (count - 1)] : -1; }
    void clear() noexcept       { count = 0; }

    void push (int note) noexcept
    {
        remove (note);
        if (count == (int) notes.size())           // full: drop the oldest
        {
            for (int i = 1; i < count; ++i) notes[(size_t) (i - 1)] = notes[(size_t) i];
            --count;
        }
        notes[(size_t) count++] = note;
    }

    void remove (int note) noexcept
    {
        for (int i = 0; i < count; ++i)
            if (notes[(size_t) i] == note)
            {
                for (int j = i + 1; j < count; ++j) notes[(size_t) (j - 1)] = notes[(size_t) j];
                --count;
                return;
            }
    }

private:
    std::array<int, 16> notes {};
    int count = 0;
};
} // namespace dali
