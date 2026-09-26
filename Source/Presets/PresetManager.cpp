#include "PresetManager.h"
#include "Discover.h"
#include "Plugin/Parameters.h"
#include "DSP/DspCommon.h"

namespace dali
{
namespace
{
    SoundValues readSound (juce::AudioProcessorValueTreeState& s)
    {
        auto v = [&s] (const char* id) { return s.getRawParameterValue (id)->load(); };
        SoundValues r;
        r.tune = v (params::id::tune);   r.cutoff = v (params::id::cutoff); r.resonance = v (params::id::resonance);
        r.envMod = v (params::id::envMod); r.decay = v (params::id::decay); r.accent = v (params::id::accent);
        r.slide = v (params::id::slide); r.drive = v (params::id::drive);   r.life = v (params::id::life);
        r.outputDb = v (params::id::output); r.wave = (int) v (params::id::wave);
        return r;
    }
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state, PatternStore& patterns)
    : apvts (state), patternStore (patterns)
{
    for (const auto& id : params::soundParameterIds())
        apvts.addParameterListener (id, this);
    rescan();
}

PresetManager::~PresetManager()
{
    for (const auto& id : params::soundParameterIds())
        apvts.removeParameterListener (id, this);
}

// Can be called from the audio thread (automation): only touches an atomic.
void PresetManager::parameterChanged (const juce::String&, float)
{
    if (! suppressDirty.load (std::memory_order_relaxed))
        paramsDirty.store (true, std::memory_order_relaxed);
}

juce::File PresetManager::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Dali Audio").getChildFile ("Dali303").getChildFile ("Presets");
}

void PresetManager::rescan()
{
    const auto info = getCurrentInfo();
    entries.clear();

    // Factory presets in category order
    const auto* fps = getFactoryPresets();
    for (std::size_t c = 0; c < getNumPresetCategories(); ++c)
        for (std::size_t i = 0; i < getNumFactoryPresets(); ++i)
            if (juce::String (fps[i].category) == getPresetCategories()[c])
                entries.push_back ({ fps[i].name, fps[i].category, true, (int) i, {} });

    // User presets
    auto folder = getUserPresetFolder();
    juce::Array<juce::File> files;
    if (folder.isDirectory())
        files = folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
    files.sort();
    for (const auto& f : files)
    {
        if (auto xml = juce::XmlDocument::parse (f))
            if (xml->hasTagName ("Dali303Preset"))
                entries.push_back ({ xml->getStringAttribute ("name", f.getFileNameWithoutExtension()),
                                     xml->getStringAttribute ("category", "User"), false, -1, f });
    }

    currentIndex = findEntry (info.name, info.category);
    sendChangeMessage();
}

int PresetManager::findEntry (const juce::String& name, const juce::String& category) const
{
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].name == name && entries[(size_t) i].category == category)
            return i;
    return -1;
}

PresetManager::Info PresetManager::getCurrentInfo() const
{
    const juce::ScopedLock sl (infoLock);
    return { currentName, currentCategory, isModified() };
}

bool PresetManager::isModified() const noexcept
{
    return paramsDirty.load (std::memory_order_relaxed) || patternStore.getVersion() != cleanPatternVersion;
}

bool PresetManager::isCurrentFactory() const noexcept
{
    return currentIndex >= 0 && entries[(size_t) currentIndex].isFactory;
}

void PresetManager::setIdentity (const juce::String& name, const juce::String& category, bool modified)
{
    {
        const juce::ScopedLock sl (infoLock);
        currentName = name;
        currentCategory = category;
    }
    if (! modified) markClean();
    else paramsDirty.store (true);
}

void PresetManager::markClean()
{
    paramsDirty.store (false);
    cleanPatternVersion = patternStore.getVersion();
}

void PresetManager::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        const float norm = p->convertTo0to1 (plainValue);
        p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        p->endChangeGesture();
    }
}

float PresetManager::getParam (const juce::String& id) const
{
    return apvts.getRawParameterValue (id)->load();
}

// ---------------------------------------------------------------------------
bool PresetManager::load (int index)
{
    if (index < 0 || index >= (int) entries.size())
        return false;

    const auto entry = entries[(size_t) index];
    suppressDirty.store (true);
    const bool ok = entry.isFactory ? loadFactory (entry.factoryIndex) : loadUserFile (entry.file);
    suppressDirty.store (false);

    if (ok)
    {
        currentIndex = index;
        setIdentity (entry.name, entry.category, false);
        discoverCounter = 0;
        lastDirection = {};
    }
    sendChangeMessage();
    return ok;
}

bool PresetManager::loadFactory (int i)
{
    if (i < 0 || i >= (int) getNumFactoryPresets())
        return false;

    const auto& fp = getFactoryPresets()[i];
    using namespace params;
    setParam (id::tune, fp.tune);       setParam (id::cutoff, fp.cutoff);   setParam (id::resonance, fp.resonance);
    setParam (id::envMod, fp.envMod);   setParam (id::decay, fp.decay);     setParam (id::accent, fp.accent);
    setParam (id::slide, fp.slide);     setParam (id::drive, fp.drive);     setParam (id::life, fp.life);
    setParam (id::output, fp.outputDb); setParam (id::wave, (float) fp.wave);

    Pattern p;
    if (patternFromString (fp.pattern, p))
        patternStore.write (p);
    return true;
}

bool PresetManager::loadUserFile (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("Dali303Preset"))
        return false;

    if (auto* ps = xml->getChildByName ("Params"))
        for (auto* e : ps->getChildWithTagNameIterator ("P"))
        {
            const auto id = e->getStringAttribute ("id");
            if (params::soundParameterIds().contains (id))
                setParam (id, (float) e->getDoubleAttribute ("v"));
        }

    if (auto* pe = xml->getChildByName ("Pattern"))
    {
        Pattern p;
        if (patternFromString (pe->getStringAttribute ("data").toStdString(), p))
            patternStore.write (p);
    }
    return true;
}

void PresetManager::next()
{
    if (entries.empty()) return;
    load (currentIndex < 0 ? 0 : (currentIndex + 1) % (int) entries.size());
}

void PresetManager::previous()
{
    if (entries.empty()) return;
    const int n = (int) entries.size();
    load (currentIndex < 0 ? n - 1 : (currentIndex - 1 + n) % n);
}

// ---------------------------------------------------------------------------
std::unique_ptr<juce::XmlElement> PresetManager::createPresetXml (const juce::String& name, const juce::String& category) const
{
    auto xml = std::make_unique<juce::XmlElement> ("Dali303Preset");
    xml->setAttribute ("name", name);
    xml->setAttribute ("category", category);
    xml->setAttribute ("version", 1);

    auto* ps = xml->createNewChildElement ("Params");
    for (const auto& id : params::soundParameterIds())
    {
        auto* e = ps->createNewChildElement ("P");
        e->setAttribute ("id", id);
        e->setAttribute ("v", (double) getParam (id));
    }
    xml->createNewChildElement ("Pattern")->setAttribute ("data", juce::String (patternToString (patternStore.read())));
    return xml;
}

bool PresetManager::save()
{
    if (currentIndex < 0 || entries[(size_t) currentIndex].isFactory)
        return false;

    const auto& e = entries[(size_t) currentIndex];
    auto xml = createPresetXml (e.name, e.category);
    if (! xml->writeTo (e.file))
        return false;

    markClean();
    sendChangeMessage();
    return true;
}

bool PresetManager::saveAs (const juce::String& rawName, const juce::String& category)
{
    const auto name = rawName.trim().replace ("*", "").trim();
    if (name.isEmpty())
        return false;

    auto folder = getUserPresetFolder();
    if (! folder.createDirectory())
        return false;

    const auto file = folder.getChildFile (juce::File::createLegalFileName (name) + fileExtension);
    auto xml = createPresetXml (name, category);
    if (! xml->writeTo (file))
        return false;

    setIdentity (name, category, false);
    rescan();                                  // re-finds the new entry by name + category
    return true;
}

void PresetManager::restoreIdentity (const juce::String& name, const juce::String& category, bool modified)
{
    setIdentity (name.isNotEmpty() ? name : juce::String ("Init"),
                 category.isNotEmpty() ? category : juce::String ("User"), modified);
    currentIndex = findEntry (name, category);
    sendChangeMessage();
}

// ---------------------------------------------------------------------------
void PresetManager::discover()
{
    const auto sound = readSound (apvts);
    const auto pattern = patternStore.read();

    // Deterministic: seed = press counter x current pattern x current sound.
    ++discoverCounter;
    const uint32_t seed = dsp::hashCombine (dsp::hashCombine (discoverCounter, pattern.hash()), sound.hash());
    const auto r = dali::discover (sound, pattern, seed);

    using namespace params;
    setParam (id::cutoff, r.sound.cutoff);  setParam (id::resonance, r.sound.resonance);
    setParam (id::envMod, r.sound.envMod);  setParam (id::decay, r.sound.decay);
    setParam (id::accent, r.sound.accent);  setParam (id::slide, r.sound.slide);
    setParam (id::drive, r.sound.drive);    setParam (id::life, r.sound.life);
    setParam (id::output, r.sound.outputDb);
    patternStore.write (r.pattern);

    lastDirection = discoverDirectionName (r.direction);
    paramsDirty.store (true);
    sendChangeMessage();
}
} // namespace dali
