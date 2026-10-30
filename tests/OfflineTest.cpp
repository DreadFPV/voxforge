// VoxForge – offline DSP tests (no JUCE). Build: see tests/CMakeLists.txt or
//   g++ -O2 -std=c++17 -I../Source tests/OfflineTest.cpp -o vf_test
#include "dsp/Engine.h"
#include <functional>
#include <cstdio>
#include <chrono>
#include <fstream>
#include <memory>

using namespace vf;
static const double SR = 48000.0;
static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("  FAIL: " __VA_ARGS__); std::printf ("\n"); } else { std::printf ("  ok:   " __VA_ARGS__); std::printf ("\n"); } } while (0)

// "vocal-like" source: band-limited pulse train through 3 formants
struct SynthVoice
{
    double phase = 0; Biquad f1, f2, f3; Rng rng { 7 };
    SynthVoice()
    {
        f1.set (Biquad::BandPass, SR, 700, 5); f2.set (Biquad::BandPass, SR, 1200, 6); f3.set (Biquad::BandPass, SR, 2600, 8);
    }
    float next (double hz)
    {
        phase += hz / SR; if (phase >= 1) phase -= 1;
        // additive band-limited saw
        float s = 0; const int nh = (int) (16000.0 / hz);
        for (int h = 1; h <= std::min (nh, 60); ++h) s += std::sin (kTwoPi * (float) (phase * h)) / h;
        s *= 0.3f;
        return 0.8f * f1.process (s) + 0.5f * f2.process (s) + 0.25f * f3.process (s) + 0.002f * rng.nextBi();
    }
};

static float measurePitchCents (const std::vector<float>& x, size_t from, size_t to, float refMidi, float* outMidi = nullptr)
{
    PitchDetector d; d.prepare (SR, 60); std::vector<float> notes;
    for (size_t i = 0; i < to && i < x.size(); ++i)
        if (d.push (x[i]) && i > from && d.result().voiced && d.result().confidence > 0.8f) notes.push_back (freqToMidi (d.result().f0));
    if (notes.empty()) return 9999;
    std::sort (notes.begin(), notes.end());
    const float med = notes[notes.size() / 2];
    if (outMidi) *outMidi = med;
    return (med - refMidi) * 100.0f;
}

static void writeWav (const char* path, const std::vector<float>& l, const std::vector<float>& r)
{
    std::ofstream f (path, std::ios::binary);
    const uint32_t n = (uint32_t) l.size(), bytes = n * 4;
    auto w32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto w16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); w32 (36 + bytes); f.write ("WAVEfmt ", 8); w32 (16); w16 (1); w16 (2); w32 ((uint32_t) SR); w32 ((uint32_t) SR * 4); w16 (4); w16 (16);
    f.write ("data", 4); w32 (bytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        const int16_t a = (int16_t) (clampf (l[i], -1, 1) * 32767), b = (int16_t) (clampf (r[i], -1, 1) * 32767);
        f.write ((const char*) &a, 2); f.write ((const char*) &b, 2);
    }
}

static void run (VoxEngine& e, std::vector<float>& l, std::vector<float>& r, int block = 256)
{
    for (size_t i = 0; i < l.size(); i += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, l.size() - i);
        e.process (&l[i], &r[i], n);
    }
}

int main()
{
    std::printf ("VoxForge offline tests\n");
    const size_t N = (size_t) (SR * 3.0);

    // ------------------------------------------------ 1. hard tune, detuned + vibrato voice
    {
        std::printf ("[1] Autotune: A3 +35 cents with vibrato -> A3\n");
        auto e = std::make_unique<VoxEngine>();
        e->prepare (SR, 512, false);
        EngineParams p; p.tuneOn = true; p.keyRoot = 0; p.scale = 1 + (int) ScaleType::Major; p.retuneMs = 0;
        e->setParams (p);
        SynthVoice v; std::vector<float> l (N), r (N), in (N);
        for (size_t i = 0; i < N; ++i)
        {
            const double t = i / SR;
            const double midi = 57.35 + 0.08 * std::sin (2 * M_PI * 5.5 * t);
            l[i] = r[i] = in[i] = v.next (midiToFreq ((float) midi));
        }
        run (*e, l, r);
        float mIn = 0, mOut = 0;
        const float cin = measurePitchCents (in, (size_t) SR, N, 57, &mIn);
        const float cout = measurePitchCents (l, (size_t) SR, N, 57, &mOut);
        std::printf ("     input %.1f cents, output %.1f cents (latency %d)\n", cin, cout, e->getLatency());
        CHECK (std::fabs (cout) < 6.0f, "corrected to within 6 cents (%.1f)", cout);
        writeWav ("test1_in.wav", in, in); writeWav ("test1_out.wav", l, r);
    }

    // ------------------------------------------------ 2. note snapping between scale notes
    {
        std::printf ("[2] Autotune: sung F#4 (not in C major) snaps to F or G\n");
        auto e = std::make_unique<VoxEngine>(); e->prepare (SR, 512, false);
        EngineParams p; p.tuneOn = true; p.keyRoot = 0; p.scale = 1 + (int) ScaleType::Major; p.retuneMs = 10;
        e->setParams (p);
        SynthVoice v; std::vector<float> l (N), r (N);
        for (size_t i = 0; i < N; ++i) l[i] = r[i] = v.next (midiToFreq (66.3f));
        run (*e, l, r);
        float m = 0; measurePitchCents (l, (size_t) SR, N, 67, &m);
        std::printf ("     output note %.2f\n", m);
        CHECK (std::fabs (m - 67.0f) < 0.08f, "snapped to G4 (%.2f)", m);
    }

    // ------------------------------------------------ 3. harmony +2 scale steps (A3 -> C4 in C major), DI muted
    {
        std::printf ("[3] Harmony: +2 scale steps, DI mute\n");
        auto e = std::make_unique<VoxEngine>(); e->prepare (SR, 512, false);
        EngineParams p; p.tuneOn = true; p.keyRoot = 0; p.scale = 1 + (int) ScaleType::Major; p.retuneMs = 0;
        p.harmOn = true; p.harmMode = 0; p.harmVoices = 1; p.voiceInterval[0] = 2; p.diMute = true; p.harmHumanize = 0; p.harmLevelDb = 0; p.voicePan[0] = 0;
        e->setParams (p);
        SynthVoice v; std::vector<float> l (N), r (N);
        for (size_t i = 0; i < N; ++i) l[i] = r[i] = v.next (midiToFreq (57.0f));
        run (*e, l, r);
        float m = 0; measurePitchCents (l, (size_t) SR, N, 60, &m);
        std::printf ("     harmony note %.2f\n", m);
        CHECK (std::fabs (m - 60.0f) < 0.1f, "harmony at C4 (%.2f)", m);
        writeWav ("test3_harmony.wav", l, r);
    }

    // ------------------------------------------------ 4. MIDI harmony / MIDI tune
    {
        std::printf ("[4] MIDI tune: held E4 forces lead to E\n");
        auto e = std::make_unique<VoxEngine>(); e->prepare (SR, 512, false);
        EngineParams p; p.tuneOn = true; p.midiTune = true; p.retuneMs = 0;
        e->setParams (p); e->noteOn (64);
        SynthVoice v; std::vector<float> l (N), r (N);
        for (size_t i = 0; i < N; ++i) l[i] = r[i] = v.next (midiToFreq (62.4f));
        run (*e, l, r);
        float m = 0; measurePitchCents (l, (size_t) SR, N, 64, &m);
        CHECK (std::fabs (m - 64.0f) < 0.1f, "lead at E4 (%.2f)", m);
    }

    // ------------------------------------------------ 5. key detection from a D minor melody
    {
        std::printf ("[5] Key detection: D minor melody\n");
        auto e = std::make_unique<VoxEngine>(); e->prepare (SR, 512, false);
        EngineParams p; p.tuneOn = false; e->setParams (p);
        const int mel[] = { 62, 65, 69, 67, 65, 64, 62, 69, 70, 69, 67, 65, 64, 61, 62, 74, 72, 70, 69, 65, 62, 57, 62, 65 };
        const size_t noteLen = (size_t) (SR * 0.35);
        const size_t total = noteLen * 24 * 3;
        SynthVoice v; std::vector<float> l (total), r (total);
        for (size_t i = 0; i < total; ++i)
        {
            const int note = mel[(i / noteLen) % 24];
            const float env = (i % noteLen) < noteLen * 0.85 ? 1.0f : 0.0f;
            l[i] = r[i] = v.next (midiToFreq ((float) note + 0.1f)) * env;
        }
        run (*e, l, r);
        const int root = e->meters.keyRoot.load(); const bool minor = e->meters.keyMinor.load();
        std::printf ("     detected %s %s (conf %.2f)\n", root >= 0 ? noteName (root) : "?", minor ? "minor" : "major", e->meters.keyConfidence.load());
        CHECK ((root == 2 && minor) || (root == 5 && ! minor), "D minor (or relative F major)");
    }

    // ------------------------------------------------ 6. source classification
    {
        std::printf ("[6] Source classification\n");
        auto classify = [] (std::function<float (size_t)> gen, size_t n)
        {
            auto e = std::make_unique<VoxEngine>(); e->prepare (SR, 512, false);
            EngineParams p; p.tuneOn = false; e->setParams (p);
            std::vector<float> l (n), r (n);
            for (size_t i = 0; i < n; ++i) l[i] = r[i] = gen (i);
            run (*e, l, r);
            return (SourceType) e->meters.source.load();
        };
        const size_t n = (size_t) (SR * 6);
        // drums: noise bursts + kick
        Rng rng (5);
        auto drums = [&] (size_t i) { const size_t ph = i % 12000; const float env = std::exp (-(float) ph / 1500.0f); return env * (0.5f * rng.nextBi() + 0.6f * std::sin (kTwoPi * 55.0f * ph / (float) SR)); };
        // synth: stable saw lead
        SynthVoice sv; auto synth = [&] (size_t i) { const int note = 69 + ((i / 24000) % 2) * 5; return sv.next (midiToFreq ((float) note)); };
        // bass: low saw
        SynthVoice bv; auto bass = [&] (size_t i) { const int note = 36 + ((i / 24000) % 3) * 2; return bv.next (midiToFreq ((float) note)) * 2.0f; };
        // vocal-like: vibrato, gaps, consonant noise
        SynthVoice vv; Rng nr (9);
        auto vocal = [&] (size_t i)
        {
            const size_t ph = i % 30000; const double t = i / SR;
            if (ph < 2500) return 0.15f * nr.nextBi() * (ph < 1500 ? 1.0f : 0.3f);      // "s" / consonant
            if (ph > 27000) return 0.0f;
            const int notes[] = { 57, 60, 62, 64, 62, 60 };
            const float m = (float) notes[(i / 30000) % 6] + 0.25f * (float) std::sin (2 * M_PI * 5.2 * t) + 0.1f * nr.nextBi();
            return vv.next (midiToFreq (m));
        };
        const SourceType d = classify (drums, n), s = classify (synth, n), b = classify (bass, n), v = classify (vocal, n);
        std::printf ("     drums->%s synth->%s bass->%s vocal->%s\n", sourceName (d), sourceName (s), sourceName (b), sourceName (v));
        CHECK (d == SourceType::Drums, "drums recognised");
        CHECK (s == SourceType::Synth, "synth recognised");
        CHECK (b == SourceType::Bass, "bass recognised");
        CHECK (v == SourceType::Vocal, "vocal recognised");
    }

    // ------------------------------------------------ 7. auto analysis
    {
        std::printf ("[7] Auto analysis produces settings\n");
        auto e = std::make_unique<VoxEngine>(); e->prepare (SR, 512, false);
        EngineParams p; e->setParams (p);
        e->startAutoAnalysis (3.0f);
        SynthVoice v; Rng nr (3); const size_t n = (size_t) (SR * 6);
        std::vector<float> l (n), r (n);
        for (size_t i = 0; i < n; ++i)
        {
            const bool silent = (i / 24000) % 4 == 3;
            l[i] = r[i] = silent ? 0.003f * nr.nextBi() : v.next (midiToFreq (57.0f + (float) ((i / 24000) % 3)));
        }
        run (*e, l, r);
        CHECK (e->getAnalysisState() == VoxEngine::AnalysisState::Done, "analysis finished");
        const AutoSettings s = e->computeAutoSettings();
        std::printf ("     valid=%d gate=%.1f dB  deess=%.0f  tilt=%.1f  hp=%.0f Hz  mud=%.1f dB  compIn=%.1f  female=%d\n",
                     s.valid, s.gateThresholdDb, s.deEssAmount, s.sculptTilt, s.hpFreq, s.mudCutDb, s.compInputDb, s.female);
        CHECK (s.valid, "settings valid");
        CHECK (s.gateThresholdDb > -60 && s.gateThresholdDb < -20, "gate threshold between noise and voice");
    }

    // ------------------------------------------------ 8. everything on, stability + CPU
    {
        std::printf ("[8] All modules on: stability and CPU\n");
        for (int mode = 0; mode < 3; ++mode)
        {
            auto e = std::make_unique<VoxEngine>(); e->prepare (SR, 512, mode == 2);
            EngineParams p;
            p.gateOn = p.sculptOn = p.deEssOn = p.eqOn = p.compOn = p.satOn = true;
            p.harmOn = true; p.harmVoices = 8; p.dlyOn = p.revOn = p.modOn = p.dblOn = true;
            p.compMode = mode; p.satMode = mode; p.dlyMode = mode + 1; p.revMode = mode; p.modMode = mode;
            p.eq.bGain[1] = 6; p.eq.hsGain = 4; p.dlyFb = 0.9f; p.revDecay = 8; p.satDownsample = 4;
            e->setParams (p);
            SynthVoice v; Rng nr (mode + 1);
            const size_t n = (size_t) (SR * 10);
            std::vector<float> l (n), r (n);
            for (size_t i = 0; i < n; ++i)
            {
                const float m = 50.0f + 20.0f * (float) ((i / 9000) % 7) / 7.0f;
                l[i] = v.next (midiToFreq (m)) + 0.05f * nr.nextBi(); r[i] = l[i] * 0.8f + 0.05f * nr.nextBi();
            }
            const auto t0 = std::chrono::steady_clock::now();
            run (*e, l, r, 128);
            const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            float peak = 0; bool finite = true;
            for (size_t i = 0; i < n; ++i) { if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) finite = false; peak = std::max (peak, std::fabs (l[i])); }
            std::printf ("     mode %d: %.1f%% CPU of realtime (1 core), peak %.2f\n", mode, 100.0 * sec / 10.0, peak);
            CHECK (finite && peak < 8.0f, "stable output");
            if (mode == 0) writeWav ("test8_all.wav", l, r);
        }
    }

    std::printf (failures ? "\n%d FAILURE(S)\n" : "\nALL PASSED\n", failures);
    return failures ? 1 : 0;
}
