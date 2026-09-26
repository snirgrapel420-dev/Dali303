#include "FactoryPresets.h"

namespace dali
{
namespace
{
    const char* const categories[] = {
        "Classic Acid", "Deep Acid", "Goa Acid", "Acid Trance", "Devil Fish Inspired",
        "Dark Acid", "Screaming", "Industrial Acid", "Psychedelic", "Dali Originals", "User"
    };

    //   name                category              tune  cut   res   env   dec   acc   sld   drv   life  outDb wave pattern
    const FactoryPreset presets[] = {
        { "First Light",      "Classic Acid",        0.f, .30f, .74f, .58f, .36f, .70f, .34f, .18f, .20f, -3.f, 0,
          "A1 A1 A2A A1 C2S D2 A1 G1A A1 A1S A2 A1 E2A D2S C2 A1" },
        { "Rubber Line",      "Classic Acid",        0.f, .26f, .80f, .64f, .30f, .62f, .30f, .22f, .15f, -3.f, 0,
          "C2 C2S C3 C2A - D#2 C2 C2AS G2 C2 A#1S C2 C3A C2 - F2S" },
        { "Night Shift",      "Classic Acid",        0.f, .34f, .66f, .50f, .42f, .55f, .40f, .12f, .10f, -4.5f, 1,
          "E1 E1 E2A E1 G1S A1 E1 - E1 D2A E1 B1S E1 E1 G1A A1" },
        { "Subsurface",       "Deep Acid",           0.f, .18f, .68f, .46f, .55f, .50f, .52f, .20f, .30f, -1.5f, 0,
          "F1 - F1S G#1 F1 - C2A F1 F1 - F1S D#1 F1 - G#1A F1" },
        { "Pressure Drop",    "Deep Acid",           0.f, .15f, .76f, .52f, .48f, .72f, .44f, .30f, .25f, -3.0f, 0,
          "D1 D1 - D1A F1S D1 - D1 C2A D1 - D1 D1S F1 G1A D1" },
        { "Deep Current",     "Deep Acid",          -2.f, .22f, .62f, .40f, .62f, .45f, .60f, .15f, .40f, -5.0f, 1,
          "G1S A#1 G1 - G1 D2AS C2 G1 - G1S F1 G1A - G1 A#1S G1" },
        { "Kali Yuga",        "Goa Acid",            0.f, .36f, .84f, .66f, .26f, .75f, .28f, .35f, .35f, -4.f, 0,
          "E1 E2 E1 F2A E1 E2S G2 E1 E1 E2A E1 A#1 E1 F2S E2 B1A" },
        { "Temple Run",       "Goa Acid",            0.f, .32f, .80f, .70f, .22f, .68f, .25f, .28f, .30f, -3.0f, 0,
          "D1 D2 D1 D#2 D1 D2A D1 A1 D1 D2 D1S D#2 F2A D1 D2 C2S" },
        { "Mandala Engine",   "Goa Acid",            0.f, .40f, .86f, .60f, .30f, .70f, .36f, .40f, .55f, -4.0f, 0,
          "A1 A2 E2 A1A C3 A1 G2S A2 A1 A2A E2 A1 D#2 A1S A2 F2A" },
        { "Supernova",        "Acid Trance",         0.f, .44f, .78f, .56f, .40f, .60f, .45f, .30f, .20f, -3.0f, 0,
          "A1 A1 A2A A1 A1 G2S A1 A1 A1 A1 A2A A1 C3S B2 A1 E2A" },
        { "Hands Up High",    "Acid Trance",         0.f, .48f, .72f, .50f, .46f, .58f, .50f, .24f, .15f, -2.5f, 0,
          "F1 F2 F1 C2A F1 F2 G#2S F1 D#1 D#2 D#1 A#1A D#1 D#2 G2S D#1" },
        { "Fishbone",         "Devil Fish Inspired", 0.f, .28f, .90f, .78f, .20f, .85f, .38f, .45f, .35f, -5.f, 0,
          "C2A C2 C3S C2 D#2A C2 - C2S G2A C2 A#1 C2A C3S C2 F2 C2A" },
        { "Hook Line Sinker", "Devil Fish Inspired", 0.f, .24f, .92f, .84f, .34f, .80f, .58f, .50f, .40f, -5.f, 0,
          "G1A G1S D2 G1 - G1AS A#1 G1 F2A G1 - G1S C2A D2S G1 G1A" },
        { "Wet Circuit",      "Devil Fish Inspired", 0.f, .35f, .88f, .72f, .28f, .78f, .42f, .60f, .30f, -7.5f, 1,
          "E1A E1 E2S D2 E1 E1A G1 E1S B1 E1A E1 E2 E1S D2A C2 E1" },
        { "Black Sun",        "Dark Acid",          -1.f, .16f, .82f, .58f, .44f, .66f, .46f, .42f, .30f, -3.f, 0,
          "C#1 - C#1 D1A C#1 - C#1S G#1 C#1 - C#1 D1A - C#1S F#1 C#1A" },
        { "Void Walker",      "Dark Acid",           0.f, .12f, .86f, .62f, .52f, .70f, .55f, .48f, .45f, -6.0f, 1,
          "F#1 F#1S G1 F#1 - F#1A C#2 F#1 - F#1 F#1S G1A F#1 - A1 F#1" },
        { "Scream Theory",    "Screaming",           0.f, .42f, .95f, .82f, .24f, .90f, .32f, .70f, .35f, -8.f, 0,
          "A1A A2 A1 A2AS C3 A1A A2 G2S A1A A2 A1 E2AS D3 A1A A2 C3S" },
        { "Red Line",         "Screaming",           0.f, .46f, .93f, .88f, .18f, .92f, .26f, .80f, .30f, -6.0f, 0,
          "D2A D2 D3S D2 D2A F2 D2 C3S D2A D2 D3 A2S D2A D2 D#3 D2" },
        { "Steel Press",      "Industrial Acid",     0.f, .30f, .78f, .74f, .16f, .88f, .20f, .75f, .20f, -8.f, 1,
          "C1A C1 C1 C2A C1 C1 C#2A C1 C1A C1 C1 C2A C1 G1 C1A C1" },
        { "Rust Protocol",    "Industrial Acid",    -3.f, .26f, .84f, .80f, .22f, .82f, .30f, .85f, .40f, -6.0f, 0,
          "E1A E1 - E1A F1 E1 - E1AS B1 E1A - E1 F1A E1 - E1AS" },
        { "Liquid Mirrors",   "Psychedelic",         0.f, .38f, .80f, .48f, .58f, .55f, .70f, .20f, .70f, -2.5f, 0,
          "B1S D2S F#2 B1 A2S F#2S D2 B1 E2S G2S B1 F#2 D2S C#2S B1 A1" },
        { "Third Eye Drift",  "Psychedelic",         0.f, .33f, .88f, .54f, .50f, .62f, .64f, .35f, .85f, -5.f, 0,
          "D#1 D#2S A#1 D#1 F#2AS D#1 C#2 D#1S G#1 D#2 D#1 F#1S D#1 A#1A C#2 D#1" },
        { "Dali Signature",   "Dali Originals",      0.f, .31f, .83f, .62f, .34f, .76f, .40f, .38f, .50f, -4.f, 0,
          "A1 A1 C2S A1A E2 A1 G1S A1 A1A A2 A1 C2S D2A A1 E1S G1" },
        { "Melting Clocks",   "Dali Originals",     -5.f, .23f, .87f, .60f, .60f, .68f, .75f, .40f, .80f, -5.0f, 0,
          "F1S G#1S C2 F1 - F1AS A#1S C#2 C2 - G#1S F1 G1A F1S E1S F1" },
    };
}

const FactoryPreset* getFactoryPresets() noexcept   { return presets; }
std::size_t getNumFactoryPresets() noexcept          { return sizeof (presets) / sizeof (presets[0]); }
const char* const* getPresetCategories() noexcept    { return categories; }
std::size_t getNumPresetCategories() noexcept        { return sizeof (categories) / sizeof (categories[0]); }
} // namespace dali
