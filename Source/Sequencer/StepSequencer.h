#pragma once

#include "Pattern.h"
#include <climits>
#include <cmath>

namespace dali
{
/** An event produced by the sequencer, sample-accurate within the current block. */
struct SeqEvent
{
    int      offset = 0;
    bool     noteOn = false;
    int      note = 36;
    bool     accent = false;
    bool     legato = false;
    float    gateSteps = 0.5f;
    int      stepIndex = 0;
    uint32_t positionKey = 0;
};

/**
    Host-synchronised 16-step acid sequencer (1/16 notes).

    Timing is derived ONLY from the host musical position (PPQ), never from an
    internal counter, so it follows play/stop, loops, locates and tempo changes.
    Gate rules (acid classic):
      - a gated step opens the gate at the step start and closes it half a step later;
      - a SLIDE step keeps the gate open into the next step; if that step is gated,
        its note is reached by gliding (legato, no envelope retrigger);
      - a rest closes any held gate.
    Header-only, no JUCE, no allocation.
*/
class StepSequencer
{
public:
    void reset() noexcept
    {
        running = false;
        heldNote = -1;
        prevSlide = false;
        currentStep = -1;
        lastHalfIndex = LLONG_MIN;
    }

    int getCurrentStep() const noexcept { return currentStep; }

    template <typename Emit>
    void stop (Emit&& emit) noexcept
    {
        if (heldNote >= 0) emitOff (0, emit);
        reset();
    }

    template <typename Emit>
    void process (bool playing, double ppq, double bpm, double sampleRate, int numSamples,
                  const Pattern& pattern, Emit&& emit) noexcept
    {
        if (! playing || bpm <= 0.0 || numSamples <= 0)
        {
            if (running) stop (emit);
            return;
        }

        const double sixteenthsPerSample = bpm / 60.0 * 4.0 / sampleRate;
        const double a = ppq * 4.0;
        const double b = a + numSamples * sixteenthsPerSample;

        if (! running || std::abs (a - expectedPos) > 0.01)
        {
            // transport started, looped or located: resync cleanly
            if (heldNote >= 0) emitOff (0, emit);
            prevSlide = false;
            running = true;
            lastHalfIndex = (long long) std::ceil (a * 2.0 - 1.0e-9) - 1;

            const double fl = std::floor (a);
            if (a - fl > 1.0e-9 && a - fl < 0.5)          // started inside a step's gate: play it now
                stepStart ((long long) fl, 0, pattern, emit);
        }
        expectedPos = b;

        for (long long m = lastHalfIndex + 1; (double) m < b * 2.0; ++m)
        {
            lastHalfIndex = m;
            const double t = (double) m * 0.5;
            int off = (int) ((t - a) / sixteenthsPerSample);
            off = off < 0 ? 0 : (off >= numSamples ? numSamples - 1 : off);

            const long long k = floorDiv2 (m);
            if (m - 2 * k == 0) stepStart (k, off, pattern, emit);
            else                halfStep (k, off, pattern, emit);
        }
    }

private:
    static long long floorDiv2 (long long m) noexcept { return m >= 0 ? m / 2 : -((-m + 1) / 2); }

    static int wrapIndex (long long k, int len) noexcept
    {
        const long long r = k % len;
        return (int) (r < 0 ? r + len : r);
    }

    template <typename Emit>
    void emitOff (int off, Emit& emit) noexcept
    {
        SeqEvent e;
        e.offset = off;
        e.noteOn = false;
        e.note = heldNote;
        emit (e);
        heldNote = -1;
    }

    template <typename Emit>
    void stepStart (long long k, int off, const Pattern& p, Emit& emit) noexcept
    {
        const int len = p.length > 0 ? p.length : Pattern::kMaxSteps;
        const int idx = wrapIndex (k, len);
        const Step& s = p.steps[(size_t) idx];
        currentStep = idx;

        if (s.gate)
        {
            const bool legato = heldNote >= 0 && prevSlide;
            if (heldNote >= 0 && ! legato) emitOff (off, emit);

            SeqEvent e;
            e.offset = off;
            e.noteOn = true;
            e.note = s.note;
            e.accent = s.accent;
            e.legato = legato;
            e.gateSteps = s.slide ? 1.0f : 0.5f;
            e.stepIndex = idx;
            e.positionKey = (uint32_t) (k & 0xffffffff);
            emit (e);
            heldNote = s.note;
        }
        else if (heldNote >= 0)
        {
            emitOff (off, emit);
        }
        prevSlide = s.gate && s.slide;
    }

    template <typename Emit>
    void halfStep (long long k, int off, const Pattern& p, Emit& emit) noexcept
    {
        const int len = p.length > 0 ? p.length : Pattern::kMaxSteps;
        const Step& s = p.steps[(size_t) wrapIndex (k, len)];
        if (s.gate && ! s.slide && heldNote >= 0)
            emitOff (off, emit);
    }

    bool running = false;
    double expectedPos = 0.0;
    long long lastHalfIndex = LLONG_MIN;
    int heldNote = -1;
    bool prevSlide = false;
    int currentStep = -1;
};
} // namespace dali
