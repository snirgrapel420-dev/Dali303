#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace dali::ui
{
/** Dali303 palette: black hardware, neon used only where it carries meaning. */
namespace colours
{
    inline const juce::Colour background   { 0xff09080c };
    inline const juce::Colour panel        { 0xff121016 };
    inline const juce::Colour panelHi      { 0xff1a1720 };
    inline const juce::Colour panelEdge    { 0xff26222d };
    inline const juce::Colour groove       { 0xff060508 };
    inline const juce::Colour metalDark    { 0xff141217 };
    inline const juce::Colour metalLight   { 0xff3b3742 };

    inline const juce::Colour purple       { 0xffa64dff };
    inline const juce::Colour purpleDim    { 0xff5a2d8a };
    inline const juce::Colour magenta      { 0xffff3d9a };
    inline const juce::Colour magentaDim   { 0xff8a2456 };

    inline const juce::Colour text         { 0xffe9e4f0 };
    inline const juce::Colour textDim      { 0xff8b8496 };
    inline const juce::Colour textFaint    { 0xff4d4856 };
}

inline juce::Font font (float size, bool bold = false, float kerning = 0.0f)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)
                           .withKerningFactor (kerning));
}

/** Rounded hardware panel with a soft top highlight and a fine edge. */
inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 10.0f)
{
    juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 18, { 0, 6 }).drawForRectangle (g, r.toNearestInt());
    g.setGradientFill (juce::ColourGradient (colours::panelHi, r.getX(), r.getY(),
                                             colours::panel, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + corner, r.getRight() - corner);
}

/** Small spaced-caps section title with a neon tick. */
inline void drawSectionTitle (juce::Graphics& g, const juce::String& title, juce::Rectangle<float> r,
                              juce::Colour accent = colours::purple)
{
    g.setColour (accent);
    g.fillRoundedRectangle (r.getX(), r.getCentreY() - 5.0f, 3.0f, 10.0f, 1.5f);
    g.setColour (colours::textDim);
    g.setFont (font (11.0f, true, 0.25f));
    g.drawText (title, r.withTrimmedLeft (10.0f), juce::Justification::centredLeft);
}
} // namespace dali::ui
