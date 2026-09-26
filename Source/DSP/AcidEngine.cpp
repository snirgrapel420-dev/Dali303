#include "AcidEngine.h"

namespace dali::dsp
{
namespace
{
    // Level calibration (measured with Tools/DaliRender, see Docs/DSP_NOTES.md)
    constexpr float kBaseInGain   = 0.40f;
    constexpr float kOutputTrim   = 0.90f;
    constexpr float kResCurvePow  = 1.4f;
    constexpr float kMaxLoopGain  = 4.45f;   // > 4 => self-oscillation at full resonance
}

int AcidEngine::effectiveOversampling (int requested, double sampleRate) noexcept
{
    // Keep the oversampled rate around (requested x 48 kHz): at 96/192 kHz the base
    // rate already provides headroom, so we save CPU without losing quality.
    int f = requested >= 4 ? 4 : (requested >= 2 ? 2 : 1);
    if (sampleRate > 60000.0)  f /= 2;
    if (sampleRate > 120000.0) f /= 2;
    return std::max (1, f);
}

void AcidEngine::prepare (double sampleRate)
{
    fs = sampleRate;

    osc.prepare (sampleRate);
    filterEnv.prepare (sampleRate);
    filterEnv.setSustain (0.0f);
    ampEnv.prepare (sampleRate);
    ampEnv.setAttack (0.0025f);
    ampEnv.setDecay (3.5f);
    ampEnv.setSustain (0.35f);
    ampEnv.setRelease (0.009f);
    accent.prepare (sampleRate);
    slide.prepare (sampleRate);
    life.prepare (sampleRate);
    circuit.prepare (sampleRate);           // allocates oversampling filters (once)
    dc.prepare (sampleRate);

    smCutoff.prepare (sampleRate, 0.005f);
    smRes.prepare (sampleRate, 0.008f);
    smEnvMod.prepare (sampleRate, 0.008f);
    smDrive.prepare (sampleRate, 0.010f);
    smOut.prepare (sampleRate, 0.020f);
    smBend.prepare (sampleRate, 0.006f);
    smMod.prepare (sampleRate, 0.010f);
    smResMod.prepare (sampleRate, 0.010f);
    smAccVca.prepare (sampleRate, 0.003f);

    appliedOversampling = -1;
    lastWave = -1;
    setParams (params);
    reset();
}

void AcidEngine::reset() noexcept
{
    osc.reset();
    filterEnv.reset();
    ampEnv.reset();
    accent.reset();
    circuit.reset();
    dc.reset();

    smCutoff.reset (params.cutoff);
    smRes.reset (params.resonance);
    smEnvMod.reset (params.envMod);
    smDrive.reset (params.drive);
    smOut.reset (dbToGain (params.outputDb));
    smBend.reset (smBend.getTarget());
    smMod.reset (smMod.getTarget());
    smResMod.reset (smResMod.getTarget());
    smAccVca.reset (1.0f);

    slide.jumpTo (36.0f);
    currentNote = -1;
    noteAccent = false;
    mods = {};
    prevFrame = computeFrame (params.cutoff * 8.2f, params.resonance, params.drive, 0.0f);
    resetContext();
}

void AcidEngine::resetContext() noexcept
{
    life.reset();
    prevNote = -1;
    prevAccent = prevSlide = false;
    accentRun = 0;
}

void AcidEngine::setParams (const EngineParams& p) noexcept
{
    params = p;

    if (p.wave != lastWave)
    {
        osc.setWave (p.wave == 1 ? AcidOscillator::Wave::Square : AcidOscillator::Wave::Saw);
        lastWave = p.wave;
    }

    const int os = effectiveOversampling (p.oversampling, fs);
    if (os != appliedOversampling)
    {
        circuit.setOversampling (os);
        appliedOversampling = os;
    }

    smCutoff.setTarget (p.cutoff);
    smRes.setTarget (p.resonance);
    smEnvMod.setTarget (p.envMod);
    smDrive.setTarget (p.drive);
    smOut.setTarget (dbToGain (p.outputDb));

    slide.setTime (mapping::slideSeconds (p.slide));
    accent.setResonance (p.resonance);
    osc.setCurve (0.05f + 0.10f * (devMode == DevMode::NoLife ? 0.0f : p.life));
}

void AcidEngine::setPerformance (const PerformanceControls& pc) noexcept
{
    smBend.setTarget (pc.pitchBend);
    smMod.setTarget (pc.modWheel * 2.0f + pc.ccCutoff * 2.5f);   // octaves
    smResMod.setTarget (pc.ccResonance * 0.3f);
}

void AcidEngine::noteOn (const NoteOnInfo& in) noexcept
{
    const bool acc    = in.accent && devMode != DevMode::NoAccent;
    const bool legato = in.legato && currentNote >= 0 && devMode != DevMode::NoSlide;
    const float lifeAmt = devMode == DevMode::NoLife ? 0.0f : params.life;

    if (in.gateSteps >= 0.0f) lastGateSteps = in.gateSteps;

    NoteContext c;
    c.note        = in.note;
    c.velocity    = in.velocity;
    c.accent      = acc;
    c.slide       = legato;
    c.gateSteps   = lastGateSteps;
    c.prevNote    = prevNote;
    c.prevAccent  = prevAccent;
    c.prevSlide   = prevSlide;
    c.accentRun   = acc ? accentRun + 1 : 0;
    c.stepIndex   = in.stepIndex;
    c.positionKey = in.positionKey;

    mods = life.onNote (c, lifeAmt);

    noteAccent   = acc;
    noteVelocity = in.velocity;
    smAccVca.setTarget (acc ? 1.0f + params.accent * 0.9f : 1.0f);

    if (legato)
    {
        slide.slideTo ((float) in.note);
    }
    else
    {
        slide.jumpTo ((float) in.note);

        float decaySec = mapping::decaySeconds (params.decay) * mods.decayScale;
        if (acc && decaySec > 0.2f)                       // accented notes pull the filter env short (acid classic),
            decaySec = lerpf (decaySec, 0.2f, 0.75f * params.accent); // scaled by the ACCENT knob

        filterEnv.setAttack (acc ? 0.0008f : 0.003f);    // accent = sharper transient
        filterEnv.setDecay (decaySec);
        filterEnv.noteOn();

        ampEnv.setAttack (acc ? 0.0007f : 0.0025f);
        ampEnv.noteOn();
    }

    if (acc)
        accent.trigger (lerpf (0.16f, 0.26f, params.resonance));

    accentRun  = c.accentRun;
    prevNote   = in.note;
    prevAccent = acc;
    prevSlide  = legato;
    currentNote = in.note;
}

void AcidEngine::noteOff() noexcept
{
    ampEnv.noteOff();
    currentNote = -1;
}

void AcidEngine::allNotesOff() noexcept
{
    ampEnv.noteOff();
    currentNote = -1;
}

CircuitFrame AcidEngine::computeFrame (float cutoffOct, float res, float drive, float sweep) const noexcept
{
    CircuitFrame f;
    f.cutoffHz = 40.0f * std::exp2 (cutoffOct);

    const float resEff = clampf (res, 0.0f, 1.0f);
    const float k = kMaxLoopGain * std::pow (resEff, kResCurvePow);
    const float d = clampf (drive, 0.0f, 1.25f);

    f.k      = k * (1.0f + 0.06f * d);
    f.inGain = kBaseInGain * std::exp2 (d * 4.3f);
    f.asym   = 0.02f + 0.06f * d;
    f.blend  = 0.25f;
    // Resonance steals passband level (ladder behaviour) — give part of it back,
    // and compensate the input gain so DRIVE changes character more than volume.
    f.level  = (1.0f + 0.55f * k) * std::pow (kBaseInGain / f.inGain, 0.72f) * (1.0f / kBaseInGain) * 0.5f;

    const float bias = 0.04f + 0.20f * d + mods.satBias + (noteAccent ? 0.08f * params.accent * sweep : 0.0f);
    f.postGain     = 1.0f + 4.5f * d * d;
    f.postBias     = bias;
    f.postTanhBias = std::tanh (bias);
    const float slope = f.postGain * (1.0f - f.postTanhBias * f.postTanhBias);
    f.postNorm     = 1.0f / std::sqrt (std::max (slope, 0.05f));

    switch (devMode)
    {
        case DevMode::FilterClean:
            f.inGain = 0.05f;  f.asym = 0.0f;  f.postEnabled = false;
            f.level  = (1.0f + 0.55f * k) * (1.0f / 0.05f) * 0.2f;
            break;
        case DevMode::NoDrive:
            f.postEnabled = false;
            break;
        default: break;
    }
    return f;
}

void AcidEngine::process (float* out, int numSamples) noexcept
{
    const float lifeAmt = devMode == DevMode::NoLife ? 0.0f : params.life;
    const bool oscOnly  = devMode == DevMode::OscillatorOnly;
    const bool noDrive  = devMode == DevMode::NoDrive;

    for (int i = 0; i < numSamples; ++i)
    {
        const float cutoffK = smCutoff.next();
        const float resK    = smRes.next();
        const float envMod  = smEnvMod.next();
        const float driveK  = noDrive ? 0.0f : smDrive.next();
        const float outGain = smOut.next();
        const float bend    = smBend.next();
        const float modOct  = smMod.next();
        const float resMod  = smResMod.next();
        const float accVca  = smAccVca.next();

        const float meg   = filterEnv.process();
        const float veg   = ampEnv.process();
        const float sweep = accent.process() * mods.accentScale;
        const float notePitch = slide.process();
        life.tick();

        if (ampEnv.isIdle())
        {
            out[i] = 0.0f;
            continue;
        }

        // ---------------- pitch ----------------
        float cents = life.driftCents (lifeAmt) + mods.pitchCents;
        cents -= lifeAmt * 4.0f * resK * meg;                 // resonance "loads" the VCO on transients
        const float pitch = notePitch + params.tune + bend + cents * 0.01f;
        const float x = osc.process (midiToHz (pitch));

        float y;
        if (oscOnly)
        {
            y = 0.5f * x;
        }
        else
        {
            // ---------------- cutoff (in octaves above 40 Hz) ----------------
            const float longness = slide.getLongness();
            const float motion   = slide.motion();
            float oct = cutoffK * 8.2f - envMod * 1.0f;                       // env amount also lowers the base (acid trait)
            oct += envMod * 5.5f * mods.envScale * meg;                       // filter envelope
            oct += params.accent * 2.8f * sweep;                              // accent sweep
            oct += mods.cutoffOct + modOct;                                   // LIFE per-note + mod wheel / CC74
            oct += lifeAmt * 0.35f * (veg - 0.6f);                            // VCA -> filter bleed (LIFE)
            oct += slide.deviation() / 12.0f * lerpf (0.15f, 0.7f, longness); // long slides drag the filter

            // ---------------- resonance ----------------
            float res = resK + resMod + mods.resOffset;
            res += params.accent * 0.10f * sweep * resK;                      // accent tightens resonance
            res += motion * (0.06f + 0.10f * lifeAmt) * (1.0f - longness);    // short slides kick resonance

            // ---------------- drive ----------------
            float drv = driveK + mods.driveOffset;
            drv += (noteAccent ? params.accent * 0.22f * sweep : 0.0f);       // accent pushes the circuit
            drv += lifeAmt * 0.12f * meg;                                     // transient bite (LIFE)
            drv += motion * 0.05f * longness * lifeAmt;                       // long slides swell

            const CircuitFrame frame = computeFrame (oct, res, drv, sweep);
            y = circuit.process (x, prevFrame, frame);
            prevFrame = frame;
            y = dc.process (y);
        }

        const float amp = veg * (0.8f + 0.2f * noteVelocity) * accVca;
        out[i] = y * amp * outGain * kOutputTrim;
    }
}
} // namespace dali::dsp
