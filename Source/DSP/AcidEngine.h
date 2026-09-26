#pragma once

#include "AccentEngine.h"
#include "AcidCircuit.h"
#include "EngineParams.h"
#include "Envelope.h"
#include "LifeEngine.h"
#include "NoteContext.h"
#include "Oscillator.h"
#include "SlideEngine.h"

namespace dali::dsp
{
/** A note event as seen by the engine (from MIDI or the internal sequencer). */
struct NoteOnInfo
{
    int      note        = 36;
    float    velocity    = 0.8f;
    bool     accent      = false;
    bool     legato      = false;   // true = slide into this note (no envelope retrigger)
    float    gateSteps   = -1.0f;   // expected gate length in 16ths, -1 if unknown (live MIDI)
    int      stepIndex   = -1;
    uint32_t positionKey = 0;
};

/**
    AcidEngine — the complete monophonic Dali303 voice.

        SlideEngine ─► pitch ─► AcidOscillator ─► DALI ACID CIRCUIT ─► DC ─► VCA ─► out
                                                   ▲   ▲   ▲
         Filter env (MEG) ── AccentEngine ── LifeEngine / NoteContext ── knobs

    Every module is swappable behind a small interface. The engine owns all
    interaction logic ("how accent touches resonance", "how slide touches the
    filter", …) in one place: computeFrame().

    Realtime-safe: no allocation after prepare(); process() is noexcept.
*/
class AcidEngine
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void resetContext() noexcept;          // call on transport start -> repeatable playback

    void setParams (const EngineParams& p) noexcept;
    void setPerformance (const PerformanceControls& pc) noexcept;
    void setDevMode (DevMode m) noexcept   { devMode = m; }

    void noteOn (const NoteOnInfo& info) noexcept;
    void noteOff() noexcept;
    void allNotesOff() noexcept;

    void process (float* out, int numSamples) noexcept;

    int    getLatencySamples() const noexcept { return circuit.getLatencySamples(); }
    int    getActiveOversampling() const noexcept { return circuit.getOversampling(); }
    double getSampleRate() const noexcept     { return fs; }

    static int effectiveOversampling (int requested, double sampleRate) noexcept;

private:
    CircuitFrame computeFrame (float cutoffOct, float res, float drive, float sweep) const noexcept;

    double fs = 44100.0;
    EngineParams params;
    DevMode devMode = DevMode::Full;

    AcidOscillator osc;
    Envelope       filterEnv, ampEnv;
    AccentEngine   accent;
    SlideEngine    slide;
    LifeEngine     life;
    AcidCircuit    circuit;
    DcBlocker      dc;

    Smoother smCutoff, smRes, smEnvMod, smDrive, smOut, smBend, smMod, smResMod, smAccVca;

    NoteMods mods;
    CircuitFrame prevFrame;
    bool  noteAccent = false;
    float noteVelocity = 0.8f;
    int   currentNote = -1;
    int   prevNote = -1;
    bool  prevAccent = false, prevSlide = false;
    int   accentRun = 0;
    float lastGateSteps = 0.5f;
    int   appliedOversampling = -1, lastWave = -1;
};
} // namespace dali::dsp
