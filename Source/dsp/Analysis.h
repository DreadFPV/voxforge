// VoxForge – sound recognition: key/scale detection, source classification, voice analysis
#pragma once
#include "Common.h"

namespace vf
{
// ============================================================== spectral frame provider
// Collects mono audio, produces a magnitude spectrum every `hop` samples.
class SpectrumFrames
{
public:
    static constexpr int kSize = 4096;
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        buf.assign (kSize, 0.0f); work.assign (kSize, {});
        mag.assign (kSize / 2, 0.0f); prevMag.assign (kSize / 2, 0.0f);
        window.resize (kSize);
        for (int i = 0; i < kSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * i / (kSize - 1));
        pos = 0; hopCount = 0;
        hop = (int) std::lround (sr * 0.0427); // ~2048 @ 48k
    }
    inline bool push (float x)
    {
        buf[(size_t) pos] = x; pos = (pos + 1) % kSize;
        if (++hopCount < hop) return false;
        hopCount = 0; compute(); return true;
    }
    const std::vector<float>& magnitudes() const { return mag; }
    float binHz() const { return (float) (sr / kSize); }
    float flux() const { return lastFlux; }
    float rmsDb() const { return lastRmsDb; }
    double sampleRate() const { return sr; }

private:
    void compute()
    {
        double e = 0;
        for (int i = 0; i < kSize; ++i)
        {
            const float s = buf[(size_t) ((pos + i) % kSize)];
            e += (double) s * s;
            work[(size_t) i] = { s * window[(size_t) i], 0.0f };
        }
        lastRmsDb = gainToDb ((float) std::sqrt (e / kSize));
        fft (work);
        float fl = 0;
        for (int k = 0; k < kSize / 2; ++k)
        {
            const float m = std::abs (work[(size_t) k]) * (2.0f / kSize);
            const float d = m - prevMag[(size_t) k];
            if (d > 0) fl += d;
            prevMag[(size_t) k] = m;
            mag[(size_t) k] = m;
        }
        lastFlux = fl;
    }
    double sr = 48000;
    int pos = 0, hop = 2048, hopCount = 0;
    float lastFlux = 0, lastRmsDb = -120;
    std::vector<float> buf, window, mag, prevMag;
    std::vector<std::complex<float>> work;
};

// ============================================================== key / scale detection
class KeyDetector
{
public:
    void reset() { std::fill (std::begin (hist), std::end (hist), 0.0f); total = 0; key = -1; minor = false; conf = 0; candidate = -1; candCount = 0; }

    // monophonic note evidence (from pitch tracker)
    void addPitch (float midi, float weight)
    {
        if (weight <= 0) return;
        const int pc = (((int) std::lround (midi)) % 12 + 12) % 12;
        // weight the nearest pitch class, with a bit to neighbours when out of tune
        const float frac = std::fabs (midi - std::round (midi));
        hist[pc] += weight * (1.0f - frac);
        total += weight;
    }

    // polyphonic chroma evidence (from spectrum)
    void addSpectrum (const std::vector<float>& mag, float binHz, float weight)
    {
        if (weight <= 0) return;
        float chroma[12] {};
        const int k0 = std::max (1, (int) (80.0f / binHz)), k1 = std::min ((int) mag.size() - 1, (int) (2500.0f / binHz));
        float sum = 0;
        for (int k = k0; k <= k1; ++k)
        {
            const float m = mag[(size_t) k];
            // peak picking keeps chroma clean
            if (m < mag[(size_t) k - 1] || m < mag[(size_t) k + 1]) continue;
            const float midi = freqToMidi ((float) k * binHz);
            const int pc = (((int) std::lround (midi)) % 12 + 12) % 12;
            chroma[pc] += std::sqrt (m); sum += std::sqrt (m);
        }
        if (sum <= 1e-6f) return;
        for (int i = 0; i < 12; ++i) hist[i] += weight * chroma[i] / sum;
        total += weight;
    }

    // decay old evidence (call per analysis frame); tau in frames
    void decay (float factor)
    {
        for (float& h : hist) h *= factor;
        total *= factor;
    }

    // re-evaluate; returns true if key changed
    bool evaluate()
    {
        if (total < 2.0f) { conf = 0; return false; }
        // Krumhansl–Kessler profiles
        static const float maj[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
        static const float mnr[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
        float best = -2, second = -2; int bestK = 0; bool bestMinor = false;
        for (int mode = 0; mode < 2; ++mode)
            for (int root = 0; root < 12; ++root)
            {
                const float* prof = mode ? mnr : maj;
                const float c = correlate (prof, root);
                if (c > best) { second = best; best = c; bestK = root; bestMinor = mode == 1; }
                else if (c > second) second = c;
            }
        const float newConf = clampf ((best - second) * 6.0f + (best - 0.5f), 0.0f, 1.0f);
        const int code = bestK + (bestMinor ? 12 : 0);
        const int current = key < 0 ? -1 : key + (minor ? 12 : 0);
        if (code == current) { candCount = 0; conf = newConf; return false; }
        // hysteresis: candidate must win several evaluations in a row
        if (code == candidate) ++candCount; else { candidate = code; candCount = 1; }
        if (current < 0 || candCount >= 4)
        {
            key = bestK; minor = bestMinor; conf = newConf; candCount = 0;
            return true;
        }
        return false;
    }

    int root() const { return key; }           // -1 = unknown
    bool isMinor() const { return minor; }
    float confidence() const { return conf; }
    float evidence() const { return total; }

private:
    float correlate (const float* prof, int root) const
    {
        float mx = 0, my = 0;
        for (int i = 0; i < 12; ++i) { mx += hist[(i + root) % 12]; my += prof[i]; }
        mx /= 12; my /= 12;
        float sxy = 0, sxx = 0, syy = 0;
        for (int i = 0; i < 12; ++i)
        {
            const float x = hist[(i + root) % 12] - mx, y = prof[i] - my;
            sxy += x * y; sxx += x * x; syy += y * y;
        }
        return sxx > 0 && syy > 0 ? sxy / std::sqrt (sxx * syy) : 0;
    }
    float hist[12] {};
    float total = 0, conf = 0;
    int key = -1, candidate = -1, candCount = 0;
    bool minor = false;
};

// ============================================================== source classification
enum class SourceType { Unknown = 0, Vocal, Synth, Guitar, Bass, Drums, NumTypes };

inline const char* sourceName (SourceType t)
{
    switch (t)
    {
        case SourceType::Vocal:  return "Vocal";
        case SourceType::Synth:  return "Synth";
        case SourceType::Guitar: return "Guitar";
        case SourceType::Bass:   return "Bass";
        case SourceType::Drums:  return "Drums";
        default:                 return "Listening...";
    }
}

class SourceClassifier
{
public:
    void reset()
    {
        frames = 0; voicedRatio = 0; centroid = 0; flatness = 0; fluxNorm = 0; f0Med = 0; f0Jit = 0;
        sibil = 0; onsetRate = 0; lastFlux = 0; std::fill (std::begin (score), std::end (score), 0.0f);
        type = SourceType::Unknown; conf = 0; prevF0 = 0;
    }

    // per pitch-hop
    void addPitch (bool voiced, float f0, float confidence)
    {
        const float a = 0.01f;
        voicedRatio += a * ((voiced && confidence > 0.6f ? 1.0f : 0.0f) - voicedRatio);
        if (voiced && f0 > 0)
        {
            const float m = freqToMidi (f0);
            if (f0Med <= 0) f0Med = m; else f0Med += 0.02f * (m - f0Med);
            if (prevF0 > 0) f0Jit += 0.02f * (std::fabs (m - prevF0) - f0Jit);
            prevF0 = m;
        }
        else prevF0 = 0;
    }

    // per spectrum frame
    void addSpectrum (const std::vector<float>& mag, float binHz, float flux, float rmsDb)
    {
        if (rmsDb < -55) return;
        double sum = 0, wsum = 0, logSum = 0, hi = 0;
        int cnt = 0;
        const int k0 = (int) (60.0f / binHz);
        for (int k = k0; k < (int) mag.size(); ++k)
        {
            const double m = mag[(size_t) k] + 1e-9;
            const double f = k * binHz;
            sum += m; wsum += m * f; logSum += std::log (m); ++cnt;
            if (f > 4500 && f < 11000) hi += m;
        }
        if (sum <= 0) return;
        const float c = (float) (wsum / sum);
        const float fl = (float) (std::exp (logSum / cnt) / (sum / cnt));
        const float a = 0.05f;
        centroid += a * (c - centroid);
        flatness += a * (fl - flatness);
        const float fn = flux / (float) (sum + 1e-9);
        fluxNorm += a * (fn - fluxNorm);
        sibil += a * ((float) (hi / sum) - sibil);
        // onset: flux peak
        const bool onset = fn > 0.35f && flux > lastFlux * 1.5f;
        onsetRate += 0.03f * ((onset ? 1.0f : 0.0f) - onsetRate);
        lastFlux = flux;
        ++frames;
        if (frames > 20) classify();
    }

    SourceType current() const { return type; }
    float confidence() const { return conf; }
    float medianNote() const { return f0Med; }

private:
    static float bell (float x, float centre, float width) { const float d = (x - centre) / width; return std::exp (-d * d); }

    void classify()
    {
        float s[(int) SourceType::NumTypes] {};
        const float vr = voicedRatio, jit = f0Jit, cen = centroid, fl = flatness, on = onsetRate;
        // Vocal: mostly voiced but with gaps, lively pitch, sibilance energy, mid range
        s[(int) SourceType::Vocal] = bell (vr, 0.65f, 0.3f) * 1.0f + bell (jit, 0.25f, 0.2f) * 0.8f
                                   + clampf (sibil * 8.0f, 0, 1) * 0.6f + bell (f0Med, 62.0f, 14.0f) * 0.6f
                                   + bell (on, 0.06f, 0.08f) * 0.3f;
        // Synth: very stable pitch, high voicing, low jitter
        s[(int) SourceType::Synth] = bell (vr, 0.92f, 0.15f) * 1.0f + bell (jit, 0.03f, 0.06f) * 1.2f
                                   + bell (fl, 0.05f, 0.08f) * 0.4f;
        // Guitar: plucked onsets, decaying notes, medium voicing, centroid 800–3000
        s[(int) SourceType::Guitar] = bell (on, 0.15f, 0.1f) * 1.0f + bell (vr, 0.6f, 0.3f) * 0.5f
                                    + bell (cen, 1800.0f, 1200.0f) * 0.5f + bell (jit, 0.08f, 0.08f) * 0.4f
                                    + bell (f0Med, 55.0f, 12.0f) * 0.3f;
        // Bass: low pitch, low centroid
        const float lowness = f0Med > 0 ? clampf ((48.0f - f0Med) / 8.0f, 0.0f, 1.0f) : 0.0f; // below ~C3
        s[(int) SourceType::Synth] *= 1.0f - 0.6f * lowness;
        s[(int) SourceType::Bass] = lowness * 2.2f + bell (cen, 300.0f, 350.0f) * 1.0f
                                  + bell (vr, 0.8f, 0.3f) * 0.3f;
        // Drums: noisy, transient, unpitched
        s[(int) SourceType::Drums] = bell (vr, 0.05f, 0.2f) * 1.2f + clampf (on * 4.0f, 0, 1) * 1.0f
                                   + clampf (fl * 3.0f, 0, 1) * 0.8f;
        int best = 1; float total = 0;
        for (int i = 1; i < (int) SourceType::NumTypes; ++i)
        {
            score[i] += 0.1f * (s[i] - score[i]);
            total += score[i];
            if (score[i] > score[best]) best = i;
        }
        type = (SourceType) best;
        conf = total > 0 ? score[best] / total : 0;
    }

    int frames = 0;
    float voicedRatio = 0, centroid = 0, flatness = 0, fluxNorm = 0, f0Med = 0, f0Jit = 0, sibil = 0;
    float onsetRate = 0, lastFlux = 0, prevF0 = 0;
    float score[(int) SourceType::NumTypes] {};
    SourceType type = SourceType::Unknown;
    float conf = 0;
};

// ============================================================== voice analysis → suggested settings
struct AutoSettings
{
    bool valid = false;
    SourceType source = SourceType::Unknown;
    int keyRoot = -1; bool keyMinor = false;
    float gateThresholdDb = -60, deEssAmount = 0, sculptTilt = 0, hpFreq = 80;
    float mudCutDb = 0, mudFreq = 300, presenceDb = 0, airDb = 0;
    float compInputDb = 6;
    bool female = false;
    float levelDb = -20, noiseDb = -70;
};

class VoiceAnalyzer
{
public:
    void reset()
    {
        rmsHist.clear(); rmsHist.reserve (20000); f0Notes.reserve (20000); bands.fill (0); bandFrames = 0; f0Notes.clear(); sibRatio = 0; sibFrames = 0;
    }

    void addBlockRms (float rmsDb) { if (rmsHist.size() < 20000) rmsHist.push_back (rmsDb); }
    void addPitch (float midi) { if (f0Notes.size() < 20000) f0Notes.push_back (midi); }

    void addSpectrum (const std::vector<float>& mag, float binHz, float rmsDb)
    {
        if (rmsDb < -50) return;
        static const float edges[7] = { 60, 150, 450, 1500, 4500, 9000, 16000 };
        std::array<double, 6> e {}; double tot = 0;
        for (int k = 1; k < (int) mag.size(); ++k)
        {
            const float f = k * binHz; const double p = (double) mag[(size_t) k] * mag[(size_t) k];
            for (int b = 0; b < 6; ++b) if (f >= edges[b] && f < edges[b + 1]) { e[(size_t) b] += p; break; }
            tot += p;
        }
        if (tot <= 0) return;
        for (int b = 0; b < 6; ++b) bands[(size_t) b] += 10.0f * std::log10 ((float) (e[(size_t) b] / tot) + 1e-9f);
        ++bandFrames;
        sibRatio += (float) (e[4] / tot); ++sibFrames;
    }

    bool hasEnoughData() const { return bandFrames > 40 && rmsHist.size() > 200; }

    AutoSettings compute (SourceType src, int keyRoot, bool keyMinor) const
    {
        AutoSettings s; s.source = src; s.keyRoot = keyRoot; s.keyMinor = keyMinor;
        if (! hasEnoughData()) return s;
        auto sorted = rmsHist; std::sort (sorted.begin(), sorted.end());
        const float noise = sorted[(size_t) (sorted.size() * 0.10)];
        const float level = sorted[(size_t) (sorted.size() * 0.90)];
        s.noiseDb = noise; s.levelDb = level;
        s.gateThresholdDb = clampf (noise + std::max (6.0f, 0.25f * (level - noise)), -80.0f, -20.0f);
        if (level - noise < 12.0f) s.gateThresholdDb = -80.0f; // no clear noise floor → keep gate open

        // average relative band levels (dB re total)
        float b[6]; for (int i = 0; i < 6; ++i) b[i] = bands[(size_t) i] / (float) bandFrames;
        // reference shape for a well-balanced vocal (dB re total energy)
        static const float ref[6] = { -14.0f, -4.0f, -4.5f, -12.0f, -20.0f, -30.0f };
        const float mud = b[1] - ref[1], pres = b[3] - ref[3], air = b[5] - ref[5];
        s.mudCutDb = clampf (-(mud) * 0.6f, -6.0f, 0.0f);
        s.presenceDb = clampf (-(pres) * 0.5f, -3.0f, 4.0f);
        s.airDb = clampf (-(air) * 0.4f, -2.0f, 4.0f);
        const float tilt = ((b[4] + b[5]) - (b[1] + b[2])) - ((ref[4] + ref[5]) - (ref[1] + ref[2]));
        s.sculptTilt = clampf (-tilt * 0.15f, -4.0f, 4.0f);

        const float sib = sibFrames ? sibRatio / (float) sibFrames : 0;
        s.deEssAmount = clampf ((sib - 0.02f) * 900.0f, 10.0f, 80.0f);

        if (! f0Notes.empty())
        {
            auto n = f0Notes; std::sort (n.begin(), n.end());
            const float lo = n[(size_t) (n.size() * 0.05)], med = n[n.size() / 2];
            s.hpFreq = clampf (midiToFreq (lo) * 0.7f, 40.0f, 160.0f);
            s.female = med > freqToMidi (175.0f);
            s.mudFreq = s.female ? 350.0f : 250.0f;
        }
        // drive the compressor towards ~6 dB GR for typical levels
        s.compInputDb = clampf (-18.0f - level + 6.0f, 0.0f, 24.0f);
        if (src == SourceType::Drums) { s.deEssAmount = 0; }
        s.valid = true;
        return s;
    }

private:
    std::vector<float> rmsHist, f0Notes;
    std::array<float, 6> bands {};
    int bandFrames = 0, sibFrames = 0;
    float sibRatio = 0;
};
} // namespace vf
