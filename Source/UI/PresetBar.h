#pragma once

#include "Presets/PresetManager.h"
#include "Colours.h"

namespace dali::ui
{
/** Header preset browser:  [<] [ category · name * ] [>]  SAVE  SAVE AS  [DISCOVER] */
class PresetBar : public juce::Component,
                  private juce::ChangeListener
{
public:
    explicit PresetBar (PresetManager& pm);
    ~PresetBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Polled by the editor timer: shows the "modified" marker. */
    void refresh();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showPresetMenu();
    void showSaveAsDialog();

    PresetManager& presets;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, nameButton,
                     saveButton { "SAVE" }, saveAsButton { "SAVE AS" }, discoverButton { "DISCOVER" };
    juce::String shownName, shownCategory, discoverNote;
    bool shownModified = false;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};
} // namespace dali::ui
