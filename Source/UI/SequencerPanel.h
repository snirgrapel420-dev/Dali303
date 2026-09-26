#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Plugin/PatternStore.h"
#include "Colours.h"

namespace dali::ui
{
/**
    16-step sequencer: per step NOTE (drag / wheel), GATE, ACCENT, SLIDE.
    Toggle rows support paint-dragging across steps. Edits go straight to the
    lock-free PatternStore; changes made elsewhere (presets, DISCOVER, host
    state) are picked up through the store's version counter.
*/
class SequencerPanel : public juce::Component
{
public:
    SequencerPanel (juce::AudioProcessorValueTreeState& state, PatternStore& store, bool showInternalClock);

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    /** Editor timer: playhead + external pattern changes. */
    void update (int playingStep);

private:
    enum Row { StepRow, NoteRow, GateRow, AccentRow, SlideRow, NumRows, NoRow = -1 };

    juce::Rectangle<float> cell (int row, int col) const;
    std::pair<int, int> hitTest (juce::Point<float>) const;
    void commit();
    void transpose (int semis);
    void shift (int dir);
    bool getFlag (int row, int col) const;
    void setFlag (int row, int col, bool v);

    PatternStore& store;
    Pattern pattern;
    uint32_t seenVersion = 0;
    int playing = -1;

    juce::TextButton seqButton { "SEQ ON" }, shiftLeft { "<" }, shiftRight { ">" },
                     downSemi { "-1" }, upSemi { "+1" }, downOct { "OCT-" }, upOct { "OCT+" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> seqAttachment;
    juce::Slider bpmSlider;                                   // Standalone only
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttachment;

    juce::Rectangle<float> grid;
    float colW = 0, gap = 0;
    std::array<float, NumRows> rowY {}, rowH {};

    // interaction
    int dragRow = NoRow, dragCol = -1, dragStartNote = 0;
    float dragStartY = 0;
    bool paintValue = false;
    int hoverRow = NoRow, hoverCol = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerPanel)
};
} // namespace dali::ui
