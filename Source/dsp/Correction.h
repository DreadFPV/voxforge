// VoxForge – correction section: Gate, Sculpt, De-Esser, Parametric EQ, Compressor, Saturation
#pragma once
#include "Common.h"

namespace vf
{
// ============================================================== Gate
class Gate
{
public:
    float thresholdDb = -60, rangeDb = 60, attackMs = 1, releaseMs = 120, holdMs = 40;
    void prepare (double s) { sr = s; det.setup (0.3f, 30.0f, sr); gain = 1; holdCount = 0; open = false; }
    void process (float* l, float* r, int n)
    {
        const float thr = dbToGain (thresholdDb), thrClose = dbToGain (thresholdDb - 4.0f);
        const float floorG = dbToGain (-rangeDb);
        const float a = timeCoef (attackMs, sr), rl = timeCoef (releaseMs, sr);
        const int hold = (int) (holdMs * 0.001 * sr);
        for (int i = 0; i < n; ++i)
        {
            const float e = det.process (std::max (std::fabs (l[i]), std::fabs (r[i])));
            if (e > thr) { open = true; holdCount = hold; }
            else if (e < thrClose) { if (holdCount > 0) --holdCount; else open = false; }
            const float t = open ? 1.0f : floorG;
            gain = t + (gain - t) * (t > gain ? a : rl);
            l[i] *= gain; r[i] *= gain;
        }
    }
    float currentGain() const { return gain; }
private:
    double sr = 48000; EnvFollower det; float gain = 1; int holdCount = 0; bool open = false;
};

// ============================================================== Sculpt (adaptive EQ + tilt + dynamic resonance control)
class Sculpt
{
public:
    static constexpr int kBands = 7;
    float amount = 0.5f, tiltDb = 0; bool dynamic = false;

    void prepare (double s)
    {
        sr = s;
        for (int b = 0; b < kBands; ++b)
        {
            meas[b].set (Biquad::BandPass, sr, centres[b], 1.2f);
            meas[b].reset(); eq[b].reset();
            longDb[b] = -60; fastEnv[b] = 0; gainDb[b] = 0;
        }
        tiltLo.reset(); tiltHi.reset();
        updateFilters();
    }

    void process (float* l, float* r, int n)
    {
        // measure on mono
        float blockE[kBands] {};
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (l[i] + r[i]);
            for (int b = 0; b < kBands; ++b) { const float y = meas[b].process (m); blockE[b] += y * y; }
        }
        const float blockSec = (float) (n / sr);
        const float aLong = std::exp (-blockSec / 3.0f), aFast = std::exp (-blockSec / 0.015f);
        float totalDb = 0; float dbs[kBands];
        bool active = false;
        for (int b = 0; b < kBands; ++b)
        {
            const float db = 10.0f * std::log10 (blockE[b] / std::max (1, n) + 1e-12f);
            dbs[b] = db;
            if (db > -70) active = true;
        }
        if (active)
        {
            for (int b = 0; b < kBands; ++b)
            {
                longDb[b] = dbs[b] + (longDb[b] - dbs[b]) * aLong;
                fastEnv[b] = dbs[b] + (fastEnv[b] - dbs[b]) * aFast;
                totalDb += longDb[b] - target[b];
            }
            const float meanDev = totalDb / kBands;
            for (int b = 0; b < kBands; ++b)
            {
                const float dev = (longDb[b] - target[b]) - meanDev;
                float g = clampf (-dev * 0.5f * amount, -6.0f, 6.0f);
                if (dynamic && (b == 1 || b == 4 || b == 5))
                {
                    const float spike = fastEnv[b] - longDb[b] - 3.0f;
                    if (spike > 0) g -= clampf (spike * amount, 0.0f, 8.0f);
                }
                gainDb[b] += 0.2f * (g - gainDb[b]);
            }
        }
        updateFilters();
        for (int i = 0; i < n; ++i)
        {
            float x0 = l[i], x1 = r[i];
            for (int b = 0; b < kBands; ++b) { x0 = eq[b].process (x0, 0); x1 = eq[b].process (x1, 1); }
            x0 = tiltHi.process (tiltLo.process (x0, 0), 0);
            x1 = tiltHi.process (tiltLo.process (x1, 1), 1);
            l[i] = x0; r[i] = x1;
        }
    }

    float bandGain (int b) const { return gainDb[b]; }

private:
    void updateFilters()
    {
        for (int b = 0; b < kBands; ++b) eq[b].set (Biquad::Peak, sr, centres[b], 0.9f, gainDb[b]);
        tiltLo.set (Biquad::LowShelf, sr, 900.0f, 0.5f, -tiltDb * 0.5f);
        tiltHi.set (Biquad::HighShelf, sr, 900.0f, 0.5f, tiltDb * 0.5f);
    }
    double sr = 48000;
    const float centres[kBands] = { 120, 300, 800, 2000, 3500, 6000, 11000 };
    // typical well-balanced vocal band levels (relative, dB)
    const float target[kBands] = { -6, -2, 0, -3, -6, -10, -16 };
    Biquad meas[kBands], eq[kBands], tiltLo, tiltHi;
    float longDb[kBands] {}, fastEnv[kBands] {}, gainDb[kBands] {};
};

// ============================================================== De-Esser (adaptive, no threshold)
class DeEsser
{
public:
    float amount = 0.5f, freq = 6500;
    void prepare (double s)
    {
        sr = s; side.reset(); split.reset();
        sEnv.setup (0.5f, 60.0f, sr); fEnv.setup (5.0f, 150.0f, sr);
        lastFreq = -1; gr = 1;
    }
    void process (float* l, float* r, int n)
    {
        if (lastFreq != freq)
        {
            side.set (Biquad::BandPass, sr, freq, 1.4f);
            split.set (Biquad::HighPass, sr, freq * 0.7f, 0.707f);
            lastFreq = freq;
        }
        const float maxGr = amount * 14.0f;
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (l[i] + r[i]);
            const float s = sEnv.process (side.process (m)) + 1e-9f;
            const float f = fEnv.process (m) + 1e-9f;
            float red = 0;
            if (f > 0.0005f)
            {
                // sibilant when the s-band carries a large share of the level
                const float excessDb = 20.0f * std::log10 (s / f) + 8.0f;
                red = clampf (excessDb * (0.6f + amount), 0.0f, maxGr);
            }
            const float tg = dbToGain (-red);
            gr = tg < gr ? tg : tg + (gr - tg) * 0.9985f;
            const float h0 = split.process (l[i], 0), h1 = split.process (r[i], 1);
            l[i] = (l[i] - h0) + h0 * gr;
            r[i] = (r[i] - h1) + h1 * gr;
        }
    }
    float reductionDb() const { return -gainToDb (gr); }
private:
    double sr = 48000; Biquad side, split; EnvFollower sEnv, fEnv; float lastFreq = -1, gr = 1;
};

// ============================================================== Parametric EQ
class ParametricEQ
{
public:
    struct Settings
    {
        bool hpOn = true; float hpFreq = 80;
        bool lpOn = false; float lpFreq = 18000;
        float lsFreq = 120, lsGain = 0;
        float bFreq[4] { 250, 1000, 3000, 7000 }, bGain[4] {}, bQ[4] { 1, 1, 1, 1 };
        float hsFreq = 10000, hsGain = 0;
        int solo = 0; // 0 off, 1 LS, 2..5 bells, 6 HS
    } s;

    void prepare (double r) { sr = r; for (auto& f : flt) f.reset(); soloF.reset(); update(); }
    void update()
    {
        flt[0].set (Biquad::HighPass, sr, s.hpFreq, 0.707f);
        flt[1].set (Biquad::HighPass, sr, s.hpFreq, 0.707f);
        flt[2].set (Biquad::LowShelf, sr, s.lsFreq, 0.707f, s.lsGain);
        for (int b = 0; b < 4; ++b) flt[3 + b].set (Biquad::Peak, sr, s.bFreq[b], s.bQ[b], s.bGain[b]);
        flt[7].set (Biquad::HighShelf, sr, s.hsFreq, 0.707f, s.hsGain);
        flt[8].set (Biquad::LowPass, sr, s.lpFreq, 0.707f);
        flt[9].set (Biquad::LowPass, sr, s.lpFreq, 0.707f);
        float sf = 1000, sq = 1;
        if (s.solo == 1) { sf = s.lsFreq; sq = 0.7f; }
        else if (s.solo >= 2 && s.solo <= 5) { sf = s.bFreq[s.solo - 2]; sq = s.bQ[s.solo - 2]; }
        else if (s.solo == 6) { sf = s.hsFreq; sq = 0.7f; }
        soloF.set (Biquad::BandPass, sr, sf, sq);
    }
    void process (float* l, float* r, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            float x[2] = { l[i], r[i] };
            for (int c = 0; c < 2; ++c)
            {
                float v = x[c];
                if (s.solo > 0) { v = soloF.process (v, c); x[c] = v; continue; }
                if (s.hpOn) { v = flt[0].process (v, c); v = flt[1].process (v, c); }
                for (int k = 2; k < 8; ++k) v = flt[k].process (v, c);
                if (s.lpOn) { v = flt[8].process (v, c); v = flt[9].process (v, c); }
                x[c] = v;
            }
            l[i] = x[0]; r[i] = x[1];
        }
    }
private:
    double sr = 48000; Biquad flt[10], soloF;
};

// ============================================================== Compressor (Tube / Stress / Multi)
class Compressor
{
public:
    enum Mode { Tube = 0, Stress, Multi };
    int mode = Tube;
    float inputDb = 6, outputDb = 0, attackMs = 10, releaseMs = 150, mix = 1, depth = 0.5f;
    float ratio = 4;

    void prepare (double s)
    {
        sr = s; env = 0; grDb = 0; optoSlow = 0;
        for (auto& f : xo) f.reset();
        for (auto& e : bandEnv) e = 0;
        xo[0].set (Biquad::LowPass, sr, 120.0f, 0.707f);  xo[1].set (Biquad::LowPass, sr, 120.0f, 0.707f);
        xo[2].set (Biquad::HighPass, sr, 120.0f, 0.707f); xo[3].set (Biquad::HighPass, sr, 120.0f, 0.707f);
        xo[4].set (Biquad::LowPass, sr, 2500.0f, 0.707f); xo[5].set (Biquad::LowPass, sr, 2500.0f, 0.707f);
        xo[6].set (Biquad::HighPass, sr, 2500.0f, 0.707f); xo[7].set (Biquad::HighPass, sr, 2500.0f, 0.707f);
        scHp.set (Biquad::HighPass, sr, 90.0f, 0.707f); scHp.reset();
    }

    void process (float* l, float* r, int n)
    {
        const float inG = dbToGain (inputDb), outG = dbToGain (outputDb);
        if (mode == Multi) { processMulti (l, r, n, inG, outG); return; }
        const bool tube = mode == Tube;
        const float thr = tube ? -18.0f : -20.0f;
        const float rt = tube ? 3.0f : ratio;
        const float knee = tube ? 10.0f : 3.0f;
        const float att = timeCoef (tube ? 10.0f : attackMs, sr);
        const float relFast = timeCoef (tube ? 60.0f : releaseMs, sr);
        const float autoMakeup = tube ? (-thr) * (1.0f - 1.0f / rt) * 0.45f : (-thr) * (1.0f - 1.0f / rt) * 0.35f;
        const float mk = dbToGain (autoMakeup) * outG;
        for (int i = 0; i < n; ++i)
        {
            const float x0 = l[i] * inG, x1 = r[i] * inG;
            const float sc = tube ? std::max (std::fabs (x0), std::fabs (x1)) : std::fabs (scHp.process (0.5f * (x0 + x1)));
            const float lv = gainToDb (sc);
            float over = lv - thr, target = 0;
            if (over > knee * 0.5f) target = over * (1.0f - 1.0f / rt);
            else if (over > -knee * 0.5f) { const float k = over + knee * 0.5f; target = (1.0f - 1.0f / rt) * k * k / (2.0f * knee); }
            if (target > grDb) grDb = target + (grDb - target) * att;
            else
            {
                if (tube)
                {
                    // opto: fast first stage, then slow program-dependent tail
                    optoSlow = std::max (optoSlow * 0.99999f, grDb);
                    const float slow = timeCoef (500.0f + 3000.0f * clampf (optoSlow / 12.0f, 0, 1), sr);
                    const float coef = grDb > optoSlow * 0.5f ? relFast : slow;
                    grDb = target + (grDb - target) * coef;
                }
                else grDb = target + (grDb - target) * relFast;
            }
            const float g = dbToGain (-grDb) * mk;
            float y0 = x0 * g, y1 = x1 * g;
            if (tube) { y0 = tubeColour (y0); y1 = tubeColour (y1); }
            else { y0 = stressColour (y0); y1 = stressColour (y1); }
            l[i] = l[i] + (y0 - l[i]) * mix;
            r[i] = r[i] + (y1 - r[i]) * mix;
        }
    }
    float reductionDb() const { return grDb; }

private:
    static float tubeColour (float x) { return std::tanh (x * 0.9f + 0.05f * x * x) / 0.9f; }
    static float stressColour (float x) { return x + 0.04f * x * x - 0.02f * x * x * x; }

    void processMulti (float* l, float* r, int n, float inG, float outG)
    {
        // OTT style: downward + upward compression per band
        const float att = timeCoef (std::max (0.5f, attackMs), sr), rel = timeCoef (std::max (20.0f, releaseMs), sr);
        const float dThr = -24.0f, uThr = -38.0f;
        float maxGr = 0;
        for (int i = 0; i < n; ++i)
        {
            float in[2] = { l[i] * inG, r[i] * inG }, out[2];
            float bandsS[3][2];
            for (int c = 0; c < 2; ++c)
            {
                const float lo = xo[1].process (xo[0].process (in[c], c), c);
                const float rest = xo[3].process (xo[2].process (in[c], c), c);
                const float mid = xo[5].process (xo[4].process (rest, c), c);
                const float hi = xo[7].process (xo[6].process (rest, c), c);
                bandsS[0][c] = lo; bandsS[1][c] = mid; bandsS[2][c] = hi;
            }
            out[0] = out[1] = 0;
            for (int b = 0; b < 3; ++b)
            {
                const float a = std::max (std::fabs (bandsS[b][0]), std::fabs (bandsS[b][1]));
                bandEnv[b] = a > bandEnv[b] ? a + (bandEnv[b] - a) * att : a + (bandEnv[b] - a) * rel;
                const float lv = gainToDb (bandEnv[b]);
                float gDb = 0;
                if (lv > dThr) gDb = -(lv - dThr) * (1.0f - 1.0f / 8.0f);
                else if (lv < uThr && lv > -75.0f) gDb = std::min (18.0f, (uThr - lv) * (1.0f - 1.0f / 3.0f));
                gDb *= depth;
                maxGr = std::max (maxGr, -gDb);
                const float g = dbToGain (gDb);
                out[0] += bandsS[b][0] * g; out[1] += bandsS[b][1] * g;
            }
            out[0] *= outG; out[1] *= outG;
            l[i] = l[i] + (out[0] - l[i]) * mix;
            r[i] = r[i] + (out[1] - r[i]) * mix;
        }
        grDb = maxGr;
    }

    double sr = 48000;
    float env = 0, grDb = 0, optoSlow = 0, bandEnv[3] {};
    Biquad xo[8], scHp;
};

// ============================================================== Saturation (Tape / Tube / Digital)
class Saturation
{
public:
    enum Mode { Tape = 0, Tube, Digital };
    int mode = Tape;
    float driveDb = 6, tone = 0.5f, mix = 1, outputDb = 0, bits = 12; int downsample = 1;

    void prepare (double s) { sr = s; pre.reset(); post.reset(); dcb.reset(); hold[0] = hold[1] = 0; holdCount = 0; lastTone = -1; dcb.set (Biquad::HighPass, sr, 15.0f, 0.707f); }

    void process (float* l, float* r, int n)
    {
        if (lastTone != tone)
        {
            post.set (Biquad::LowPass, sr, 2500.0f + 17000.0f * tone, 0.707f);
            pre.set (Biquad::HighShelf, sr, 3000.0f, 0.707f, (tone - 0.5f) * 6.0f);
            lastTone = tone;
        }
        const float dg = dbToGain (driveDb), og = dbToGain (outputDb);
        const float comp = 1.0f / std::sqrt (std::max (1.0f, dg));
        const float q = std::pow (2.0f, clampf (bits, 2, 16) - 1.0f);
        for (int i = 0; i < n; ++i)
        {
            float x[2] = { l[i], r[i] };
            for (int c = 0; c < 2; ++c)
            {
                float v = x[c], y;
                switch (mode)
                {
                    case Tape:
                        v = pre.process (v * dg, c);
                        y = std::tanh (v) * comp * 1.2f;
                        y = post.process (y, c);
                        break;
                    case Tube:
                    {
                        // asymmetric, bias-shifted triode style (Culture Vulture-ish)
                        v = v * dg;
                        y = v >= 0 ? std::tanh (v * 1.2f) : std::tanh (v * 0.8f) * 1.1f;
                        y += 0.12f * v * v / (1.0f + v * v);
                        y = dcb.process (y, c) * comp * 1.3f;
                        y = post.process (y, c);
                        break;
                    }
                    default:
                    {
                        if (c == 0 && ++holdCount >= downsample) holdCount = 0;
                        if (holdCount == 0) hold[c] = std::round (v * dg * q) / q;
                        y = clampf (hold[c], -1.0f, 1.0f) * comp;
                        break;
                    }
                }
                x[c] = x[c] + (y * og - x[c]) * mix;
            }
            l[i] = x[0]; r[i] = x[1];
        }
    }
private:
    double sr = 48000; Biquad pre, post, dcb; float hold[2] {}; int holdCount = 0; float lastTone = -1;
};
} // namespace vf
