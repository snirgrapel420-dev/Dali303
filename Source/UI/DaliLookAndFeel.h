#pragma once

#include "Colours.h"

namespace dali::ui
{
/**
    Dali303 look: machined black knobs with a knurled skirt, a glowing neon
    value arc and pointer; dark hardware buttons with neon edges when active.

    Slider properties:  "dali_hero"    -> larger glow (LIFE)
                        "dali_bipolar" -> value arc drawn from 12 o'clock (TUNE)
    Button property:    "dali_hero"    -> DISCOVER style
*/
class DaliLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DaliLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;

    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;

    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip,
                     const juce::Rectangle<float>& body) override;
};
} // namespace dali::ui
