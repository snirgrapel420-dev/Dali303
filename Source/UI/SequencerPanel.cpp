#include "SequencerPanel.h"
#include "Plugin/Parameters.h"

namespace dali::ui
{
using namespace colours;

namespace
{
    constexpr int kMinNote = 24, kMaxNote = 67;    // C1 .. G4
    const char* rowNames[] = { "STEP", "NOTE", "GATE", "ACCENT", "SLIDE" };
}

SequencerPanel::SequencerPanel (juce::AudioProcessorValueTreeState& state, PatternStore& s, bool showInternalClock)
    : store (s)
{
    if (showInternalClock)
    {
        bpmSlider.setSliderStyle (juce::Slider::LinearBar);
        bpmSlider.setColour (juce::Slider::trackColourId, purpleDim.withAlpha (0.6f));
        bpmSlider.setColour (juce::Slider::backgroundColourId, groove);
        bpmSlider.setColour (juce::Slider::textBoxTextColourId, text);
        bpmSlider.setColour (juce::Slider::textBoxOutlineColourId, panelEdge);
        bpmSlider.setTooltip ("Internal clock (Standalone only - in a DAW the sequencer follows the host)");
        bpmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, params::id::clockBpm, bpmSlider);
        addAndMakeVisible (bpmSlider);
    }

    pattern = store.read();
    seenVersion = store.getVersion();

    seqButton.setClickingTogglesState (true);
    seqButton.setTooltip ("Internal sequencer on/off. Off = play Dali303 from MIDI notes.");
    seqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, params::id::seqOn, seqButton);

    shiftLeft.setTooltip ("Rotate pattern left");
    shiftRight.setTooltip ("Rotate pattern right");
    downSemi.setTooltip ("Transpose -1 semitone");
    upSemi.setTooltip ("Transpose +1 semitone");
    downOct.setTooltip ("Transpose -1 octave");
    upOct.setTooltip ("Transpose +1 octave");

    shiftLeft.onClick  = [this] { shift (-1); };
    shiftRight.onClick = [this] { shift (1); };
    downSemi.onClick   = [this] { transpose (-1); };
    upSemi.onClick     = [this] { transpose (1); };
    downOct.onClick    = [this] { transpose (-12); };
    upOct.onClick      = [this] { transpose (12); };

    for (auto* b : { &seqButton, &shiftLeft, &shiftRight, &downSemi, &upSemi, &downOct, &upOct })
        addAndMakeVisible (b);
}

void SequencerPanel::update (int playingStep)
{
    const auto v = store.getVersion();
    if (v != seenVersion)
    {
        pattern = store.read();
        seenVersion = v;
        repaint();
    }
    if (playingStep != playing)
    {
        playing = playingStep;
        repaint (grid.toNearestInt().withHeight ((int) (rowY[NoteRow] - grid.getY())).expanded (2));
    }
}

void SequencerPanel::commit()
{
    store.write (pattern);
    seenVersion = store.getVersion();
    repaint();
}

void SequencerPanel::transpose (int semis)
{
    int lo = 127, hi = 0;
    for (const auto& s : pattern.steps) { lo = juce::jmin (lo, (int) s.note); hi = juce::jmax (hi, (int) s.note); }
    if (lo + semis < kMinNote || hi + semis > kMaxNote) return;      // keep the line intact
    for (auto& s : pattern.steps) s.note = (uint8_t) (s.note + semis);
    commit();
}

void SequencerPanel::shift (int dir)
{
    auto old = pattern.steps;
    for (int i = 0; i < Pattern::kMaxSteps; ++i)
        pattern.steps[(size_t) ((i + dir + Pattern::kMaxSteps) % Pattern::kMaxSteps)] = old[(size_t) i];
    commit();
}

bool SequencerPanel::getFlag (int row, int col) const
{
    const auto& s = pattern.steps[(size_t) col];
    return row == GateRow ? s.gate : row == AccentRow ? s.accent : s.slide;
}

void SequencerPanel::setFlag (int row, int col, bool v)
{
    auto& s = pattern.steps[(size_t) col];
    if (row == GateRow) s.gate = v; else if (row == AccentRow) s.accent = v; else s.slide = v;
}

// ---------------------------------------------------------------------------
void SequencerPanel::resized()
{
    auto r = getLocalBounds().reduced (18, 12);
    auto top = r.removeFromTop (28);

    // right-hand tools
    auto tool = [&top] (juce::Button& b, int w) { b.setBounds (top.removeFromRight (w).reduced (0, 1)); top.removeFromRight (6); };
    tool (upOct, 50); tool (downOct, 50);
    top.removeFromRight (6);
    tool (upSemi, 36); tool (downSemi, 36);
    top.removeFromRight (6);
    tool (shiftRight, 30); tool (shiftLeft, 30);
    top.removeFromRight (12);
    tool (seqButton, 76);
    if (bpmSlider.isVisible()) tool (bpmSlider, 110);

    r.removeFromTop (10);
    const float labelW = 64.0f;
    grid = r.toFloat().withTrimmedLeft (labelW);

    const float groupGap = 10.0f;
    gap = 5.0f;
    colW = (grid.getWidth() - 3.0f * groupGap - 12.0f * gap) / 16.0f;

    const float heights[NumRows] = { 16.0f, 70.0f, 26.0f, 26.0f, 26.0f };
    float total = 0; for (float h : heights) total += h;
    const float vgap = (grid.getHeight() - total) / (float) (NumRows - 1);
    float y = grid.getY();
    for (int i = 0; i < NumRows; ++i) { rowY[(size_t) i] = y; rowH[(size_t) i] = heights[i]; y += heights[i] + vgap; }
}

juce::Rectangle<float> SequencerPanel::cell (int row, int col) const
{
    constexpr float groupGap = 10.0f;
    const float x = grid.getX() + (float) col * (colW + gap) + (float) (col / 4) * (groupGap - gap);
    return { x, rowY[(size_t) row], colW, rowH[(size_t) row] };
}

std::pair<int, int> SequencerPanel::hitTest (juce::Point<float> p) const
{
    for (int row = NoteRow; row < NumRows; ++row)
        for (int col = 0; col < Pattern::kMaxSteps; ++col)
            if (cell (row, col).expanded (gap * 0.5f, 3.0f).contains (p))
                return { row, col };
    return { NoRow, -1 };
}

// ---------------------------------------------------------------------------
void SequencerPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat().reduced (1.0f), 12.0f);
    drawSectionTitle (g, "16-STEP SEQUENCER", getLocalBounds().reduced (18, 12).removeFromTop (28).toFloat(), magenta);

    // row labels
    g.setFont (font (10.0f, true, 0.2f));
    for (int row = 0; row < NumRows; ++row)
    {
        g.setColour (row == hoverRow ? text : textFaint.brighter (0.4f));
        g.drawText (rowNames[row], juce::Rectangle<float> (grid.getX() - 64.0f, rowY[(size_t) row], 60.0f, rowH[(size_t) row]),
                    juce::Justification::centredLeft);
    }

    // beat group backplates
    for (int b = 0; b < 4; ++b)
    {
        auto a = cell (StepRow, b * 4), z = cell (SlideRow, b * 4 + 3);
        g.setColour (juce::Colours::white.withAlpha (b % 2 == 0 ? 0.018f : 0.008f));
        g.fillRoundedRectangle (juce::Rectangle<float> (a.getTopLeft(), z.getBottomRight()).expanded (3.0f, 4.0f), 6.0f);
    }

    // note range (for the pitch bar)
    int lo = 127, hi = 0;
    for (const auto& s : pattern.steps) { lo = juce::jmin (lo, (int) s.note); hi = juce::jmax (hi, (int) s.note); }
    const float span = (float) juce::jmax (12, hi - lo);

    for (int col = 0; col < Pattern::kMaxSteps; ++col)
    {
        const auto& st = pattern.steps[(size_t) col];
        const bool isPlaying = col == playing;

        // --- step LED + number
        auto led = cell (StepRow, col);
        auto dot = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ led.getX() + 8.0f, led.getCentreY() });
        if (isPlaying)
        {
            g.setColour (magenta.withAlpha (0.25f));
            g.fillEllipse (dot.expanded (5.0f));
            g.setColour (magenta);
        }
        else g.setColour (col % 4 == 0 ? textFaint.brighter (0.2f) : textFaint.darker (0.3f));
        g.fillEllipse (dot);
        g.setFont (font (10.0f, true));
        g.setColour (isPlaying ? text : textFaint.brighter (0.3f));
        g.drawText (juce::String (col + 1), led.withTrimmedLeft (16.0f), juce::Justification::centredLeft);

        // --- note cell
        auto nc = cell (NoteRow, col);
        g.setColour (groove);
        g.fillRoundedRectangle (nc, 5.0f);
        const bool hoverNote = hoverRow == NoteRow && hoverCol == col;
        g.setColour (isPlaying ? magenta.withAlpha (0.8f) : (hoverNote ? purpleDim.brighter (0.4f) : panelEdge));
        g.drawRoundedRectangle (nc.reduced (0.5f), 5.0f, isPlaying ? 1.4f : 1.0f);

        // pitch bar: height = position in the pattern's range
        const float frac = ((float) st.note - (float) lo) / span;
        auto bar = nc.reduced (5.0f, 6.0f).withTrimmedTop (22.0f);
        auto fill = bar.withTop (bar.getBottom() - juce::jmax (3.0f, bar.getHeight() * (0.15f + 0.85f * frac)));
        g.setColour ((st.gate ? purple : textFaint).withAlpha (st.gate ? 0.30f : 0.15f));
        g.fillRoundedRectangle (fill, 2.0f);
        g.setColour ((st.gate ? purple : textFaint).withAlpha (st.gate ? 0.9f : 0.4f));
        g.fillRoundedRectangle (fill.withHeight (2.0f), 1.0f);

        g.setFont (font (12.5f, true));
        g.setColour (st.gate ? text : textFaint);
        g.drawText (juce::String (noteName (st.note)), nc.withHeight (24.0f), juce::Justification::centred);

        // --- toggles
        for (int row = GateRow; row <= SlideRow; ++row)
        {
            auto c = cell (row, col).reduced (1.0f, 2.0f);
            const bool on = getFlag (row, col);
            const bool dimmed = row != GateRow && ! st.gate && row != SlideRow;
            const juce::Colour hue = row == AccentRow ? magenta : (row == SlideRow ? purple : text.withAlpha (0.85f));

            g.setColour (groove);
            g.fillRoundedRectangle (c, 4.0f);
            if (on)
            {
                const float a = dimmed ? 0.35f : 1.0f;
                g.setColour (hue.withAlpha (0.18f * a));
                g.fillRoundedRectangle (c.expanded (1.5f), 5.0f);
                g.setColour (hue.withAlpha (0.85f * a));
                g.fillRoundedRectangle (c.reduced (c.getWidth() * 0.18f, c.getHeight() * 0.32f), 2.5f);
            }
            const bool hov = hoverRow == row && hoverCol == col;
            g.setColour (hov ? purpleDim.brighter (0.4f) : panelEdge.darker (0.1f));
            g.drawRoundedRectangle (c.reduced (0.5f), 4.0f, 1.0f);
        }

        // slide connector: shows the glide into the next step
        if (st.slide)
        {
            auto a = cell (SlideRow, col), b = cell (SlideRow, (col + 1) % Pattern::kMaxSteps);
            if (col < Pattern::kMaxSteps - 1)
            {
                g.setColour (purple.withAlpha (0.55f));
                g.drawLine (a.getRight() - 3.0f, a.getCentreY(), b.getX() + 3.0f, b.getCentreY(), 2.0f);
            }
        }
    }
}

// ---------------------------------------------------------------------------
void SequencerPanel::mouseDown (const juce::MouseEvent& e)
{
    const auto [row, col] = hitTest (e.position);
    dragRow = row; dragCol = col;
    if (row == NoRow) return;

    if (row == NoteRow)
    {
        dragStartNote = pattern.steps[(size_t) col].note;
        dragStartY = e.position.y;
        if (e.mods.isPopupMenu() || e.mods.isAltDown())      // alt/right-click: toggle rest
        {
            pattern.steps[(size_t) col].gate = ! pattern.steps[(size_t) col].gate;
            commit();
            dragRow = NoRow;
        }
        return;
    }
    paintValue = ! getFlag (row, col);
    setFlag (row, col, paintValue);
    commit();
}

void SequencerPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRow == NoRow) return;

    if (dragRow == NoteRow)
    {
        const float pxPerSemi = e.mods.isShiftDown() ? 18.0f : 7.0f;       // shift = fine
        const int n = juce::jlimit (kMinNote, kMaxNote, dragStartNote + (int) std::round ((dragStartY - e.position.y) / pxPerSemi));
        if (n != pattern.steps[(size_t) dragCol].note)
        {
            pattern.steps[(size_t) dragCol].note = (uint8_t) n;
            commit();
        }
        return;
    }

    // paint-drag along the same row
    for (int col = 0; col < Pattern::kMaxSteps; ++col)
    {
        const auto c = cell (dragRow, col);
        if (e.position.x >= c.getX() - gap * 0.5f && e.position.x < c.getRight() + gap * 0.5f && getFlag (dragRow, col) != paintValue)
        {
            setFlag (dragRow, col, paintValue);
            commit();
        }
    }
}

void SequencerPanel::mouseUp (const juce::MouseEvent&)  { dragRow = NoRow; }

void SequencerPanel::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto [row, col] = hitTest (e.position);
    if (row != NoteRow || w.deltaY == 0.0f) return;
    const int step = e.mods.isShiftDown() ? 12 : 1;
    const int n = juce::jlimit (kMinNote, kMaxNote, (int) pattern.steps[(size_t) col].note + (w.deltaY > 0 ? step : -step));
    pattern.steps[(size_t) col].note = (uint8_t) n;
    commit();
}

void SequencerPanel::mouseMove (const juce::MouseEvent& e)
{
    const auto [row, col] = hitTest (e.position);
    if (row != hoverRow || col != hoverCol) { hoverRow = row; hoverCol = col; repaint(); }
}

void SequencerPanel::mouseExit (const juce::MouseEvent&)
{
    hoverRow = NoRow; hoverCol = -1; repaint();
}
} // namespace dali::ui
