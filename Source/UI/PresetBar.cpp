#include "PresetBar.h"

namespace dali::ui
{
PresetBar::PresetBar (PresetManager& pm) : presets (pm)
{
    for (auto* b : { &prevButton, &nextButton, &nameButton, &saveButton, &saveAsButton, &discoverButton })
        addAndMakeVisible (b);

    nameButton.setAlpha (0.0f);            // invisible hit area — the display is painted by the bar itself
    nameButton.onStateChange = [this] { repaint(); };
    discoverButton.getProperties().set ("dali_hero", true);

    prevButton.setTooltip ("Previous preset");
    nextButton.setTooltip ("Next preset");
    nameButton.setTooltip ("Browse presets");
    saveButton.setTooltip ("Save over the current user preset");
    saveAsButton.setTooltip ("Save as a new user preset");
    discoverButton.setTooltip ("Evolve this acid in a new musical direction (sound + pattern)");

    prevButton.onClick     = [this] { presets.previous(); };
    nextButton.onClick     = [this] { presets.next(); };
    nameButton.onClick     = [this] { showPresetMenu(); };
    saveButton.onClick     = [this] { if (! presets.save()) showSaveAsDialog(); };
    saveAsButton.onClick   = [this] { showSaveAsDialog(); };
    discoverButton.onClick = [this] { presets.discover(); };

    presets.addChangeListener (this);
    refresh();
}

PresetBar::~PresetBar()
{
    presets.removeChangeListener (this);
}

void PresetBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    discoverNote = presets.getLastDiscoverDirection();
    refresh();
    repaint();
}

void PresetBar::refresh()
{
    const auto info = presets.getCurrentInfo();
    if (info.name != shownName || info.category != shownCategory || info.modified != shownModified)
    {
        shownName = info.name;
        shownCategory = info.category;
        shownModified = info.modified;
        saveButton.setEnabled (! presets.isCurrentFactory() && presets.getCurrentIndex() >= 0);
        repaint();
    }
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    discoverButton.setBounds (r.removeFromRight (122));
    r.removeFromRight (10);
    saveAsButton.setBounds (r.removeFromRight (78));
    r.removeFromRight (6);
    saveButton.setBounds (r.removeFromRight (58));
    r.removeFromRight (12);
    prevButton.setBounds (r.removeFromLeft (34));
    nextButton.setBounds (r.removeFromRight (34));
    nameButton.setBounds (r.reduced (4, 0));
}

void PresetBar::paint (juce::Graphics& g)
{
    // LCD-like preset display between < and >
    auto d = nameButton.getBounds().toFloat();
    g.setColour (colours::groove);
    g.fillRoundedRectangle (d, 5.0f);
    g.setColour (nameButton.isMouseOver() ? colours::purpleDim.brighter (0.3f) : colours::panelEdge);
    g.drawRoundedRectangle (d.reduced (0.5f), 5.0f, 1.0f);

    auto t = d.reduced (12.0f, 3.0f);
    g.setFont (font (9.5f, true, 0.25f));
    g.setColour (colours::purple.withAlpha (0.85f));
    auto cat = shownCategory.toUpperCase();
    if (discoverNote.isNotEmpty() && shownModified)
        cat << "   " << juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) << "   DISCOVER: " << discoverNote.toUpperCase();
    g.drawText (cat, t.removeFromTop (t.getHeight() * 0.42f),
                juce::Justification::bottomLeft);

    g.setFont (font (16.0f, true, 0.03f));
    g.setColour (colours::text);
    g.drawText (shownName + (shownModified ? " *" : ""), t, juce::Justification::centredLeft);

    juce::Path arrow;
    const float ax = d.getRight() - 14.0f, ay = d.getCentreY();
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (colours::purple);
    g.fillPath (arrow);
}

void PresetBar::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto& entries = presets.getEntries();
    const int current = presets.getCurrentIndex();

    juce::StringArray categories;
    for (const auto& e : entries)
        categories.addIfNotAlreadyThere (e.category);

    bool addedUserHeader = false;
    for (const auto& cat : categories)
    {
        juce::PopupMenu sub;
        bool containsCurrent = false, isUser = false;
        for (int i = 0; i < (int) entries.size(); ++i)
            if (entries[(size_t) i].category == cat)
            {
                sub.addItem (i + 1, entries[(size_t) i].name, true, i == current);
                containsCurrent |= (i == current);
                isUser |= ! entries[(size_t) i].isFactory;
            }
        if (isUser && ! addedUserHeader) { menu.addSeparator(); menu.addSectionHeader ("USER"); addedUserHeader = true; }
        menu.addSubMenu (cat, sub, true, nullptr, containsCurrent);
    }

    menu.addSeparator();
    constexpr int rescanId = 100000, folderId = 100001;
    menu.addItem (rescanId, "Rescan user presets");
    menu.addItem (folderId, "Open user preset folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton),
                        [safe = juce::Component::SafePointer<PresetBar> (this)] (int result)
                        {
                            if (safe == nullptr || result == 0) return;
                            if (result == rescanId)       safe->presets.rescan();
                            else if (result == folderId)
                            {
                                auto folder = PresetManager::getUserPresetFolder();
                                folder.createDirectory();
                                folder.startAsProcess();
                            }
                            else safe->presets.load (result - 1);
                        });
}

void PresetBar::showSaveAsDialog()
{
    auto base = presets.getCurrentInfo().name.replace ("*", "").trim();
    saveDialog = std::make_unique<juce::AlertWindow> ("Save Preset", "Name your acid:", juce::MessageBoxIconType::NoIcon);
    saveDialog->setLookAndFeel (&getLookAndFeel());
    saveDialog->addTextEditor ("name", base, "Name");
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    auto safe = juce::Component::SafePointer<PresetBar> (this);
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->saveDialog == nullptr) return;
        if (result == 1)
        {
            const auto name = safe->saveDialog->getTextEditorContents ("name");
            if (! safe->presets.saveAs (name))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Dali303",
                                                        "Could not save the preset.");
        }
        // destroy the dialog after its callback has returned
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->saveDialog.reset(); });
    }), false);
}
} // namespace dali::ui
