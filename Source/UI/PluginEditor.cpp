#include "PluginEditor.h"
#include "Plugin/PluginProcessor.h"
#include "Plugin/Parameters.h"
#include "DaliKnob.h"
#include "PresetBar.h"
#include "SequencerPanel.h"

namespace dali
{
namespace ui
{
using namespace colours;

// ============================================================================
//  Vertical peak meter
// ============================================================================
class OutputMeter : public juce::Component
{
public:
    void setPeak (float linearPeak)
    {
        const float db = juce::Decibels::gainToDecibels (linearPeak, -60.0f);
        const float target = juce::jmap (db, -48.0f, 3.0f, 0.0f, 1.0f);
        level = target > level ? target : level - 0.035f;            // fast attack, smooth fall
        level = juce::jlimit (0.0f, 1.0f, level);
        if (target > hold) { hold = target; holdFrames = 30; }
        else if (--holdFrames < 0) hold = juce::jmax (0.0f, hold - 0.02f);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (groove);
        g.fillRoundedRectangle (r, 3.0f);
        auto inner = r.reduced (3.0f);

        // segmented LED column
        const int segs = 24;
        const float segH = inner.getHeight() / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            const float t = (float) (i + 1) / (float) segs;
            auto s = juce::Rectangle<float> (inner.getX(), inner.getBottom() - (float) (i + 1) * segH, inner.getWidth(), segH - 1.5f);
            const auto hue = purple.interpolatedWith (magenta, t);
            const bool lit = t <= level + 1.0e-4f;
            g.setColour (lit ? hue : hue.withAlpha (0.08f));
            g.fillRoundedRectangle (s, 1.0f);
        }
        if (hold > 0.02f)
        {
            const float y = inner.getBottom() - hold * inner.getHeight();
            g.setColour (magenta);
            g.fillRect (inner.getX(), y, inner.getWidth(), 1.5f);
        }
    }

private:
    float level = 0, hold = 0;
    int holdFrames = 0;
};

// ============================================================================
//  The instrument face (1100 x 720 design canvas)
// ============================================================================
class MainPanel : public juce::Component
{
public:
    MainPanel (Dali303Processor& p)
        : proc (p),
          presetBar (p.getPresetManager()),
          sequencer (p.getState(), p.getPatternStore(), p.isStandalone()),
          cutoff    (p.getState(), params::id::cutoff,    "CUTOFF"),
          resonance (p.getState(), params::id::resonance, "RESONANCE"),
          envMod    (p.getState(), params::id::envMod,    "ENV"),
          decay     (p.getState(), params::id::decay,     "DECAY"),
          accent    (p.getState(), params::id::accent,    "ACCENT"),
          slide     (p.getState(), params::id::slide,     "SLIDE"),
          drive     (p.getState(), params::id::drive,     "DRIVE"),
          tune      (p.getState(), params::id::tune,      "TUNE", DaliKnob::Style::Normal, true),
          life      (p.getState(), params::id::life,      "LIFE", DaliKnob::Style::Hero),
          output    (p.getState(), params::id::output,    "OUTPUT", DaliKnob::Style::Small)
    {
        for (auto* k : { &cutoff, &resonance, &envMod, &decay, &accent, &slide, &drive, &tune, &life, &output })
            addAndMakeVisible (k);
        addAndMakeVisible (presetBar);
        addAndMakeVisible (sequencer);
        addAndMakeVisible (meter);

        cutoff.getSlider().setTooltip ("Filter cutoff. CC74 modulates it.");
        resonance.getSlider().setTooltip ("Resonance: also changes the filter's drive, harmonics and envelope response. CC71.");
        envMod.getSlider().setTooltip ("How far the envelope sweeps the filter.");
        decay.getSlider().setTooltip ("Filter envelope decay. Accented notes decay faster.");
        accent.getSlider().setTooltip ("Accent depth: sweep, resonance, punch and circuit heat on accented steps.");
        slide.getSlider().setTooltip ("Slide time. Short slides snap, long slides melt the filter along.");
        drive.getSlider().setTooltip ("Pushes the Acid Circuit: clean > warm > acid > aggressive > screaming.");
        tune.getSlider().setTooltip ("Tune (semitones).");
        life.getSlider().setTooltip ("LIFE: 0 = stable classic. Higher = the circuit reacts to the musical context. Deterministic.");
        output.getSlider().setTooltip ("Output level.");

        // --- waveform segmented switch ---
        for (auto* b : { &sawButton, &squareButton })
        {
            b->setClickingTogglesState (false);
            addAndMakeVisible (b);
        }
        if (auto* wp = p.getState().getParameter (params::id::wave))
        {
            waveAttachment = std::make_unique<juce::ParameterAttachment> (*wp, [this] (float v)
            {
                sawButton.setToggleState ((int) v == 0, juce::dontSendNotification);
                squareButton.setToggleState ((int) v == 1, juce::dontSendNotification);
            });
            waveAttachment->sendInitialUpdate();
        }
        sawButton.onClick    = [this] { if (waveAttachment) waveAttachment->setValueAsCompleteGesture (0.0f); };
        squareButton.onClick = [this] { if (waveAttachment) waveAttachment->setValueAsCompleteGesture (1.0f); };

        // --- oversampling ---
        quality.addItemList ({ "1x", "2x", "4x" }, 1);
        quality.setTooltip ("Oversampling of the Acid Circuit (filter + drive only). 2x is the sweet spot.");
        addAndMakeVisible (quality);
        qualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            p.getState(), params::id::oversampling, quality);

       #if DALI303_DEV_MODE
        for (int i = 0; i < dsp::kNumDevModes; ++i)
            devMode.addItem (dsp::devModeName ((dsp::DevMode) i), i + 1);
        devMode.setSelectedId (p.getDevMode() + 1, juce::dontSendNotification);
        devMode.onChange = [this] { proc.setDevMode (devMode.getSelectedId() - 1); };
        devMode.setTooltip ("Developer Test Mode: isolate engine sections");
        addAndMakeVisible (devMode);
       #endif

        setSize (Dali303Editor::designWidth, Dali303Editor::designHeight);
    }

    ~MainPanel() override = default;

    void tick()
    {
        sequencer.update (proc.getPlayingStep());
        presetBar.refresh();
        meter.setPeak (proc.consumeOutputPeak());

        // LIFE breathes: speed and glow follow the LIFE amount (visual only)
        const float lv = life.getNormalisedValue();
        lifePhase += 0.02f + 0.10f * lv;
        if (lifePhase > juce::MathConstants<float>::twoPi) lifePhase -= juce::MathConstants<float>::twoPi;
        repaint (lifeArea.toNearestInt());

       #if DALI303_DEV_MODE
        cpuText = "CPU " + juce::String (proc.getCpuLoad() * 100.0f, 1) + " %";
        repaint (cpuArea);
       #endif
    }

    void resized() override
    {
        presetBar.setBounds (440, 26, 640, 40);

        // ACID CIRCUIT: 4 x 2 hardware grid
        circuitArea = { 20.0f, 98.0f, 620.0f, 336.0f };
        const juce::Rectangle<int> knobs (34, 138, 592, 290);
        const int cw = knobs.getWidth() / 4, ch = knobs.getHeight() / 2;
        DaliKnob* grid[2][4] = { { &cutoff, &resonance, &envMod, &decay }, { &accent, &slide, &drive, &tune } };
        for (int r = 0; r < 2; ++r)
            for (int c = 0; c < 4; ++c)
                grid[r][c]->setBounds (juce::Rectangle<int> (knobs.getX() + c * cw, knobs.getY() + r * ch, cw, ch).reduced (6, 2));

        // LIFE hero
        lifeArea = { 652.0f, 98.0f, 236.0f, 336.0f };
        life.setBounds (juce::Rectangle<int> (672, 150, 196, 236));

        // OUTPUT column
        outputArea = { 900.0f, 98.0f, 180.0f, 336.0f };
        output.setBounds (912, 132, 110, 128);
        meter.setBounds (1040, 142, 18, 106);
        sawButton.setBounds (916, 300, 72, 26);
        squareButton.setBounds (992, 300, 72, 26);
        quality.setBounds (916, 360, 148, 26);
       #if DALI303_DEV_MODE
        devMode.setBounds (916, 400, 148, 22);
        cpuArea = { 916, 72, 160, 16 };
       #endif

        sequencer.setBounds (20, 446, 1060, 258);
    }

    void paint (juce::Graphics& g) override
    {
        // --- chassis -------------------------------------------------------
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0b11), 0, 0, background, 0, (float) getHeight(), false));
        g.fillAll();
        g.setColour (juce::Colours::white.withAlpha (0.012f));
        for (int y = 0; y < getHeight(); y += 3)                     // brushed texture
            g.drawHorizontalLine (y, 0.0f, (float) getWidth());

        // --- header --------------------------------------------------------
        g.setFont (font (11.0f, true, 0.55f));
        g.setColour (purple.withAlpha (0.9f));
        g.drawText ("DALI AUDIO", 30, 18, 200, 14, juce::Justification::centredLeft);

        g.setFont (font (34.0f, true, 0.02f));
        g.setColour (text);
        g.drawText ("Dali", 28, 32, 80, 40, juce::Justification::centredLeft);
        juce::GlyphArrangement ga;
        ga.addLineOfText (font (34.0f, true, 0.02f), "Dali", 0.0f, 0.0f);
        const float dW = ga.getBoundingBox (0, -1, true).getWidth();
        g.setGradientFill (juce::ColourGradient (purple, 28.0f + dW, 40.0f, magenta, 28.0f + dW + 70.0f, 64.0f, false));
        g.drawText ("303", (int) (30.0f + dW), 32, 90, 40, juce::Justification::centredLeft);

        g.setFont (font (10.0f, true, 0.35f));
        g.setColour (textDim);
        g.drawText ("A LIVING ACID MACHINE", 186, 46, 240, 14, juce::Justification::centredLeft);

        g.setGradientFill (juce::ColourGradient (purple.withAlpha (0.0f), 20.0f, 0, purple.withAlpha (0.45f), 550.0f, 0, false));
        g.fillRect (20.0f, 84.0f, 530.0f, 1.0f);
        g.setGradientFill (juce::ColourGradient (magenta.withAlpha (0.45f), 550.0f, 0, magenta.withAlpha (0.0f), 1080.0f, 0, false));
        g.fillRect (550.0f, 84.0f, 530.0f, 1.0f);

        // --- panels --------------------------------------------------------
        drawPanel (g, circuitArea, 12.0f);
        drawSectionTitle (g, "DALI ACID CIRCUIT", circuitArea.reduced (16.0f, 10.0f).withHeight (24.0f));
        screws (g, circuitArea);

        drawLifePanel (g);

        drawPanel (g, outputArea, 12.0f);
        drawSectionTitle (g, "OUTPUT", outputArea.reduced (16.0f, 10.0f).withHeight (24.0f));
        g.setFont (font (9.5f, true, 0.25f));
        g.setColour (textDim);
        g.drawText ("WAVE", 916, 280, 148, 16, juce::Justification::centredLeft);
        g.drawText ("CIRCUIT QUALITY", 916, 340, 148, 16, juce::Justification::centredLeft);
        g.drawText ("LEVEL", 1030, 250, 40, 14, juce::Justification::centred);

        // --- footer ---------------------------------------------------------
        g.setFont (font (9.0f, false, 0.3f));
        g.setColour (textFaint);
        const juce::String dot (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
        g.drawText ("DALI AUDIO" + dot + "DALI303" + dot + "V0.1", 20, 704, 1060, 14, juce::Justification::centredRight);

       #if DALI303_DEV_MODE
        g.setFont (font (10.0f, true, 0.1f));
        g.setColour (magenta.withAlpha (0.8f));
        g.drawText ("DEV " + cpuText, cpuArea, juce::Justification::centredRight);
       #endif
    }

private:
    void screws (juce::Graphics& g, juce::Rectangle<float> r)
    {
        for (auto p : { r.getTopLeft() + juce::Point<float> (10, 10), r.getTopRight() + juce::Point<float> (-10, 10),
                        r.getBottomLeft() + juce::Point<float> (10, -10), r.getBottomRight() + juce::Point<float> (-10, -10) })
        {
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (p + juce::Point<float> (0, 0.8f)));
            g.setColour (metalLight.withAlpha (0.8f));
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (p));
            g.setColour (groove);
            g.drawLine (p.x - 1.8f, p.y - 1.0f, p.x + 1.8f, p.y + 1.0f, 0.9f);
        }
    }

    void drawLifePanel (juce::Graphics& g)
    {
        drawPanel (g, lifeArea, 12.0f);

        // breathing neon halo behind the hero knob, scaled by LIFE
        const float lv = life.getNormalisedValue();
        const float breath = 0.85f + 0.15f * std::sin (lifePhase);
        const auto c = life.getBounds().toFloat().withTrimmedBottom (46.0f).getCentre();
        const float radius = 128.0f;
        juce::ColourGradient halo (magenta.withAlpha ((0.05f + 0.22f * lv) * breath), c.x, c.y,
                                   purple.withAlpha (0.0f), c.x + radius, c.y, true);
        halo.addColour (0.55, purple.withAlpha ((0.03f + 0.10f * lv) * breath));
        g.setGradientFill (halo);
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c));

        // neon border glows with LIFE
        g.setColour (magenta.withAlpha (0.15f + 0.45f * lv * breath));
        g.drawRoundedRectangle (lifeArea.reduced (0.5f), 12.0f, 1.2f);

        drawSectionTitle (g, "LIFE ENGINE", lifeArea.reduced (16.0f, 10.0f).withHeight (24.0f), magenta);
        g.setFont (font (9.0f, true, 0.22f));
        g.setColour (textDim);
        g.drawText ("CLASSIC", juce::Rectangle<float> (lifeArea.getX() + 14.0f, lifeArea.getBottom() - 34.0f, 90.0f, 14.0f),
                    juce::Justification::centredLeft);
        g.drawText ("ALIVE", juce::Rectangle<float> (lifeArea.getRight() - 104.0f, lifeArea.getBottom() - 34.0f, 90.0f, 14.0f),
                    juce::Justification::centredRight);
        auto bar = juce::Rectangle<float> (lifeArea.getX() + 14.0f, lifeArea.getBottom() - 16.0f, lifeArea.getWidth() - 28.0f, 3.0f);
        g.setColour (groove);
        g.fillRoundedRectangle (bar, 1.5f);
        g.setGradientFill (juce::ColourGradient (purple, bar.getX(), 0, magenta, bar.getRight(), 0, false));
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (3.0f, bar.getWidth() * lv)), 1.5f);
    }

    Dali303Processor& proc;
    PresetBar presetBar;
    SequencerPanel sequencer;
    DaliKnob cutoff, resonance, envMod, decay, accent, slide, drive, tune, life, output;
    OutputMeter meter;

    juce::TextButton sawButton { "SAW" }, squareButton { "SQUARE" };
    std::unique_ptr<juce::ParameterAttachment> waveAttachment;
    juce::ComboBox quality;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityAttachment;

   #if DALI303_DEV_MODE
    juce::ComboBox devMode;
    juce::String cpuText;
    juce::Rectangle<int> cpuArea;
   #endif

    juce::Rectangle<float> circuitArea, lifeArea, outputArea;
    float lifePhase = 0.0f;
};
} // namespace ui

// ============================================================================
Dali303Editor::Dali303Editor (Dali303Processor& p)
    : AudioProcessorEditor (p)
{
    setLookAndFeel (&lookAndFeel);
    panel = std::make_unique<ui::MainPanel> (p);
    addAndMakeVisible (*panel);

    setResizable (true, true);
    setResizeLimits (designWidth * 7 / 10, designHeight * 7 / 10, designWidth * 3 / 2, designHeight * 3 / 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    setSize (designWidth, designHeight);

    startTimerHz (30);
}

Dali303Editor::~Dali303Editor()
{
    stopTimer();
    panel.reset();
    setLookAndFeel (nullptr);
}

void Dali303Editor::paint (juce::Graphics& g)
{
    g.fillAll (ui::colours::background);
}

void Dali303Editor::resized()
{
    const float scale = (float) getWidth() / (float) designWidth;
    panel->setBounds (0, 0, designWidth, designHeight);
    panel->setTransform (juce::AffineTransform::scale (scale));
}

void Dali303Editor::timerCallback()
{
    panel->tick();
}
} // namespace dali
