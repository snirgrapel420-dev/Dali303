#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

#include "FactoryPresets.h"
#include "Plugin/PatternStore.h"

namespace dali
{
/**
    Preset browser model (message thread only, except getCurrentInfo()).

    A preset = every sound parameter + the 16-step pattern. Factory presets
    are compiled in; user presets live as small XML files (.dali303) in
        Documents/Dali Audio/Dali303/Presets

    DISCOVER creates a deterministic musical variation of the current sound
    and pattern (same starting point + same press count = same result).
*/
class PresetManager : public juce::ChangeBroadcaster,
                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    struct Entry
    {
        juce::String name, category;
        bool isFactory = true;
        int factoryIndex = -1;
        juce::File file;
    };

    struct Info
    {
        juce::String name, category;
        bool modified = false;
    };

    PresetManager (juce::AudioProcessorValueTreeState& state, PatternStore& patterns);
    ~PresetManager() override;

    const std::vector<Entry>& getEntries() const noexcept { return entries; }
    int  getCurrentIndex() const noexcept                 { return currentIndex; }
    Info getCurrentInfo() const;                           // thread-safe (used by getStateInformation)
    bool isModified() const noexcept;
    bool isCurrentFactory() const noexcept;
    juce::String getLastDiscoverDirection() const         { return lastDirection; }

    bool load (int index);
    void next();
    void previous();

    /** Overwrites the current user preset. Returns false for factory presets (use saveAs). */
    bool save();
    bool saveAs (const juce::String& name, const juce::String& category = "User");
    void rescan();

    /** Called after host state restore: re-attach name/category without touching the sound. */
    void restoreIdentity (const juce::String& name, const juce::String& category, bool modified);

    /** DISCOVER: evolve the current acid in a musical direction. */
    void discover();

    static juce::File getUserPresetFolder();
    static constexpr const char* fileExtension = ".dali303";

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    void setParam (const juce::String& id, float plainValue);
    float getParam (const juce::String& id) const;
    void setIdentity (const juce::String& name, const juce::String& category, bool modified);
    void markClean();
    bool loadFactory (int factoryIndex);
    bool loadUserFile (const juce::File& file);
    std::unique_ptr<juce::XmlElement> createPresetXml (const juce::String& name, const juce::String& category) const;
    int findEntry (const juce::String& name, const juce::String& category) const;

    juce::AudioProcessorValueTreeState& apvts;
    PatternStore& patternStore;

    std::vector<Entry> entries;
    int currentIndex = -1;

    mutable juce::CriticalSection infoLock;
    juce::String currentName { "Init" }, currentCategory { "User" };

    std::atomic<bool> paramsDirty { false };
    std::atomic<bool> suppressDirty { false };
    uint32_t cleanPatternVersion = 0;

    uint32_t discoverCounter = 0;
    juce::String lastDirection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
} // namespace dali
