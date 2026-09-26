#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DaliLookAndFeel.h"

namespace dali
{
class Dali303Processor;

namespace ui { class MainPanel; }

/**
    Dali303 editor. The whole face is laid out on a fixed 1100 x 720 design
    canvas (MainPanel) that is scaled as one piece when the window is resized,
    so proportions — and the hardware feel — never break.
*/
class Dali303Editor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit Dali303Editor (Dali303Processor&);
    ~Dali303Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth = 1100, designHeight = 720;

private:
    void timerCallback() override;

    ui::DaliLookAndFeel lookAndFeel;
    std::unique_ptr<ui::MainPanel> panel;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Dali303Editor)
};
} // namespace dali
