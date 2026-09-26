#include "PluginProcessor.h"
#include "Parameters.h"
#include "UI/PluginEditor.h"

namespace dali
{
Dali303Processor::Dali303Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, getStateTypeName(), params::createLayout())
{
    pTune         = apvts.getRawParameterValue (params::id::tune);
    pCutoff       = apvts.getRawParameterValue (params::id::cutoff);
    pRes          = apvts.getRawParameterValue (params::id::resonance);
    pEnv          = apvts.getRawParameterValue (params::id::envMod);
    pDecay        = apvts.getRawParameterValue (params::id::decay);
    pAccent       = apvts.getRawParameterValue (params::id::accent);
    pSlide        = apvts.getRawParameterValue (params::id::slide);
    pDrive        = apvts.getRawParameterValue (params::id::drive);
    pLife         = apvts.getRawParameterValue (params::id::life);
    pOutput       = apvts.getRawParameterValue (params::id::output);
    pWave         = apvts.getRawParameterValue (params::id::wave);
    pSeqOn        = apvts.getRawParameterValue (params::id::seqOn);
    pOversampling = apvts.getRawParameterValue (params::id::oversampling);
    pClockBpm     = apvts.getRawParameterValue (params::id::clockBpm);

    presetManager = std::make_unique<PresetManager> (apvts, patternStore);
    presetManager->load (0);   // start on the first factory preset
}

Dali303Processor::~Dali303Processor()
{
    cancelPendingUpdate();
}

bool Dali303Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void Dali303Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    readParameters();
    engine.prepare (sampleRate);           // all allocation happens here
    engine.setParams (engineParams);
    engine.setPerformance (perf);
    engine.reset();
    sequencer.reset();
    noteStack.clear();
    wasPlaying = false;
    internalPpq = 0.0;
    loadMeasurer.reset (sampleRate, samplesPerBlock);

    reportedLatency = engine.getLatencySamples();
    pendingLatency.store (reportedLatency);
    setLatencySamples (reportedLatency);
}

void Dali303Processor::readParameters() noexcept
{
    auto& p = engineParams;
    p.tune      = pTune->load();
    p.cutoff    = pCutoff->load();
    p.resonance = pRes->load();
    p.envMod    = pEnv->load();
    p.decay     = pDecay->load();
    p.accent    = pAccent->load();
    p.slide     = pSlide->load();
    p.drive     = pDrive->load();
    p.life      = pLife->load();
    p.outputDb  = pOutput->load();
    p.wave      = (int) pWave->load();
    static constexpr int osFactors[3] = { 1, 2, 4 };
    p.oversampling = osFactors[juce::jlimit (0, 2, (int) pOversampling->load())];
}

void Dali303Processor::pushEvent (const Event& e) noexcept
{
    if (numEvents < kMaxEvents)
        events[(size_t) numEvents++] = e;
}

void Dali303Processor::applyEvent (const Event& e, double blockPpq, double ppqPerSample, bool hasPpq) noexcept
{
    switch (e.type)
    {
        case Event::Type::SeqNoteOn:
        {
            dsp::NoteOnInfo ni;
            ni.note = e.note; ni.velocity = e.accent ? 1.0f : 0.8f; ni.accent = e.accent; ni.legato = e.legato;
            ni.gateSteps = e.gateSteps; ni.stepIndex = e.stepIndex; ni.positionKey = e.positionKey;
            engine.noteOn (ni);
            break;
        }
        case Event::Type::SeqNoteOff:
            if (noteStack.empty()) engine.noteOff();
            break;

        case Event::Type::MidiNoteOn:
        {
            // position identity for deterministic LIFE: host 16th position when available
            uint32_t key; int step;
            if (hasPpq)
            {
                const auto pos16 = (long long) std::floor ((blockPpq + e.offset * ppqPerSample) * 4.0 + 1.0e-6);
                key  = (uint32_t) (pos16 & 0xffffffff);
                step = (int) (((pos16 % 16) + 16) % 16);
            }
            else
            {
                key  = freeRunningNoteCounter++;
                step = -1;
            }

            const bool legato = ! noteStack.empty();          // overlapping notes = slide (acid convention)
            noteStack.push (e.note);
            lastMidiVelocity = e.value;

            dsp::NoteOnInfo ni;
            ni.note = e.note; ni.velocity = e.value; ni.accent = e.value >= 0.78f;   // velocity >= 100 = accent
            ni.legato = legato; ni.stepIndex = step; ni.positionKey = key;
            engine.noteOn (ni);
            break;
        }
        case Event::Type::MidiNoteOff:
        {
            const bool wasTop = noteStack.top() == e.note;
            noteStack.remove (e.note);
            if (wasTop)
            {
                if (! noteStack.empty())
                {
                    dsp::NoteOnInfo ni;                        // glide back to the still-held note
                    ni.note = noteStack.top(); ni.velocity = lastMidiVelocity; ni.legato = true;
                    engine.noteOn (ni);
                }
                else engine.noteOff();
            }
            break;
        }
        case Event::Type::AllOff:
            noteStack.clear();
            engine.allNotesOff();
            break;

        case Event::Type::PitchBend:
            perf.pitchBend = e.value * 2.0f;
            engine.setPerformance (perf);
            break;

        case Event::Type::Controller:
            if (e.controller == 1)       perf.modWheel    = e.value;
            else if (e.controller == 74) perf.ccCutoff    = e.value * 2.0f - 1.0f;
            else if (e.controller == 71) perf.ccResonance = e.value * 2.0f - 1.0f;
            engine.setPerformance (perf);
            break;
    }
}

void Dali303Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::AudioProcessLoadMeasurer::ScopedTimer timer (loadMeasurer, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    readParameters();
    engine.setDevMode ((dsp::DevMode) devMode.load (std::memory_order_relaxed));
    engine.setParams (engineParams);

    if (engine.getLatencySamples() != pendingLatency.load (std::memory_order_relaxed))
    {
        pendingLatency.store (engine.getLatencySamples());
        triggerAsyncUpdate();
    }

    // ---------------- host transport ----------------
    bool playing = false, hasPpq = false;
    double ppq = 0.0, bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm())         bpm = *b;
            if (auto p = pos->getPpqPosition()) { ppq = *p; hasPpq = true; }
        }
    // Standalone has no host transport: run an internal clock while SEQ is on.
    const bool seqOn = pSeqOn->load() > 0.5f;
    if (! hasPpq && isStandalone())
    {
        if (! seqOn) internalPpq = 0.0;                // restart from the downbeat next time
        playing = seqOn;
        hasPpq  = true;
        bpm     = (double) pClockBpm->load();
        ppq     = internalPpq;
        internalPpq += (double) numSamples * bpm / 60.0 / getSampleRate();
    }

    if (playing && ! wasPlaying)
        engine.resetContext();              // same song position => same LIFE result
    wasPlaying = playing;
    const double ppqPerSample = bpm / 60.0 / getSampleRate();

    // ---------------- gather events ----------------
    numEvents = 0;
    auto seqEmit = [this] (const SeqEvent& s)
    {
        Event e;
        e.offset = s.offset;
        e.type = s.noteOn ? Event::Type::SeqNoteOn : Event::Type::SeqNoteOff;
        e.note = s.note; e.accent = s.accent; e.legato = s.legato;
        e.gateSteps = s.gateSteps; e.stepIndex = s.stepIndex; e.positionKey = s.positionKey;
        pushEvent (e);
    };

    if (seqOn)
        sequencer.process (playing && hasPpq, ppq, bpm, getSampleRate(), numSamples, patternStore.read(), seqEmit);
    else if (seqWasOn)
        sequencer.stop (seqEmit);
    seqWasOn = seqOn;
    playingStep.store (seqOn && playing ? sequencer.getCurrentStep() : -1, std::memory_order_relaxed);

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        Event e;
        e.offset = juce::jlimit (0, juce::jmax (0, numSamples - 1), meta.samplePosition);
        if (msg.isNoteOn())                              { e.type = Event::Type::MidiNoteOn;  e.note = msg.getNoteNumber(); e.value = msg.getFloatVelocity(); }
        else if (msg.isNoteOff())                        { e.type = Event::Type::MidiNoteOff; e.note = msg.getNoteNumber(); }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) { e.type = Event::Type::AllOff; }
        else if (msg.isPitchWheel())                     { e.type = Event::Type::PitchBend; e.value = (float) (msg.getPitchWheelValue() - 8192) / 8192.0f; }
        else if (msg.isController())                     { e.type = Event::Type::Controller; e.controller = msg.getControllerNumber(); e.value = (float) msg.getControllerValue() / 127.0f; }
        else continue;
        pushEvent (e);
    }

    // stable insertion sort by offset (sequencer events keep priority at equal offsets)
    for (int i = 1; i < numEvents; ++i)
    {
        const Event tmp = events[(size_t) i];
        int j = i - 1;
        while (j >= 0 && events[(size_t) j].offset > tmp.offset) { events[(size_t) (j + 1)] = events[(size_t) j]; --j; }
        events[(size_t) (j + 1)] = tmp;
    }

    // ---------------- render, split at events (sample accurate) ----------------
    float* out = buffer.getWritePointer (0);
    int pos = 0;
    for (int i = 0; i < numEvents; ++i)
    {
        const auto& e = events[(size_t) i];
        if (e.offset > pos) { engine.process (out + pos, e.offset - pos); pos = e.offset; }
        applyEvent (e, ppq, ppqPerSample, hasPpq);
    }
    if (pos < numSamples) engine.process (out + pos, numSamples - pos);

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);

    const float peak = buffer.getMagnitude (0, 0, numSamples);
    float prev = outputPeak.load (std::memory_order_relaxed);
    while (peak > prev && ! outputPeak.compare_exchange_weak (prev, peak, std::memory_order_relaxed)) {}

    midi.clear();
}

void Dali303Processor::handleAsyncUpdate()
{
    const int lat = pendingLatency.load();
    if (lat != reportedLatency)
    {
        reportedLatency = lat;
        setLatencySamples (lat);
    }
}

// ============================================================================
//  State: parameters + pattern + preset identity. Restores the exact session.
// ============================================================================
void Dali303Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml == nullptr) return;

    xml->setAttribute ("stateVersion", 1);
    auto* pat = xml->createNewChildElement ("Pattern");
    pat->setAttribute ("data", juce::String (patternToString (patternStore.read())));

    auto* meta = xml->createNewChildElement ("PresetMeta");
    const auto info = presetManager->getCurrentInfo();
    meta->setAttribute ("name", info.name);
    meta->setAttribute ("category", info.category);
    meta->setAttribute ("modified", info.modified);

    copyXmlToBinary (*xml, destData);
}

void Dali303Processor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    Pattern restored;
    bool havePattern = false;
    juce::String presetName, presetCategory;
    bool modified = false;

    if (auto* pat = xml->getChildByName ("Pattern"))
        havePattern = patternFromString (pat->getStringAttribute ("data").toStdString(), restored);
    if (auto* meta = xml->getChildByName ("PresetMeta"))
    {
        presetName     = meta->getStringAttribute ("name");
        presetCategory = meta->getStringAttribute ("category");
        modified       = meta->getBoolAttribute ("modified");
    }

    xml->deleteAllChildElementsWithTagName ("Pattern");
    xml->deleteAllChildElementsWithTagName ("PresetMeta");
    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    if (havePattern)
        patternStore.write (restored);
    presetManager->restoreIdentity (presetName, presetCategory, modified);
}

juce::AudioProcessorEditor* Dali303Processor::createEditor()
{
    return new Dali303Editor (*this);
}
} // namespace dali

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new dali::Dali303Processor();
}
