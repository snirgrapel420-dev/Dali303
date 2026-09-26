#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace dali::params
{
namespace id
{
    inline constexpr const char* tune         = "tune";
    inline constexpr const char* cutoff       = "cutoff";
    inline constexpr const char* resonance    = "resonance";
    inline constexpr const char* envMod       = "envmod";
    inline constexpr const char* decay        = "decay";
    inline constexpr const char* accent       = "accent";
    inline constexpr const char* slide        = "slide";
    inline constexpr const char* drive        = "drive";
    inline constexpr const char* life         = "life";
    inline constexpr const char* output       = "output";
    inline constexpr const char* wave         = "wave";
    inline constexpr const char* seqOn        = "seqon";
    inline constexpr const char* oversampling = "oversampling";
    inline constexpr const char* clockBpm     = "clockbpm";   // Standalone internal clock only
}

/** Parameters that are part of a preset's sound (everything except global settings). */
const juce::StringArray& soundParameterIds();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
} // namespace dali::params
