#include "DaliKnob.h"

namespace dali::ui
{
DaliKnob::DaliKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
                    const juce::String& labelText, Style s, bool bipolar)
    : label (labelText), style (s)
{
    param = state.getParameter (paramId);
    jassert (param != nullptr);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
    slider.setMouseDragSensitivity (s == Style::Hero ? 320 : 240);
    slider.setVelocityBasedMode (false);
    slider.setScrollWheelEnabled (true);
    slider.getProperties().set ("dali_hero", s == Style::Hero);
    slider.getProperties().set ("dali_bipolar", bipolar);
    slider.setTitle (labelText);
    slider.onValueChange = [this] { repaint (valueArea); };
    slider.onDragStart   = [this] { repaint(); };
    slider.onDragEnd     = [this] { repaint(); };
    addAndMakeVisible (slider);
    slider.addMouseListener (this, false);   // hover lights the label + value

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);

    if (param != nullptr)
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

float DaliKnob::getNormalisedValue() const noexcept
{
    return param != nullptr ? param->getValue() : 0.0f;
}

void DaliKnob::resized()
{
    auto r = getLocalBounds();
    const int labelH = style == Style::Hero ? 26 : 18;
    const int valueH = style == Style::Hero ? 20 : 16;
    valueArea = r.removeFromBottom (valueH);
    labelArea = r.removeFromBottom (labelH);
    const int side = juce::jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (side, side));
}

void DaliKnob::paint (juce::Graphics& g)
{
    const bool touched = slider.isMouseOverOrDragging();

    g.setFont (font (style == Style::Hero ? 20.0f : (style == Style::Small ? 11.0f : 12.5f), true,
                     style == Style::Hero ? 0.45f : 0.2f));
    g.setColour (style == Style::Hero ? colours::text : (touched ? colours::text : colours::text.withAlpha (0.82f)));
    g.drawText (label, labelArea, juce::Justification::centred);

    g.setFont (font (style == Style::Hero ? 13.0f : 11.0f, false, 0.04f));
    g.setColour (touched ? colours::magenta : colours::textDim);
    g.drawText (slider.getTextFromValue (slider.getValue()), valueArea, juce::Justification::centredTop);
}
} // namespace dali::ui
