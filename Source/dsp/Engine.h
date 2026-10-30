// VoxForge – full processing engine (pure C++, host-independent)
#pragma once
#include "Common.h"
#include "PitchDetector.h"
#include "Psola.h"
#include "Analysis.h"
#include "Correction.h"
#include "Effects.h"
#include <atomic>
#include <memory>

namespace vf
{
constexpr int kMaxHarmonyVoices = 8;

struct EngineParams
{
    bool trackingMode = false;
    float inputDb = 0, outputDb = 0, mix = 1;

    // Tune
    bool tuneOn = true;
    int keyRoot = -1;            // -1 = auto (detected)
    int scale = 0;               // 0 = auto (detected major/minor), otherwise ScaleType index + 1
    uint16_t customMask = 0x0AB5; // C major as default
    float retuneMs = 20, toleranceCents = 0, amount = 1, humanize = 0, transpose = 0;
    bool midiTune = false;

    // Gate
    bool gateOn = false; float gateThreshDb = -55, gateRangeDb = 40, gateAttackMs = 1, gateReleaseMs = 120;
    // Sculpt
    bool sculptOn = false; float sculptAmount = 0.5f, sculptTilt = 0;
    // De-Esser
    bool deEssOn = false; float deEssAmount = 0.4f, deEssFreq = 6500;
    // EQ
    bool eqOn = false; ParametricEQ::Settings eq;
    // Compressor
    bool compOn = false; int compMode = 0; float compInput = 6, compOutput = 0, compAttack = 5, compRelease = 120, compMix = 1, compDepth = 0.5f, compRatio = 4;
    // Saturation
    bool satOn = false; int satMode = 0; float satDrive = 6, satTone = 0.5f, satMix = 1, satOutput = 0, satBits = 10; int satDownsample = 1;

    // Harmonies
    bool harmOn = false; int harmMode = 0; // 0 scale, 1 chromatic, 2 MIDI
    int harmVoices = 2; bool harmLink = false, diMute = false;
    float harmLevelDb = -6, harmHumanize = 0.3f, harmWidth = 1;
    int voiceInterval[kMaxHarmonyVoices] { 2, -3, 4, -5, 7, -7, 5, 9 };
    float voiceLevelDb[kMaxHarmonyVoices] {}, voicePan[kMaxHarmonyVoices] { -0.6f, 0.6f, -0.3f, 0.3f, -0.9f, 0.9f, -0.15f, 0.15f };

    // Effects
    bool dlyOn = false; int dlyMode = 0; float dlyTimeL = 375, dlyTimeR = 500, dlyFb = 0.35f, dlyTone = 0.5f, dlyMix = 0.2f, dlyDuck = 0.5f;
    bool revOn = false; int revMode = 0; float revSize = 0.6f, revDecay = 2.5f, revPre = 20, revDamp = 0.5f, revLowCut = 150, revMix = 0.18f, revDuck = 0.3f;
    bool modOn = false; int modMode = 0; float modRate = 0.6f, modDepth = 0.5f, modFb = 0.3f, modMix = 0.35f;
    bool dblOn = false; float dblAmount = 0.5f, dblTiming = 0.5f, dblWidth = 0.8f; bool dblFemale = false;
};

struct EngineMeters
{
    std::atomic<float> inNote { 0 }, outNote { 0 }, targetNote { 0 };
    std::atomic<bool> voiced { false };
    std::atomic<int> keyRoot { -1 };
    std::atomic<bool> keyMinor { false };
    std::atomic<float> keyConfidence { 0 };
    std::atomic<int> source { 0 };
    std::atomic<float> sourceConfidence { 0 };
    std::atomic<float> gateGain { 1 }, deEssGr { 0 }, compGr { 0 };
    std::atomic<float> inLevel { -120 }, outLevel { -120 };
    std::atomic<float> analysisProgress { 0 };
    // pitch history for the display
    static constexpr int kHist = 512;
    float histIn[kHist] {}, histOut[kHist] {};
    std::atomic<int> histWrite { 0 };
};

class VoxEngine
{
public:
    enum class AnalysisState { Idle = 0, Running, Done };

    void prepare (double sampleRate, int maxBlock, bool tracking)
    {
        sr = sampleRate; tracking_ = tracking;
        const float minF0 = tracking ? 95.0f : 60.0f;
        detector.prepare (sr, minF0);
        analyzer.prepare (sr, minF0);
        lead.prepare (2);
        for (auto& v : voices) { v.psola.prepare (1); v.timing.prepare ((int) (sr * 0.06)); v.gain.setTime (30, sr); v.gain.reset (0); }
        for (int i = 0; i < kMaxHarmonyVoices; ++i)
        {
            voices[(size_t) i].pitchRnd = SmoothRandom ((uint32_t) (101 + i * 31));
            voices[(size_t) i].timeRnd = SmoothRandom ((uint32_t) (977 + i * 53));
            voices[(size_t) i].pitchRnd.setup (2.5f + 0.3f * i, sr);
            voices[(size_t) i].timeRnd.setup (0.8f + 0.1f * i, sr);
        }
        gate.prepare (sr); sculpt.prepare (sr); deEss.prepare (sr); eq.prepare (sr); comp.prepare (sr); sat.prepare (sr);
        delay.prepare (sr); reverb.prepare (sr); modu.prepare (sr); dbl.prepare (sr);
        spectrum.prepare (sr);
        key.reset(); classifier.reset(); voiceAn.reset();
        dryDelay[0].prepare (analyzer.getLatency() + maxBlock + 16);
        dryDelay[1].prepare (analyzer.getLatency() + maxBlock + 16);
        bufL.assign ((size_t) maxBlock, 0.0f); bufR.assign ((size_t) maxBlock, 0.0f);
        keyBuf.assign ((size_t) maxBlock, 0.0f);
        tuneMix.setTime (25.0f, sr); tuneMix.reset (p.tuneOn ? 1.0f : 0.0f);
        leadGain.setTime (20.0f, sr); leadGain.reset (1.0f);
        corr = 0; sustainSec = 0; lastTarget = -999; leadRatio = 1;
        hopSec = detector.hopSeconds();
        heldCount = 0; lastHeld = -1;
        analysisState = AnalysisState::Idle;
        lastEqKey = 0;
        applyStaticParams();
    }

    int getLatency() const { return analyzer.getLatency(); }
    bool isTracking() const { return tracking_; }

    void setParams (const EngineParams& np) { p = np; applyStaticParams(); }
    const EngineParams& params() const { return p; }

    // ---------------- MIDI
    void noteOn (int note)
    {
        for (int i = 0; i < heldCount; ++i) if (held[i] == note) return;
        if (heldCount < 16) held[heldCount++] = note;
        lastHeld = note;
    }
    void noteOff (int note)
    {
        for (int i = 0; i < heldCount; ++i)
            if (held[i] == note) { held[i] = held[--heldCount]; break; }
        if (heldCount == 0) lastHeld = -1;
        else if (lastHeld == note) lastHeld = held[heldCount - 1];
    }
    void allNotesOff() { heldCount = 0; lastHeld = -1; }

    // ---------------- auto analysis
    void startAutoAnalysis (float seconds = 8.0f)
    {
        analysisSamplesTarget = (int64_t) (seconds * sr);
        analysisSamples = 0;
        requestAnalysisReset = true;
        analysisState = AnalysisState::Running;
    }
    AnalysisState getAnalysisState() const { return analysisState.load(); }
    // call from message thread when state == Done
    AutoSettings computeAutoSettings() const
    {
        return voiceAn.compute ((SourceType) meters.source.load(), meters.keyRoot.load(), meters.keyMinor.load());
    }
    void acknowledgeAnalysis() { analysisState = AnalysisState::Idle; }
    void resetKeyDetection() { requestKeyReset = true; }

    EngineMeters meters;

    // ---------------- processing
    void process (float* l, float* r, int n)
    {
        if (requestKeyReset.exchange (false)) key.reset();
        if (requestAnalysisReset.exchange (false)) voiceAn.reset();

        const float inG = dbToGain (p.inputDb);
        float inPeak = 0;
        for (int i = 0; i < n; ++i)
        {
            l[i] *= inG; r[i] *= inG;
            inPeak = std::max (inPeak, std::max (std::fabs (l[i]), std::fabs (r[i])));
            dryDelay[0].push (l[i]); dryDelay[1].push (r[i]);
        }
        meters.inLevel = gainToDb (inPeak);

        // ---- Gate (pre-detection, like Mantra's correction chain)
        if (p.gateOn) { gate.process (l, r, n); meters.gateGain = gate.currentGain(); }
        else meters.gateGain = 1.0f;

        // ---- analysis, tune & harmonies (sample loop)
        tuneMix.target = p.tuneOn ? 1.0f : 0.0f;
        leadGain.target = (p.harmOn && p.diMute) ? 0.0f : 1.0f;
        const int Lat = analyzer.getLatency();
        double blockE = 0;
        const bool harm = p.harmOn;
        for (int i = 0; i < n; ++i)
        {
            const float mono = 0.5f * (l[i] + r[i]);
            blockE += (double) mono * mono;
            if (detector.push (mono)) onPitchHop();
            if (spectrum.push (mono)) onSpectrumFrame();
            analyzer.push (l[i], r[i]);

            // lead: crossfade between latency-aligned dry and PSOLA
            const int64_t now = analyzer.newest();
            float dl = analyzer.sample (0, now - Lat), dr = analyzer.sample (1, now - Lat);
            float pl = dl, pr = dr;
            const float tm = tuneMix.next();
            if (tm > 0.0001f || p.tuneOn) { lead.setRatio (leadRatio); lead.process (analyzer, pl, pr); }
            float outL = dl + (pl - dl) * tm, outR = dr + (pr - dr) * tm;
            const float lg = leadGain.next();
            outL *= lg; outR *= lg;
            keyBuf[(size_t) i] = 0.5f * (outL + outR);

            if (harm || anyVoiceAudible)
            {
                anyVoiceAudible = false;
                const float hl = dbToGain (p.harmLevelDb);
                for (int v = 0; v < kMaxHarmonyVoices; ++v)
                {
                    Voice& vc = voices[(size_t) v];
                    const float g = vc.gain.next();
                    if (g < 1.0e-4f && vc.gain.target <= 0.0f) continue;
                    anyVoiceAudible = true;
                    float s, dummy;
                    vc.psola.process (analyzer, s, dummy);
                    vc.timing.push (s);
                    const float dms = 1.5f + p.harmHumanize * (10.0f + 8.0f * vc.timeRnd.next());
                    s = vc.timing.read (dms * 0.001f * (float) sr) * g * hl;
                    const float pan = clampf (vc.pan * p.harmWidth, -1.0f, 1.0f);
                    outL += s * std::sqrt (0.5f * (1.0f - pan));
                    outR += s * std::sqrt (0.5f * (1.0f + pan));
                }
            }
            l[i] = outL; r[i] = outR;
        }
        const float blockRms = (float) std::sqrt (blockE / std::max (1, n));
        if (analysisState == AnalysisState::Running)
        {
            if (gainToDb (blockRms) > -60.0f) { voiceAn.addBlockRms (gainToDb (blockRms)); analysisSamples += n; }
            meters.analysisProgress = clampf ((float) analysisSamples / (float) std::max<int64_t> (1, analysisSamplesTarget), 0, 1);
            if (analysisSamples >= analysisSamplesTarget && voiceAn.hasEnoughData()) analysisState = AnalysisState::Done;
        }

        // ---- correction chain
        if (p.sculptOn) sculpt.process (l, r, n);
        if (p.deEssOn) { deEss.process (l, r, n); meters.deEssGr = deEss.reductionDb(); } else meters.deEssGr = 0;
        if (p.eqOn) eq.process (l, r, n);
        if (p.compOn) { comp.process (l, r, n); meters.compGr = comp.reductionDb(); } else meters.compGr = 0;
        if (p.satOn) sat.process (l, r, n);

        // ---- effects
        if (p.dblOn) dbl.process (l, r, n);
        if (p.modOn) modu.process (l, r, n);
        if (p.dlyOn) delay.process (l, r, keyBuf.data(), n);
        if (p.revOn) reverb.process (l, r, keyBuf.data(), n);

        // ---- global mix + output
        const float outG = dbToGain (p.outputDb);
        float outPeak = 0;
        for (int i = 0; i < n; ++i)
        {
            const int back = n - i + Lat;
            const float dl = dryDelay[0].readInt (back), dr = dryDelay[1].readInt (back);
            l[i] = (dl + (l[i] - dl) * p.mix) * outG;
            r[i] = (dr + (r[i] - dr) * p.mix) * outG;
            if (! std::isfinite (l[i])) l[i] = 0;
            if (! std::isfinite (r[i])) r[i] = 0;
            outPeak = std::max (outPeak, std::max (std::fabs (l[i]), std::fabs (r[i])));
        }
        meters.outLevel = gainToDb (outPeak);
    }

private:
    struct Voice
    {
        PsolaVoice psola; DelayLine timing; Smoother gain;
        SmoothRandom pitchRnd { 1 }, timeRnd { 2 };
        float pan = 0;
    };

    uint16_t currentMask() const
    {
        int root = p.keyRoot >= 0 ? p.keyRoot : meters.keyRoot.load();
        int sc = p.scale;
        if (sc == 0) // auto
        {
            if (root < 0) return 0x0FFF;
            return rotateMask (scaleMask (meters.keyMinor.load() ? ScaleType::Minor : ScaleType::Major), root);
        }
        const ScaleType st = (ScaleType) (sc - 1);
        if (st == ScaleType::Custom) return p.customMask ? p.customMask : (uint16_t) 0x0FFF; // custom mask is absolute
        if (root < 0) root = 0;
        return rotateMask (scaleMask (st), root);
    }

    void onPitchHop()
    {
        const auto& res = detector.result();
        const bool voiced = res.voiced && res.confidence > 0.55f && res.f0 > 0;
        analyzer.setPitch (res.f0, voiced);
        classifier.addPitch (voiced, res.f0, res.confidence);

        const float hop = hopSec;
        const uint16_t mask = currentMask();
        if (voiced)
        {
            const float m = freqToMidi (res.f0);
            key.addPitch (m, res.confidence * dbToGain (std::max (-40.0f, res.rmsDb + 20.0f)) * 0.05f);
            if (analysisState == AnalysisState::Running) voiceAn.addPitch (m);

            float target;
            if (p.midiTune && lastHeld >= 0)
                target = (float) lastHeld + 12.0f * std::round ((m - (float) lastHeld) / 12.0f);
            else
                target = (float) quantizeToMask (m, mask);

            float d = target - m;
            const float tol = p.toleranceCents * 0.01f;
            d = d > 0 ? std::max (0.0f, d - tol) : std::min (0.0f, d + tol);
            d *= p.amount;

            if (std::fabs (target - lastTarget) < 0.01f) sustainSec += hop; else sustainSec = 0;
            lastTarget = target;
            const float sustainF = clampf (sustainSec / 0.4f, 0, 1);
            const float tau = p.retuneMs * (1.0f + p.humanize * 4.0f * sustainF) + p.humanize * 15.0f * sustainF;
            const float a = tau <= 0.5f ? 1.0f : 1.0f - std::exp (-hop * 1000.0f / tau);
            corr += (d - corr) * a;

            const float tuneCorr = p.tuneOn ? corr : 0.0f;
            const float outNote = m + tuneCorr + p.transpose;
            leadRatio = std::pow (2.0f, (tuneCorr + p.transpose) / 12.0f);

            meters.inNote = m; meters.outNote = outNote; meters.targetNote = target; meters.voiced = true;
            pushHistory (m, outNote);
            updateHarmonies (m, outNote, mask, true);
        }
        else
        {
            corr *= 0.9f;
            meters.voiced = false;
            pushHistory (0, 0);
            updateHarmonies (0, 0, mask, false);
        }
        // key evaluation ~ every 0.25 s
        if (++keyEvalCount >= 60)
        {
            keyEvalCount = 0;
            key.decay (0.995f);
            key.evaluate();
            meters.keyRoot = key.root(); meters.keyMinor = key.isMinor(); meters.keyConfidence = key.confidence();
        }
    }

    void updateHarmonies (float inNote, float outNote, uint16_t mask, bool voiced)
    {
        if (! p.harmOn)
        {
            for (auto& v : voices) v.gain.target = 0;
            return;
        }
        int sortedHeld[16]; int nh = heldCount;
        for (int i = 0; i < nh; ++i) sortedHeld[i] = held[i];
        std::sort (sortedHeld, sortedHeld + nh);

        const int base = voiced ? quantizeToMask (outNote, mask) : 60;
        for (int v = 0; v < kMaxHarmonyVoices; ++v)
        {
            Voice& vc = voices[(size_t) v];
            int src = v;
            if (p.harmLink && (v & 1)) src = v - 1;
            const float lvl = dbToGain (p.voiceLevelDb[src]);
            vc.pan = (p.harmLink && (v & 1)) ? -p.voicePan[src] : p.voicePan[v];
            const bool enabled = v < p.harmVoices;
            float gainT = enabled ? lvl : 0.0f;
            float semis = 0;
            if (p.harmMode == 2) // MIDI
            {
                if (v < nh && voiced) semis = (float) sortedHeld[v] - inNote;
                else gainT = 0;
            }
            else if (voiced)
            {
                const int iv = p.voiceInterval[src];
                if (p.harmMode == 0) semis = (outNote - inNote) + (float) (scaleStep (base, iv, mask) - base);
                else semis = (outNote - inNote) + (float) iv;
            }
            const float detune = p.harmHumanize * 0.12f * vc.pitchRnd.next(); // up to ±12 cents
            vc.psola.setRatio (std::pow (2.0f, (semis + detune) / 12.0f));
            vc.psola.setUnvoicedGain (0.35f);
            vc.gain.target = gainT;
        }
    }

    void onSpectrumFrame()
    {
        const auto& mag = spectrum.magnitudes();
        const float bin = spectrum.binHz();
        classifier.addSpectrum (mag, bin, spectrum.flux(), spectrum.rmsDb());
        // polyphonic sources (synth pads, guitar chords) feed chroma into key detection
        const bool mostlyUnpitched = ! meters.voiced.load();
        if (spectrum.rmsDb() > -50 && classifier.current() != SourceType::Drums)
            key.addSpectrum (mag, bin, mostlyUnpitched ? 0.6f : 0.15f);
        if (analysisState == AnalysisState::Running) voiceAn.addSpectrum (mag, bin, spectrum.rmsDb());
        meters.source = (int) classifier.current();
        meters.sourceConfidence = classifier.confidence();
    }

    void pushHistory (float in, float out)
    {
        const int w = meters.histWrite.load (std::memory_order_relaxed);
        meters.histIn[w] = in; meters.histOut[w] = out;
        meters.histWrite.store ((w + 1) % EngineMeters::kHist, std::memory_order_release);
    }

    void applyStaticParams()
    {
        gate.thresholdDb = p.gateThreshDb; gate.rangeDb = p.gateRangeDb; gate.attackMs = p.gateAttackMs; gate.releaseMs = p.gateReleaseMs;
        sculpt.amount = p.sculptAmount; sculpt.tiltDb = p.sculptTilt; sculpt.dynamic = ! p.trackingMode;
        deEss.amount = p.deEssAmount; deEss.freq = p.deEssFreq;
        eq.s = p.eq; eq.update();
        comp.mode = p.compMode; comp.inputDb = p.compInput; comp.outputDb = p.compOutput; comp.attackMs = p.compAttack;
        comp.releaseMs = p.compRelease; comp.mix = p.compMix; comp.depth = p.compDepth; comp.ratio = p.compRatio;
        sat.mode = p.satMode; sat.driveDb = p.satDrive; sat.tone = p.satTone; sat.mix = p.satMix; sat.outputDb = p.satOutput;
        sat.bits = p.satBits; sat.downsample = std::max (1, p.satDownsample);
        delay.mode = p.dlyMode; delay.timeL = p.dlyTimeL; delay.timeR = p.dlyTimeR; delay.feedback = p.dlyFb;
        delay.tone = p.dlyTone; delay.mix = p.dlyMix; delay.duck = p.dlyDuck;
        reverb.mode = p.revMode; reverb.size = p.revSize; reverb.decay = p.revDecay; reverb.preDelayMs = p.revPre;
        reverb.damp = p.revDamp; reverb.lowCut = p.revLowCut; reverb.mix = p.revMix; reverb.duck = p.revDuck;
        modu.mode = p.modMode; modu.rate = p.modRate; modu.depth = p.modDepth; modu.feedback = p.modFb; modu.mix = p.modMix;
        dbl.amount = p.dblAmount; dbl.timing = p.dblTiming; dbl.width = p.dblWidth; dbl.female = p.dblFemale;
    }

    double sr = 48000; bool tracking_ = false;
    EngineParams p;
    PitchDetector detector;
    PsolaAnalyzer analyzer;
    PsolaVoice lead;
    std::array<Voice, kMaxHarmonyVoices> voices;
    Gate gate; Sculpt sculpt; DeEsser deEss; ParametricEQ eq; Compressor comp; Saturation sat;
    Delay delay; Reverb reverb; Modulation modu; Doubler dbl;
    SpectrumFrames spectrum; KeyDetector key; SourceClassifier classifier; VoiceAnalyzer voiceAn;
    DelayLine dryDelay[2];
    std::vector<float> bufL, bufR, keyBuf;
    Smoother tuneMix, leadGain;
    float corr = 0, sustainSec = 0, lastTarget = -999, leadRatio = 1, hopSec = 0.004f;
    int held[16] {}; int heldCount = 0, lastHeld = -1;
    int keyEvalCount = 0;
    bool anyVoiceAudible = false;
    std::atomic<AnalysisState> analysisState { AnalysisState::Idle };
    std::atomic<bool> requestKeyReset { false }, requestAnalysisReset { false };
    int64_t analysisSamples = 0, analysisSamplesTarget = 1;
    uint64_t lastEqKey = 0;
};
} // namespace vf
