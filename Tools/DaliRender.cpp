// ============================================================================
//  DaliRender — Dali303 Developer / DSP Test Mode (offline, no JUCE, no DAW)
//
//  Renders isolated engine tests to WAV files and prints measurements, so that
//  when something sounds wrong we can find WHICH part of the engine is at fault.
//
//    DaliRender [outputDir]          -> full test suite + WAVs
//    DaliRender --bench              -> CPU benchmark only
//    DaliRender --presets [dir]      -> render + level-check every factory preset
// ============================================================================
#include "DSP/AcidEngine.h"
#include "Sequencer/Pattern.h"
#include "Sequencer/StepSequencer.h"
#include "Presets/FactoryPresets.h"
#include "Presets/Discover.h"

#include <chrono>
#include <complex>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#if defined(__SSE__) || defined(_M_X64)
 #include <xmmintrin.h>
 #include <pmmintrin.h>
#endif

using namespace dali;
using namespace dali::dsp;

namespace
{
using Automation = std::function<void (EngineParams&, double seconds)>;

void enableFlushToZero()
{
   #if defined(__SSE__) || defined(_M_X64)
    _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE (_MM_DENORMALS_ZERO_ON);
   #endif
}

bool writeWav (const std::string& path, const std::vector<float>& data, int sampleRate)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return false;
    const uint32_t dataBytes = (uint32_t) (data.size() * sizeof (float));
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); w32 (16); w16 (3); w16 (1); w32 ((uint32_t) sampleRate);
    w32 ((uint32_t) sampleRate * 4); w16 (4); w16 (32);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    std::fwrite (data.data(), sizeof (float), data.size(), f);
    std::fclose (f);
    return true;
}

struct Stats { float peak = 0, rms = 0; int nonFinite = 0; };

Stats analyse (const std::vector<float>& d)
{
    Stats s; double acc = 0;
    for (float v : d)
    {
        if (! std::isfinite (v)) { ++s.nonFinite; continue; }
        s.peak = std::max (s.peak, std::abs (v));
        acc += (double) v * v;
    }
    s.rms = (float) std::sqrt (acc / std::max<size_t> (1, d.size()));
    return s;
}

float toDb (float g) { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

/** Render a pattern through the real sequencer, simulating a host at `bpm`. */
std::vector<float> renderPattern (double sr, EngineParams p, DevMode mode, const Pattern& pat,
                                  double bpm, double seconds, const Automation& automate = {})
{
    AcidEngine eng;
    eng.prepare (sr);
    eng.setDevMode (mode);
    eng.setParams (p);
    eng.reset();

    StepSequencer seq;
    std::vector<float> out ((size_t) (seconds * sr), 0.0f);
    std::vector<SeqEvent> events;
    events.reserve (64);
    const int block = 256;
    double ppq = 0.0;

    for (size_t pos = 0; pos < out.size(); pos += block)
    {
        const int n = (int) std::min<size_t> (block, out.size() - pos);
        if (automate) { automate (p, (double) pos / sr); eng.setParams (p); }

        events.clear();
        seq.process (true, ppq, bpm, sr, n, pat, [&] (const SeqEvent& e) { events.push_back (e); });

        int cur = 0;
        for (const auto& e : events)
        {
            if (e.offset > cur) { eng.process (out.data() + pos + (size_t) cur, e.offset - cur); cur = e.offset; }
            if (e.noteOn)
            {
                NoteOnInfo ni;
                ni.note = e.note; ni.velocity = e.accent ? 1.0f : 0.8f; ni.accent = e.accent;
                ni.legato = e.legato; ni.gateSteps = e.gateSteps; ni.stepIndex = e.stepIndex; ni.positionKey = e.positionKey;
                eng.noteOn (ni);
            }
            else eng.noteOff();
        }
        if (cur < n) eng.process (out.data() + pos + (size_t) cur, n - cur);
        ppq += n * bpm / 60.0 / sr;
    }
    return out;
}

/** Render one held note. */
std::vector<float> renderHeld (double sr, EngineParams p, DevMode mode, int note, double seconds,
                               const Automation& automate = {}, bool accent = false)
{
    AcidEngine eng;
    eng.prepare (sr);
    eng.setDevMode (mode);
    eng.setParams (p);
    eng.reset();
    NoteOnInfo ni; ni.note = note; ni.accent = accent; ni.velocity = accent ? 1.0f : 0.8f;
    eng.noteOn (ni);

    std::vector<float> out ((size_t) (seconds * sr), 0.0f);
    const int block = 128;
    for (size_t pos = 0; pos < out.size(); pos += block)
    {
        const int n = (int) std::min<size_t> (block, out.size() - pos);
        if (automate) { automate (p, (double) pos / sr); eng.setParams (p); }
        eng.process (out.data() + pos, n);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Aliasing measurement: energy NOT located at harmonics of f0 (Hann-windowed FFT)
// ---------------------------------------------------------------------------
void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * kPiD / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

float inharmonicRatioDb (const std::vector<float>& sig, size_t start, double sr, double f0)
{
    const size_t N = 1 << 16;
    std::vector<std::complex<double>> a (N);
    for (size_t i = 0; i < N; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * kPiD * (double) i / (double) (N - 1));
        a[i] = (double) sig[start + i] * w;
    }
    fft (a);
    double harm = 0, other = 0;
    const double binHz = sr / (double) N;
    for (size_t k = 2; k < N / 2; ++k)
    {
        const double f = (double) k * binHz;
        if (f < 30.0) continue;
        const double e = std::norm (a[k]);
        const double h = f / f0;
        const double nearest = std::round (h);
        if (nearest >= 1.0 && std::abs (h - nearest) * f0 < 6.0 * binHz) harm += e; else other += e;
    }
    return (float) (10.0 * std::log10 ((other + 1e-30) / (harm + 1e-30)));
}

Pattern demoPattern()
{
    Pattern p;
    patternFromString ("A1 A1 A2A A1 C2S D2 A1 G1A A1 A1S A2 A1 E2A D2S C2 A1", p);
    return p;
}

void report (const char* name, const std::vector<float>& d)
{
    const auto s = analyse (d);
    std::printf ("  %-34s peak %6.1f dBFS   rms %6.1f dBFS   %s\n", name, toDb (s.peak), toDb (s.rms),
                 s.nonFinite ? "!! NON-FINITE SAMPLES !!" : "ok");
}

void save (const std::string& dir, const char* name, const std::vector<float>& d, double sr)
{
    report (name, d);
    if (! dir.empty()) writeWav (dir + "/" + name + ".wav", d, (int) sr);
}

void runBench()
{
    std::printf ("\nCPU benchmark (30 s of dense 16th-note acid, high res + drive)\n");
    const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    EngineParams p; p.resonance = 0.9f; p.drive = 0.7f; p.life = 0.5f;
    for (double sr : rates)
        for (int os : { 1, 2, 4 })
        {
            p.oversampling = os;
            const auto t0 = std::chrono::high_resolution_clock::now();
            auto d = renderPattern (sr, p, DevMode::Full, demoPattern(), 138.0, 30.0);
            const auto t1 = std::chrono::high_resolution_clock::now();
            const double secs = std::chrono::duration<double> (t1 - t0).count();
            std::printf ("  %6.1f kHz  OS %dx (eff %dx): %6.2f%% of one core  (%.0fx realtime)\n",
                         sr / 1000.0, os, AcidEngine::effectiveOversampling (os, sr), 100.0 * secs / 30.0, 30.0 / secs);
        }
}

void runPresets (const std::string& dir)
{
    std::printf ("\nFactory presets @ 48 kHz, 2x OS, 134 BPM, 8 s each\n");
    const double sr = 48000.0;
    for (std::size_t i = 0; i < getNumFactoryPresets(); ++i)
    {
        const auto& fp = getFactoryPresets()[i];
        EngineParams p;
        p.tune = fp.tune; p.cutoff = fp.cutoff; p.resonance = fp.resonance; p.envMod = fp.envMod;
        p.decay = fp.decay; p.accent = fp.accent; p.slide = fp.slide; p.drive = fp.drive;
        p.life = fp.life; p.outputDb = fp.outputDb; p.wave = fp.wave; p.oversampling = 2;
        Pattern pat; patternFromString (fp.pattern, pat);
        auto d = renderPattern (sr, p, DevMode::Full, pat, 134.0, 8.0);
        char name[128]; std::snprintf (name, sizeof name, "P%02d_%s", (int) i + 1, fp.name);
        for (char* c = name; *c; ++c) if (*c == ' ') *c = '_';
        save (dir, name, d, sr);
    }

    std::printf ("\nDISCOVER chains (8 presses from every factory preset): worst-case levels\n");
    float worstPeak = -100.0f, minRms = 100.0f, maxRms = -100.0f; int bad = 0;
    for (std::size_t i = 0; i < getNumFactoryPresets(); ++i)
    {
        const auto& fp = getFactoryPresets()[i];
        SoundValues s { fp.tune, fp.cutoff, fp.resonance, fp.envMod, fp.decay, fp.accent, fp.slide, fp.drive, fp.life, fp.outputDb, fp.wave };
        Pattern pat; patternFromString (fp.pattern, pat);
        for (uint32_t press = 1; press <= 8; ++press)
        {
            auto r = discover (s, pat, hashCombine (hashCombine (press, pat.hash()), s.hash()));
            s = r.sound; pat = r.pattern;
        }
        EngineParams p;
        p.tune = s.tune; p.cutoff = s.cutoff; p.resonance = s.resonance; p.envMod = s.envMod; p.decay = s.decay;
        p.accent = s.accent; p.slide = s.slide; p.drive = s.drive; p.life = s.life; p.outputDb = s.outputDb; p.wave = s.wave;
        const auto st = analyse (renderPattern (sr, p, DevMode::Full, pat, 134.0, 4.0));
        bad += st.nonFinite ? 1 : 0;
        worstPeak = std::max (worstPeak, toDb (st.peak));
        minRms = std::min (minRms, toDb (st.rms)); maxRms = std::max (maxRms, toDb (st.rms));
    }
    std::printf ("  worst peak %.1f dBFS, rms range %.1f .. %.1f dBFS, non-finite renders: %d\n", worstPeak, minRms, maxRms, bad);
}
} // namespace

int main (int argc, char** argv)
{
    enableFlushToZero();
    std::string dir = argc > 1 ? argv[1] : "";
    if (dir == "--bench") { runBench(); return 0; }
    if (dir == "--presets") { runPresets (argc > 2 ? argv[2] : ""); return 0; }

    const double sr = 48000.0;
    std::printf ("Dali303 DSP test suite @ %.0f Hz\n\n", sr);

    EngineParams base;

    std::printf ("[Oscillator]\n");
    { auto p = base; p.wave = 0; save (dir, "01_osc_saw",    renderHeld (sr, p, DevMode::OscillatorOnly, 45, 2.0), sr); }
    { auto p = base; p.wave = 1; save (dir, "02_osc_square", renderHeld (sr, p, DevMode::OscillatorOnly, 45, 2.0), sr); }

    std::printf ("[Filter]\n");
    {
        auto p = base; p.envMod = 0.0f; p.resonance = 0.75f; p.drive = 0.2f;
        save (dir, "03_filter_cutoff_sweep", renderHeld (sr, p, DevMode::Full, 33, 5.0,
              [] (EngineParams& q, double t) { q.cutoff = (float) (t / 5.0); }), sr);
    }
    {
        auto p = base; p.envMod = 0.0f; p.cutoff = 0.45f; p.drive = 0.2f;
        save (dir, "04_resonance_sweep", renderHeld (sr, p, DevMode::Full, 33, 5.0,
              [] (EngineParams& q, double t) { q.resonance = (float) (t / 5.0); }), sr);
    }
    {
        auto p = base; p.envMod = 0.0f; p.cutoff = 0.5f; p.resonance = 1.0f; p.drive = 0.0f;
        save (dir, "05_self_oscillation", renderHeld (sr, p, DevMode::FilterClean, 33, 2.0), sr);
    }

    std::printf ("[Drive / Acid Circuit]  (level should stay within a few dB while character changes)\n");
    for (float d : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        auto p = base; p.drive = d; p.envMod = 0.0f; p.cutoff = 0.45f; p.resonance = 0.8f;
        auto x = renderHeld (sr, p, DevMode::Full, 33, 1.0);
        char name[64]; std::snprintf (name, sizeof name, "   held note, drive %.2f", d);
        report (name, x);
    }
    {
        auto p = base; p.resonance = 0.85f;
        save (dir, "06_drive_sweep_pattern", renderPattern (sr, p, DevMode::Full, demoPattern(), 130.0, 8.0,
              [] (EngineParams& q, double t) { q.drive = (float) (t / 8.0); }), sr);
    }

    std::printf ("[Accent]\n");
    {
        Pattern acc; patternFromString ("C2 C2A C2A C2A C2 C2 C2A C2 C2A C2A C2A C2A C2 C2 C2 C2", acc);
        auto p = base; p.resonance = 0.85f;
        save (dir, "07_accent_run",   renderPattern (sr, p, DevMode::Full,     acc, 130.0, 8.0), sr);
        save (dir, "08_accent_off",   renderPattern (sr, p, DevMode::NoAccent, acc, 130.0, 8.0), sr);
    }

    std::printf ("[Slide]\n");
    {
        Pattern sl; patternFromString ("C2S C3 - - C2S G2 - - C3S C2 - - C2S D#3S C2S G2", sl);
        auto p = base; p.slide = 0.15f;
        save (dir, "09_slide_short", renderPattern (sr, p, DevMode::Full, sl, 120.0, 8.0), sr);
        p.slide = 0.8f;
        save (dir, "10_slide_long",  renderPattern (sr, p, DevMode::Full, sl, 120.0, 8.0), sr);
    }

    std::printf ("[LIFE]  (determinism: two renders must be bit-identical)\n");
    {
        auto p = base; p.life = 0.0f;
        save (dir, "11_life_0",   renderPattern (sr, p, DevMode::Full, demoPattern(), 135.0, 8.0), sr);
        p.life = 1.0f;
        auto a = renderPattern (sr, p, DevMode::Full, demoPattern(), 135.0, 8.0);
        auto b = renderPattern (sr, p, DevMode::Full, demoPattern(), 135.0, 8.0);
        save (dir, "12_life_100", a, sr);
        std::printf ("  deterministic: %s\n", std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0 ? "YES" : "NO !!");
    }

    std::printf ("[Aliasing]  inharmonic energy, high note, full res, full drive (lower = cleaner)\n");
    for (int os : { 1, 2, 4 })
    {
        auto p = base; p.envMod = 0.0f; p.cutoff = 0.85f; p.resonance = 0.9f; p.drive = 1.0f; p.life = 0.0f; p.oversampling = os;
        const double srA = 44100.0;
        auto x = renderHeld (srA, p, DevMode::Full, 81, 2.0);   // A5 = 880 Hz
        // the tiny analog drift (0.25 cent) is included; harmonic bins are +-6 bins wide
        std::printf ("  44.1 kHz, OS %dx : %6.1f dB\n", os, inharmonicRatioDb (x, 20000, srA, 880.0 * std::exp2 (0.0)));
    }

    std::printf ("[Extremes]  (headroom sanity: everything maxed, plus everything minimal)\n");
    {
        Pattern acc; patternFromString ("C2A C3AS C2A C2A G2AS C2A C2A D#3A C2AS C2A C2A C3A C2A C2AS G1A C2A", acc);
        auto p = base; p.cutoff = 1.0f; p.resonance = 1.0f; p.envMod = 1.0f; p.accent = 1.0f; p.drive = 1.0f; p.life = 1.0f; p.decay = 1.0f;
        report ("   all knobs max + all accents", renderPattern (sr, p, DevMode::Full, acc, 140.0, 6.0));
        p.cutoff = 0.0f; p.envMod = 0.0f; p.resonance = 1.0f; p.drive = 0.0f;
        report ("   closed filter, max resonance", renderPattern (sr, p, DevMode::Full, acc, 140.0, 6.0));
        auto q = base; q.cutoff = 0.5f; q.resonance = 1.0f; q.envMod = 0.0f; q.drive = 0.0f;
        report ("   full engine self-oscillation", renderHeld (sr, q, DevMode::Full, 33, 2.0));
        q.cutoff = 0.1f; q.resonance = 0.0f;
        report ("   dark, no resonance", renderHeld (sr, q, DevMode::Full, 33, 2.0));
    }

    std::printf ("[Sample rates]\n");
    for (double r : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        auto p = base; p.resonance = 0.9f; p.drive = 0.6f;
        auto x = renderPattern (r, p, DevMode::Full, demoPattern(), 130.0, 4.0);
        char name[64]; std::snprintf (name, sizeof name, "   pattern @ %.1f kHz", r / 1000.0);
        report (name, x);
    }

    std::printf ("[Demo]\n");
    {
        auto p = base;
        save (dir, "13_demo_pattern", renderPattern (sr, p, DevMode::Full, demoPattern(), 132.0, 16.0,
              [] (EngineParams& q, double t) {
                  q.cutoff = 0.22f + 0.25f * (float) (0.5 - 0.5 * std::cos (t * 0.4));
                  q.drive  = 0.15f + 0.5f  * (float) (t / 16.0);
              }), sr);
    }

    runBench();
    return 0;
}
