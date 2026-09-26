#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Colours.h"

namespace dali::ui
{
/**
    One hardware knob: rotary slider bound to a parameter, a name label and a
    value readout that lights up while the knob is touched.
    Double-click returns to the default value.
*/
class DaliKnob : public juce::Component
{
public:
    enum class Style { Normal, Hero, Small };

    DaliKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
              const juce::String& labelText, Style style = Style::Normal, bool bipolar = false);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    juce::Slider& getSlider() noexcept { return slider; }
    float getNormalisedValue() const noexcept;

private:
    juce::String label;
    Style style;
    juce::RangedAudioParameter* param = nullptr;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::Rectangle<int> labelArea, valueArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DaliKnob)
};
} // namespace dali::ui
