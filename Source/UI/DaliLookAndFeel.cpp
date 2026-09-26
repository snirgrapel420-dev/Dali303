#include "DaliLookAndFeel.h"

namespace dali::ui
{
using namespace colours;

DaliLookAndFeel::DaliLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, purple);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, purpleDim.withAlpha (0.55f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, groove);
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::arrowColourId, purple);
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, text);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, groove);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, panelEdge);
    setColour (juce::TextEditor::focusedOutlineColourId, purple);
    setColour (juce::TextEditor::highlightColourId, purpleDim);
    setColour (juce::CaretComponent::caretColourId, magenta);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, purpleDim);
    setColour (juce::BubbleComponent::backgroundColourId, panel);
    setColour (juce::BubbleComponent::outlineColourId, purpleDim);
    setColour (juce::TooltipWindow::backgroundColourId, panel);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, purpleDim);
}

// ============================================================================
//  Knob
// ============================================================================
void DaliLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& slider)
{
    const bool hero    = (bool) slider.getProperties().getWithDefault ("dali_hero", false);
    const bool bipolar = (bool) slider.getProperties().getWithDefault ("dali_bipolar", false);
    const bool active  = slider.isMouseOverOrDragging();

    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float r  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    const float angle = startAngle + pos * (endAngle - startAngle);
    auto polar = [cx, cy] (float radius, float a) { return juce::Point<float> (cx + radius * std::sin (a), cy - radius * std::cos (a)); };

    // --- scale ticks ---------------------------------------------------------
    const int numTicks = hero ? 21 : 11;
    for (int i = 0; i < numTicks; ++i)
    {
        const float t = (float) i / (float) (numTicks - 1);
        const float a = startAngle + t * (endAngle - startAngle);
        const bool major = (i % (hero ? 5 : 5)) == 0;
        const bool lit = bipolar ? (t >= juce::jmin (0.5f, pos) && t <= juce::jmax (0.5f, pos)) : t <= pos;
        g.setColour (lit ? purple.withAlpha (0.75f) : textFaint.withAlpha (major ? 0.9f : 0.5f));
        g.drawLine (juce::Line<float> (polar (r * (major ? 0.90f : 0.93f), a), polar (r * 0.99f, a)), major ? 1.6f : 1.0f);
    }

    // --- value arc (groove + neon + glow) ---------------------------------------
    const float arcR = r * 0.83f, arcW = r * (hero ? 0.055f : 0.065f);
    juce::Path track;
    track.addCentredArc (cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (groove);
    g.strokePath (track, juce::PathStrokeType (arcW * 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path arc;
        arc.addCentredArc (cx, cy, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        const juce::ColourGradient grad (purple, cx - r, cy + r, magenta, cx + r, cy - r, false);

        const float glowBoost = hero ? 1.6f : (active ? 1.2f : 1.0f);
        for (int i = 3; i >= 1; --i)   // soft glow halo
        {
            g.setColour (purple.withAlpha (0.07f * glowBoost));
            g.strokePath (arc, juce::PathStrokeType (arcW * (1.0f + (float) i * 1.4f), juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }
        g.setGradientFill (grad);
        g.strokePath (arc, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- body: drop shadow, knurled skirt, cap ----------------------------------------
    const float bodyR = r * 0.70f;
    juce::Path body;
    body.addEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.85f), (int) (r * 0.28f), { 0, (int) (r * 0.12f) }).drawForPath (g, body);

    g.setGradientFill (juce::ColourGradient (metalLight, cx, cy - bodyR, metalDark, cx, cy + bodyR, false));
    g.fillPath (body);

    // knurling rotates with the knob -> movement feels physical
    const int ridges = hero ? 48 : 36;
    for (int i = 0; i < ridges; ++i)
    {
        const float a = angle + (float) i * juce::MathConstants<float>::twoPi / (float) ridges;
        const float light = 0.5f + 0.5f * std::cos (a + 0.6f);     // lit from upper left
        g.setColour (juce::Colours::white.withAlpha (0.03f + 0.10f * light));
        g.drawLine (juce::Line<float> (polar (bodyR * 0.86f, a), polar (bodyR * 0.99f, a)), 1.0f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.2f);

    const float capR = bodyR * 0.80f;
    juce::ColourGradient cap (juce::Colour (0xff302c36), cx - capR * 0.5f, cy - capR,
                              juce::Colour (0xff111014), cx + capR * 0.4f, cy + capR, false);
    g.setGradientFill (cap);
    g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);

    // concentric machining rings
    for (int i = 1; i <= 4; ++i)
    {
        const float rr = capR * (0.25f + 0.18f * (float) i);
        g.setColour (juce::Colours::white.withAlpha (0.018f));
        g.drawEllipse (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, 0.8f);
    }

    // rim highlight (top) + shade (bottom)
    juce::Path rimTop;
    rimTop.addCentredArc (cx, cy, capR - 0.5f, capR - 0.5f, 0.0f, -2.2f, 2.2f, true);
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.strokePath (rimTop, juce::PathStrokeType (1.0f));

    // specular
    const float sR = capR * 0.55f;
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), cx - capR * 0.35f, cy - capR * 0.55f,
                                             juce::Colours::transparentWhite, cx - capR * 0.35f + sR, cy - capR * 0.55f + sR, true));
    g.fillEllipse (cx - capR * 0.35f - sR, cy - capR * 0.55f - sR, sR * 2.0f, sR * 2.0f);

    // --- pointer ------------------------------------------------------------
    const auto p0 = polar (capR * 0.30f, angle), p1 = polar (capR * 0.92f, angle);
    g.setColour (magenta.withAlpha (hero ? 0.35f : 0.22f));
    g.drawLine (juce::Line<float> (p0, p1), hero ? 6.0f : 5.0f);
    g.setColour (magenta);
    g.drawLine (juce::Line<float> (p0, p1), 2.2f);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (p1));
}

// ============================================================================
//  Buttons
// ============================================================================
void DaliLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                            bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool hero = (bool) b.getProperties().getWithDefault ("dali_hero", false);
    const bool on = b.getToggleState();
    const float corner = 5.0f;

    if (hero)
    {
        const float glow = down ? 0.45f : (highlighted ? 0.35f : 0.22f);
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (magenta.withAlpha (glow * 0.18f));
            g.fillRoundedRectangle (r.expanded ((float) i * 1.5f - 1.0f), corner + (float) i);
        }
        g.setGradientFill (juce::ColourGradient (purple.withAlpha (down ? 0.95f : 0.8f), r.getX(), r.getY(),
                                                 magenta.withAlpha (down ? 0.95f : 0.8f), r.getRight(), r.getBottom(), false));
        g.fillRoundedRectangle (r, corner);
        g.setColour (juce::Colours::white.withAlpha (highlighted ? 0.35f : 0.2f));
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
        return;
    }

    g.setGradientFill (juce::ColourGradient (down ? groove : juce::Colour (0xff1d1a22), r.getX(), r.getY(),
                                             down ? groove : juce::Colour (0xff110f14), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);

    if (on)
    {
        g.setColour (magenta.withAlpha (0.12f));
        g.fillRoundedRectangle (r, corner);
        g.setColour (magenta.withAlpha (0.9f));
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.2f);
    }
    else
    {
        g.setColour (highlighted ? purpleDim.brighter (0.3f) : panelEdge.brighter (0.15f));
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    }
}

void DaliLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool highlighted, bool)
{
    const bool hero = (bool) b.getProperties().getWithDefault ("dali_hero", false);
    const bool on = b.getToggleState();
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (hero ? juce::Colours::white : (on ? text : (highlighted ? text : textDim)));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

juce::Font DaliLookAndFeel::getTextButtonFont (juce::TextButton& b, int h)
{
    const bool hero = (bool) b.getProperties().getWithDefault ("dali_hero", false);
    return font (juce::jmin (13.0f, (float) h * 0.45f), true, hero ? 0.22f : 0.12f);
}

// ============================================================================
//  Combo / popup / alert / bubble
// ============================================================================
void DaliLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (groove);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.isMouseOver (true) ? purpleDim.brighter (0.3f) : panelEdge.brighter (0.1f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 13.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (purple);
    g.fillPath (arrow);
}

juce::Font DaliLookAndFeel::getComboBoxFont (juce::ComboBox&)  { return font (12.0f, true, 0.08f); }

void DaliLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font DaliLookAndFeel::getPopupMenuFont()          { return font (14.0f); }

void DaliLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (panel);
    g.setColour (purpleDim);
    g.drawRect (0, 0, w, h, 1);
}

juce::Font DaliLookAndFeel::getAlertWindowTitleFont()   { return font (17.0f, true, 0.05f); }
juce::Font DaliLookAndFeel::getAlertWindowMessageFont() { return font (14.0f); }
juce::Font DaliLookAndFeel::getAlertWindowFont()        { return font (13.0f); }

void DaliLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&,
                                  const juce::Rectangle<float>& body)
{
    g.setColour (panel);
    g.fillRoundedRectangle (body, 4.0f);
    g.setColour (purpleDim);
    g.drawRoundedRectangle (body.reduced (0.5f), 4.0f, 1.0f);
}
} // namespace dali::ui
