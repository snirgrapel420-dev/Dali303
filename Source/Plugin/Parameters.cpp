#include "Parameters.h"
#include "DSP/EngineParams.h"

namespace dali::params
{
const juce::StringArray& soundParameterIds()
{
    static const juce::StringArray ids { id::tune, id::cutoff, id::resonance, id::envMod, id::decay,
                                         id::accent, id::slide, id::drive, id::life, id::output, id::wave };
    return ids;
}

namespace
{
    std::unique_ptr<juce::AudioParameterFloat> knob (const char* pid, const char* name, float def,
                                                     std::function<juce::String (float, int)> text)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { pid, 1 }, name, juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (text)));
    }

    juce::String percent (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace dali::dsp;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id::tune, 1 }, "Tune", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("st").withStringFromValueFunction (
            [] (float v, int) { return (v >= 0 ? "+" : "") + juce::String (v, 2) + " st"; })));

    layout.add (knob (id::cutoff, "Cutoff", 0.33f, [] (float v, int) {
        const float hz = mapping::cutoffHz (v);
        return hz < 1000.0f ? juce::String (juce::roundToInt (hz)) + " Hz" : juce::String (hz / 1000.0f, 2) + " kHz"; }));
    layout.add (knob (id::resonance, "Resonance", 0.72f, percent));
    layout.add (knob (id::envMod,    "Env Mod",   0.55f, percent));
    layout.add (knob (id::decay,     "Decay",     0.38f, [] (float v, int) {
        const float s = mapping::decaySeconds (v);
        return s < 1.0f ? juce::String (juce::roundToInt (s * 1000.0f)) + " ms" : juce::String (s, 2) + " s"; }));
    layout.add (knob (id::accent,    "Accent",    0.65f, percent));
    layout.add (knob (id::slide,     "Slide",     0.35f, [] (float v, int) {
        return juce::String (juce::roundToInt (mapping::slideSeconds (v) * 1000.0f)) + " ms"; }));
    layout.add (knob (id::drive,     "Drive",     0.25f, percent));
    layout.add (knob (id::life,      "Life",      0.25f, percent));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id::output, 1 }, "Output", juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f), -3.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, 1) + " dB"; })));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { id::wave, 1 }, "Waveform", juce::StringArray { "Saw", "Square" }, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { id::seqOn, 1 }, "Sequencer", true));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { id::oversampling, 1 }, "Oversampling", juce::StringArray { "1x", "2x", "4x" }, 1,
        juce::AudioParameterChoiceAttributes().withAutomatable (false)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id::clockBpm, 1 }, "Internal BPM", juce::NormalisableRange<float> (60.0f, 200.0f, 0.1f), 128.0f,
        juce::AudioParameterFloatAttributes().withLabel ("BPM").withAutomatable (false).withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, 1) + " BPM"; })));

    return layout;
}
} // namespace dali::params
