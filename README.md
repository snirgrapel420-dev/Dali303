# Dali303 — A Living Acid Machine
**Dali Audio** · VST3 Instrument (+ Standalone) · C++17 / JUCE 8

303 DNA. Devil Fish attitude. Dali Audio engineering.
Original DSP, original architecture, original GUI — the TB-303 and Devil Fish are
sonic/conceptual references only. No code, samples, schematics or artwork are copied.

---

## Build

Requirements: CMake ≥ 3.22, a C++17 compiler (Visual Studio 2022 on Windows, Xcode 14+ on macOS), git.

```bash
# Windows (VS 2022)
cmake -B build -G "Visual Studio 17 2022" -A x64   # or "Visual Studio 18 2026" (CMake 4.2+)
cmake --build build --config Release

# macOS (universal arm64 + x86_64)
cmake -B build -G Xcode
cmake --build build --config Release
```

JUCE 8.0.4 is downloaded automatically. To use a local checkout: `-DJUCE_DIR=/path/to/JUCE`.

Results:
- VST3: `build/Dali303_artefacts/Release/VST3/Dali303.vst3`
  → copy to `C:\Program Files\Common Files\VST3\` (Windows) or `~/Library/Audio/Plug-Ins/VST3/` (macOS), then rescan plug-ins in Ableton.
- Standalone: `build/Dali303_artefacts/Release/Standalone/`

### GitHub Actions (automatic builds)

`.github/workflows/build.yml` builds everything on every push — no local compiler needed:

1. **DSP tests** (Linux): builds `DaliRender`, runs the engine suite, the preset/DISCOVER stress test and the CPU
   benchmark. Fails on NaN/Inf, clipping above 0 dBFS or non-deterministic LIFE. WAV renders are uploaded.
2. **Build** (Windows x64 + macOS universal): VST3 + Standalone, zipped as `Dali303-Windows` / `Dali303-macOS`
   under the run's *Artifacts*. Then **pluginval** at strictness 10 (state restore, automation, sample rates,
   block sizes, threading); its log is uploaded too.
3. **Release**: push a tag (`git tag v0.1.0 && git push --tags`) → a GitHub Release with both zips.

macOS builds are ad-hoc signed, not notarised. If macOS blocks the plug-in after downloading:
`xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Dali303.vst3`

Options:
- `-DDALI303_DEV_MODE=ON` — Developer Test Mode in the GUI (engine isolation selector + live CPU %).
- `-DDALI303_BUILD_TOOLS=ON` (default) — builds `DaliRender`, the offline DSP test tool.

> Licensing note: JUCE 8 is dual-licensed (AGPLv3 / commercial). Selling Dali303 closed-source requires a JUCE commercial licence.

---

## Using it

| Control | What it does |
|---|---|
| CUTOFF / RESONANCE | Nonlinear 4-pole acid filter. Resonance also changes internal drive, harmonics and envelope response. CC74 / CC71. |
| ENV / DECAY | Filter envelope depth and decay. Accented notes decay faster (the classic acid "bite"). |
| ACCENT | Depth of accent: sweep boost, resonance loading, punch, VCA and circuit heat. Accents stack when repeated. |
| SLIDE | Slide time. Short slides snap, long slides drag the filter and resonance with them. |
| DRIVE | Drives the **Dali Acid Circuit** itself (inside the filter loop + post-stage): Clean → Warm → Acid → Aggressive → Screaming. Level-compensated. |
| TUNE | ±12 semitones. |
| **LIFE** | 0 = stable classic. Higher = context-aware interaction between notes (previous note/accent/slide, position in the bar, circuit "heat"). **Deterministic**: same pattern + same preset = same result, every playback. |
| OUTPUT | Output level + meter. |
| WAVE | Saw / Square. |
| CIRCUIT QUALITY | Oversampling of the nonlinear section only: 1x / 2x (default) / 4x. |

**Sequencer** (bottom): drag a note cell up/down (Shift = fine), mouse-wheel = ±1 semitone (Shift+wheel = octave), Alt/right-click = rest.
GATE / ACCENT / SLIDE rows: click, or click-and-drag to paint across steps. `<` `>` rotate, `-1` `+1` `OCT-` `OCT+` transpose.
It follows the host: BPM, play/stop and song position (it locks to the bar — start playback anywhere).
In the **Standalone** app there is no host, so an internal clock runs while `SEQ ON` is lit; its BPM control appears next to `SEQ ON` (standalone only).

**External MIDI**: switch `SEQ ON` off and play from a MIDI clip/keyboard.
Velocity ≥ 100 = accent · overlapping notes = slide (legato) · pitch bend ±2 st · mod wheel adds circuit movement.

**Presets**: `<` `>` browse, click the display for the category menu, `SAVE` / `SAVE AS`.
24 factory presets in 10 categories. User presets: `Documents/Dali Audio/Dali303/Presets/*.dali303` (readable XML).
A preset stores every sound parameter and the full pattern.

**DISCOVER**: evolves the current acid in one of five musical directions (Deeper, Screamier, Hypnotic,
Aggressive, Liquid). Bounded parameter moves + 2–4 pattern edits that keep the root, scale and groove.
Deterministic — not a dice roll. Don't like it? Press again, or reload the preset.

**Session recall**: the plug-in state stores all parameters, the pattern, the sequencer on/off state and the
preset identity (name, category, modified flag). Ableton restores the exact state.

---

## Project layout

```
Source/
  DSP/          JUCE-free engine: Oscillator, Envelope, AcidFilter, Oversampler, AcidCircuit,
                AccentEngine, SlideEngine, LifeEngine, NoteContext, AcidEngine (the voice)
  Sequencer/    Pattern (data + text format), StepSequencer (host-synced)
  Presets/      FactoryPresets (data), Discover (JUCE-free), PresetManager (JUCE)
  Plugin/       Parameters, PluginProcessor, PatternStore (lock-free), MonoNoteStack
  UI/           DaliLookAndFeel, DaliKnob, PresetBar, SequencerPanel, PluginEditor
Tools/
  DaliRender.cpp   offline Developer / DSP test suite
Docs/
  DSP_NOTES.md     engine design + measurements
```

The whole DSP layer has no JUCE dependency, so every module can be swapped, tested and rendered offline.

---

## Developer / DSP Test Mode

```bash
./DaliRender out/            # full test suite: renders WAVs of every isolated module + measurements
./DaliRender --presets out/  # renders every factory preset + DISCOVER stress test
./DaliRender --bench         # CPU benchmark at 44.1–192 kHz, 1x/2x/4x
```

In the plug-in (built with `DALI303_DEV_MODE=ON`) the selector isolates:
Full · Oscillator only · Filter clean (no nonlinearity) · No drive · No accent · No slide · No LIFE — plus live CPU.

---

## Status of this first build

Verified here (offline, g++ -Wall -Wextra, zero warnings): the complete DSP engine, sequencer, pattern format,
factory presets, DISCOVER — levels, aliasing, determinism, all sample rates, CPU. See `Docs/DSP_NOTES.md`.

**Not yet verified**: the JUCE layer (processor, preset manager, GUI) was written against the JUCE 8 API
but could not be compiled in the environment where it was written (no network → no JUCE).
First step: build it, and fix any compile errors (expected to be small).
