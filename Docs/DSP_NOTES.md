# Dali303 — DSP notes

## Signal flow (per note, mono voice)

```
NoteContext ─┬─> LifeEngine (deterministic per-note mods, heat, drift)
             ├─> AccentEngine (accent envelope → sweep capacitor, stacking)
             └─> SlideEngine (RC glide; deviation + motion feed the filter)
                        │
Oscillator (PolyBLEP curved saw / derived square, free-running phase)
   │  base rate
   ▼
┌──────── DALI ACID CIRCUIT (oversampled 1x / 2x / 4x) ────────┐
│ input gain (drive) → 4-pole ZDF ladder, Newton-solved         │
│   tanh + asymmetry at the feedback summing node,              │
│   45 Hz high-pass inside the feedback path                    │
│ → asymmetric tanh post-stage                                  │
└───────────────────────────────────────────────────────────────┘
   │  base rate
   ▼
VCA (accent punch) → DC blocker → output
```

Only the circuit is oversampled; oscillator (band-limited with PolyBLEP), envelopes and
control logic run at the base rate.

## Key design decisions

- **Filter**: 4-pole TPT/zero-delay-feedback ladder. The loop is solved with 2 Newton iterations so
  the nonlinearity sits *inside* the feedback — resonance and drive change character, not only level.
  The feedback high-pass keeps the bass from collapsing at high resonance (a known acid trait).
  Output is a blend towards 3-pole slope for a less "textbook" response.
- **Resonance mapping**: `k = 4.45 · res^1.4`; drive input gain `0.4 · 2^(4.3·drive)` with level compensation.
- **Accent** charges a sweep capacitor whose charge/discharge depends on resonance; consecutive accents stack.
  Accent also shortens decay, sharpens attack and lifts the VCA (`1 + accent·0.9`).
- **Slide** is an RC glide; its deviation and motion are sent to the filter and circuit, so short and long
  slides sound different, not just take different time.
- **LIFE** uses hashes of (position key, note, previous context) — never time-seeded randomness.
  Context is reset on transport start, so the same song position always renders the same audio.
- **Oversampling**: polyphase half-band FIR (Kaiser). Stage 1: 79 taps β 8.4, stage 2: 23 taps β 6.8.
  Effective factor is halved above 60 kHz and again above 120 kHz (192 kHz runs 1x — already alias-free enough).
  Latency is reported to the host.

## Measurements (offline test tool)

| Test | Result |
|---|---|
| Warnings (g++ -Wall -Wextra -Wshadow) | 0 |
| Non-finite samples, all tests | 0 |
| Drive sweep, held note | rms −19.8 → −15.6 dBFS (character changes, level stays) |
| Accent run vs no accent | peak −2.6 vs −6.8 dBFS |
| LIFE determinism (two renders) | bit-identical |
| Inharmonic (alias) energy, A5, max res + drive, 44.1 kHz | 1x −27.6 dB · 2x −36.4 dB · 4x −42.8 dB |
| Extremes (all knobs max, all accents) | peak −4.4 dBFS |
| Pattern level 44.1 / 48 / 88.2 / 96 / 192 kHz | −14.5 dBFS rms at every rate |
| CPU 44.1 kHz (one core) | 1x 1.1 % · 2x 2.0 % · 4x 3.6 % · 192 kHz 4.7 % |
| 24 factory presets | rms −15.2 … −16.9 dBFS (balanced within ~1.7 dB), peaks ≤ −2.2 dBFS |
| DISCOVER: 8 presses from every preset | peak ≤ −2.2 dBFS, rms −16.7 … −13.0 dBFS, deterministic |

## Real-time safety

- No allocation, locks, file I/O or GUI calls in `processBlock`. All buffers allocated in `prepare()`.
- Pattern exchange: `PatternStore` = one atomic word per step + version counter.
- Events: fixed array (1024) + stable insertion sort; rendering is split at event offsets (sample accurate).
- Preset "modified" flag set by an atomic from the parameter listener (safe from the audio thread).
- Latency changes (oversampling switch) are reported through an AsyncUpdater on the message thread.

## Next steps

1. Compile + run in Ableton (Windows + macOS); pluginval at strictness 10.
2. Listening pass per module with Developer Test Mode; tune accent/slide curves by ear.
3. Headroom: the pathological case (closed filter, max resonance, max accent on every step) peaks at −0.1 dBFS.
   Tested lowering resonance compensation 0.55 → 0.45: it only gains 0.6 dB there but drops every preset ~0.8 dB,
   so it was kept at 0.55. Normal material peaks ≤ −2.2 dBFS. Revisit by ear, not by meter.
4. Lower-latency oversampling option (IIR half-bands) for live playing.
5. ~~Internal clock for the Standalone~~ — done (internal BPM, runs while SEQ ON).
6. Pattern length / swing, pattern chaining — only after the sound is signed off.
