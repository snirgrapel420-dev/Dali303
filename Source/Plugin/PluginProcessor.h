#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

#include "DSP/AcidEngine.h"
#include "MonoNoteStack.h"
#include "PatternStore.h"
#include "Presets/PresetManager.h"
#include "Sequencer/StepSequencer.h"

namespace dali
{
/**
    Dali303 — plugin shell. Owns the parameter tree, the engine, the sequencer
    and the preset manager, and turns host MIDI + host transport into
    sample-accurate engine events.
*/
class Dali303Processor : public juce::AudioProcessor,
                         private juce::AsyncUpdater
{
public:
    Dali303Processor();
    ~Dali303Processor() override;

    // --- AudioProcessor ---
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Dali303 API (message thread unless noted) ---
    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    PatternStore&  getPatternStore() noexcept               { return patternStore; }
    PresetManager& getPresetManager() noexcept              { return *presetManager; }

    int   getPlayingStep() const noexcept   { return playingStep.load (std::memory_order_relaxed); }
    float consumeOutputPeak() noexcept      { return outputPeak.exchange (0.0f, std::memory_order_relaxed); }
    float getCpuLoad() const noexcept       { return (float) loadMeasurer.getLoadAsProportion(); }
    void  setDevMode (int m) noexcept       { devMode.store (m, std::memory_order_relaxed); }
    int   getDevMode() const noexcept       { return devMode.load (std::memory_order_relaxed); }

    static juce::String getStateTypeName() { return "Dali303State"; }
    bool isStandalone() const noexcept      { return wrapperType == wrapperType_Standalone; }

private:
    struct Event
    {
        enum class Type { SeqNoteOn, SeqNoteOff, MidiNoteOn, MidiNoteOff, AllOff, PitchBend, Controller };
        int offset = 0;
        Type type = Type::AllOff;
        int note = 0;
        float value = 0.0f;       // velocity / bend / cc value
        bool accent = false, legato = false;
        float gateSteps = -1.0f;
        int stepIndex = -1;
        uint32_t positionKey = 0;
        int controller = 0;
    };

    void handleAsyncUpdate() override;
    void readParameters() noexcept;
    void pushEvent (const Event& e) noexcept;
    void applyEvent (const Event& e, double blockPpq, double ppqPerSample, bool hasPpq) noexcept;

    juce::AudioProcessorValueTreeState apvts;
    PatternStore patternStore;
    std::unique_ptr<PresetManager> presetManager;

    dsp::AcidEngine engine;
    dsp::EngineParams engineParams;
    dsp::PerformanceControls perf;
    StepSequencer sequencer;
    MonoNoteStack noteStack;
    float lastMidiVelocity = 0.8f;
    uint32_t freeRunningNoteCounter = 0;
    bool wasPlaying = false;
    bool seqWasOn = false;

    static constexpr int kMaxEvents = 1024;
    std::array<Event, kMaxEvents> events;
    int numEvents = 0;

    std::atomic<float>* pTune = nullptr; std::atomic<float>* pCutoff = nullptr; std::atomic<float>* pRes = nullptr;
    std::atomic<float>* pEnv = nullptr;  std::atomic<float>* pDecay = nullptr;  std::atomic<float>* pAccent = nullptr;
    std::atomic<float>* pSlide = nullptr; std::atomic<float>* pDrive = nullptr; std::atomic<float>* pLife = nullptr;
    std::atomic<float>* pOutput = nullptr; std::atomic<float>* pWave = nullptr; std::atomic<float>* pSeqOn = nullptr;
    std::atomic<float>* pOversampling = nullptr; std::atomic<float>* pClockBpm = nullptr;
    double internalPpq = 0.0;     // Standalone: internal transport when there is no host

    std::atomic<int>   playingStep { -1 };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<int>   devMode { 0 };
    std::atomic<int>   pendingLatency { 0 };
    int reportedLatency = -1;
    juce::AudioProcessLoadMeasurer loadMeasurer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Dali303Processor)
};
} // namespace dali
