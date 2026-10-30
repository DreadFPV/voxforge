// VoxForge – effects section: Delay, Reverb, Modulation, Doubler (+ ducking)
#pragma once
#include "Common.h"

namespace vf
{
// ducking amount from the dry (key) signal
struct Ducker
{
    EnvFollower env; float amount = 0;
    void prepare (double sr) { env.setup (5.0f, 250.0f, sr); env.reset(); }
    inline float gain (float key)
    {
        const float e = gainToDb (env.process (key));
        const float t = clampf ((e + 45.0f) / 30.0f, 0.0f, 1.0f);
        return 1.0f - amount * t * 0.9f;
    }
};

// ============================================================== Delay
class Delay
{
public:
    enum Mode { Analog = 0, Circular, Tape, Dual };
    int mode = Analog;
    float timeL = 375, timeR = 500, feedback = 0.35f, tone = 0.5f, mix = 0.2f, duck = 0.5f;

    void prepare (double s)
    {
        sr = s;
        for (auto& d : dl) d.prepare ((int) (sr * 2.6));
        for (auto& a : diff) a.prepare ((int) (sr * 0.005), 0.5f);
        diff[1].prepare ((int) (sr * 0.0077), 0.5f); diff[2].prepare ((int) (sr * 0.0031), 0.5f); diff[3].prepare ((int) (sr * 0.0113), 0.5f);
        fbState[0] = fbState[1] = 0;
        ducker.prepare (sr);
        wow = 0; flutter = 0; lastTone = -1;
        smL.setTime (80.0f, sr); smR.setTime (80.0f, sr);
        smL.reset (timeL * 0.001f * (float) sr); smR.reset (timeR * 0.001f * (float) sr);
    }

    void process (float* l, float* r, const float* key, int n)
    {
        if (lastTone != tone)
        {
            lp.set (Biquad::LowPass, sr, 1500.0f + tone * 12000.0f, 0.6f);
            hp.set (Biquad::HighPass, sr, mode == Tape ? 180.0f : 100.0f, 0.6f);
            lastTone = tone;
        }
        ducker.amount = duck;
        smL.target = clampf (timeL, 1, 2500) * 0.001f * (float) sr;
        smR.target = clampf (mode == Dual ? timeR : timeL, 1, 2500) * 0.001f * (float) sr;
        const float wowInc = kTwoPi * 0.6f / (float) sr, flInc = kTwoPi * 6.5f / (float) sr, anInc = kTwoPi * 0.35f / (float) sr;
        for (int i = 0; i < n; ++i)
        {
            const float dL = smL.next(), dR = smR.next();
            float mod = 0;
            if (mode == Tape) { wow += wowInc; flutter += flInc; mod = (std::sin (wow) * 0.0018f + std::sin (flutter) * 0.0002f) * (float) sr; }
            else if (mode == Analog || mode == Circular) { wow += anInc; mod = std::sin (wow) * 0.0004f * (float) sr; }
            if (wow > kTwoPi) wow -= kTwoPi;
            if (flutter > kTwoPi) flutter -= kTwoPi;

            float wl = dl[0].read (dL + mod), wr = dl[1].read (dR - mod * 0.7f);
            const float inL = l[i], inR = r[i];
            float fl, fr;
            switch (mode)
            {
                case Circular:
                {
                    // ping-pong through diffusers (rotating, PCM-70 flavour)
                    fl = diff[0].process (diff[1].process (wr));
                    fr = diff[2].process (diff[3].process (wl));
                    dl[0].push (0.5f * (inL + inR) + shape (lp.process (fl, 0)) * feedback);
                    dl[1].push (shape (lp.process (fr, 1)) * feedback);
                    break;
                }
                case Tape:
                    fl = hp.process (lp.process (wl, 0), 0); fr = hp.process (lp.process (wr, 1), 1);
                    dl[0].push (std::tanh ((inL + fl * feedback) * 1.3f) / 1.3f);
                    dl[1].push (std::tanh ((inR + fr * feedback) * 1.3f) / 1.3f);
                    break;
                case Dual:
                    fl = lp.process (wl, 0); fr = lp.process (wr, 1);
                    dl[0].push (inL + fl * feedback);
                    dl[1].push (inR + fr * feedback);
                    break;
                default: // Analog (BBD-ish)
                    fl = lp.process (wl, 0); fr = lp.process (wr, 1);
                    dl[0].push (shape (inL + fl * feedback));
                    dl[1].push (shape (inR + fr * feedback));
                    break;
            }
            const float dg = ducker.gain (key[i]) * mix;
            l[i] = inL + wl * dg;
            r[i] = inR + wr * dg;
        }
    }
private:
    static float shape (float x) { return std::tanh (x * 0.9f) / 0.9f; }
    double sr = 48000;
    DelayLine dl[2]; AllpassDiffuser diff[4]; Biquad lp, hp; Ducker ducker; Smoother smL, smR;
    float fbState[2] {}, wow = 0, flutter = 0, lastTone = -1;
};

// ============================================================== Reverb (8-line FDN)
class Reverb
{
public:
    enum Mode { Hall = 0, Plate, Ambiance };
    int mode = Hall;
    float size = 0.6f, decay = 2.5f, preDelayMs = 20, damp = 0.5f, lowCut = 150, mix = 0.18f, duck = 0.3f;

    void prepare (double s)
    {
        sr = s;
        for (auto& d : lines) d.prepare ((int) (sr * 0.35));
        pre.prepare ((int) (sr * 0.25));
        for (int i = 0; i < 4; ++i) inDiff[i].prepare ((int) (sr * 0.02), 0.6f);
        for (auto& f : dampF) f.reset();
        hpL.reset();
        ducker.prepare (sr);
        lastKey = -1; phase = 0;
    }

    void process (float* l, float* r, const float* key, int n)
    {
        configure();
        ducker.amount = duck;
        const float pd = std::max (1.0f, preDelayMs * 0.001f * (float) sr);
        const float modInc = kTwoPi * 0.7f / (float) sr;
        for (int i = 0; i < n; ++i)
        {
            pre.push (0.5f * (l[i] + r[i]));
            float x = pre.read (pd);
            for (auto& a : inDiff) x = a.process (x);

            phase += modInc; if (phase > kTwoPi) phase -= kTwoPi;
            float o[8];
            for (int k = 0; k < 8; ++k)
            {
                const float m = (k < 2) ? std::sin (phase + k * 1.7f) * modDepth : 0.0f;
                o[k] = dampF[k].lp (lines[k].read (len[k] + m)) * g[k];
            }
            // Householder feedback matrix
            float sum = 0; for (float v : o) sum += v;
            sum *= 0.25f;
            for (int k = 0; k < 8; ++k)
                lines[k].push ((o[k] - sum) + (k & 1 ? x : -x) * 0.5f);

            float wl = (o[0] + o[2] + o[4] + o[6]) * 0.6f, wr = (o[1] + o[3] + o[5] + o[7]) * 0.6f;
            wl = hpL.process (wl, 0); wr = hpL.process (wr, 1);
            const float dg = ducker.gain (key[i]) * mix;
            l[i] += wl * dg; r[i] += wr * dg;
        }
    }

private:
    void configure()
    {
        const int keyNow = mode * 1000003 + (int) (size * 1000) * 1009 + (int) (decay * 100) * 7 + (int) (damp * 100) + (int) lowCut * 13;
        if (keyNow == lastKey) return;
        lastKey = keyNow;
        static const float hall[8]  = { 1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116 };
        static const float plate[8] = { 683, 743, 811, 877, 929, 997, 1063, 1129 };
        static const float amb[8]   = { 331, 373, 419, 467, 503, 547, 601, 653 };
        const float* base = mode == Hall ? hall : (mode == Plate ? plate : amb);
        const float scale = (mode == Hall ? 1.6f : (mode == Plate ? 1.1f : 0.6f)) * (0.35f + size) * (float) (sr / 44100.0);
        const float t60 = std::max (0.1f, mode == Ambiance ? decay * 0.4f : decay);
        for (int k = 0; k < 8; ++k)
        {
            len[k] = base[k] * scale;
            g[k] = std::pow (10.0f, -3.0f * len[k] / (t60 * (float) sr));
            dampF[k].setLowpass ((mode == Plate ? 12000.0f : 9000.0f) * (1.05f - damp), sr);
        }
        static const float dif[4] = { 142, 107, 379, 277 };
        for (int i = 0; i < 4; ++i)
            inDiff[i].set ((int) (dif[i] * (mode == Plate ? 1.3f : 1.0f) * (float) (sr / 44100.0)), mode == Plate ? 0.72f : 0.62f);
        modDepth = mode == Hall ? 6.0f : 2.0f;
        hpL.set (Biquad::HighPass, sr, lowCut, 0.6f);
    }
    double sr = 48000;
    DelayLine lines[8], pre; AllpassDiffuser inDiff[4]; OnePole dampF[8]; Biquad hpL; Ducker ducker;
    float len[8] {}, g[8] {}, phase = 0, modDepth = 3;
    int lastKey = -1;
};

// ============================================================== Modulation (Chorus / Flanger / Phaser)
class Modulation
{
public:
    enum Mode { Chorus = 0, Flanger, Phaser };
    int mode = Chorus;
    float rate = 0.6f, depth = 0.5f, feedback = 0.3f, mix = 0.35f;

    void prepare (double s)
    {
        sr = s; for (auto& d : dl) d.prepare ((int) (sr * 0.05));
        ph = 0; for (auto& st : ap) for (auto& v : st) v = 0; fbS[0] = fbS[1] = 0;
    }

    void process (float* l, float* r, int n)
    {
        const float inc = kTwoPi * rate / (float) sr;
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph > kTwoPi) ph -= kTwoPi;
            float in[2] = { l[i], r[i] }, wet[2];
            for (int c = 0; c < 2; ++c)
            {
                const float lfo = std::sin (ph + (c ? kPi * 0.5f : 0.0f));
                if (mode == Chorus)
                {
                    // two voices per side (2290-style dual chorus)
                    dl[c].push (in[c]);
                    const float base = 0.007f * (float) sr, dep = 0.004f * depth * (float) sr;
                    const float lfo2 = std::sin (ph * 0.73f + (c ? 2.1f : 1.0f));
                    wet[c] = 0.5f * (dl[c].read (base + dep * (0.5f + 0.5f * lfo)) + dl[c].read (base * 1.4f + dep * (0.5f + 0.5f * lfo2)));
                }
                else if (mode == Flanger)
                {
                    const float base = 0.0008f * (float) sr, dep = 0.0035f * depth * (float) sr;
                    const float d = base + dep * (0.5f + 0.5f * lfo);
                    const float y = dl[c].read (d);
                    dl[c].push (in[c] + y * clampf (feedback, -0.95f, 0.95f));
                    wet[c] = y;
                }
                else
                {
                    const float fHz = 200.0f * std::pow (15.0f, (0.5f + 0.5f * lfo) * (0.3f + 0.7f * depth));
                    const float t = std::tan (kPi * std::min (fHz, (float) sr * 0.45f) / (float) sr);
                    const float a = (t - 1.0f) / (t + 1.0f);
                    float x = in[c] + fbS[c] * clampf (feedback, -0.9f, 0.9f);
                    for (int s = 0; s < 6; ++s)
                    {
                        const float y = a * x + ap[c][s];
                        ap[c][s] = x - a * y;
                        x = y;
                    }
                    fbS[c] = x; wet[c] = x;
                }
            }
            l[i] = in[0] + (wet[0] - in[0]) * mix * (mode == Chorus ? 1.0f : 1.0f);
            r[i] = in[1] + (wet[1] - in[1]) * mix;
        }
    }
private:
    double sr = 48000; DelayLine dl[2]; float ph = 0, ap[2][6] {}, fbS[2] {};
};

// ============================================================== Doubler (simulated second take)
class Doubler
{
public:
    float amount = 0.5f, timing = 0.5f, width = 0.8f; bool female = false;

    void prepare (double s)
    {
        sr = s;
        for (auto& d : dl) d.prepare ((int) (sr * 0.12));
        rnd[0] = SmoothRandom (11); rnd[1] = SmoothRandom (29); rnd[2] = SmoothRandom (47); rnd[3] = SmoothRandom (71);
        rnd[0].setup (0.9f, sr); rnd[1].setup (1.1f, sr); rnd[2].setup (0.25f, sr); rnd[3].setup (0.3f, sr);
        lastFemale = ! female; eq[0].reset(); eq[1].reset();
    }

    void process (float* l, float* r, int n)
    {
        if (lastFemale != female)
        {
            // male: a touch darker/fuller, female: thinner/brighter
            eq[0].set (female ? Biquad::HighPass : Biquad::LowShelf, sr, female ? 180.0f : 200.0f, 0.707f, female ? 0.0f : 1.5f);
            eq[1].set (female ? Biquad::HighShelf : Biquad::LowPass, sr, female ? 5000.0f : 9000.0f, 0.707f, female ? 2.0f : 0.0f);
            lastFemale = female;
        }
        const float baseMs = 12.0f + 28.0f * timing;
        const float drift = (female ? 1.2f : 1.8f) + 3.0f * timing;
        const float lvl = amount * 0.9f;
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (l[i] + r[i]);
            dl[0].push (m); dl[1].push (m);
            // slow time drift → natural pitch / timing variation like a real double
            const float d0 = (baseMs + drift * rnd[0].next() + 6.0f * rnd[2].next()) * 0.001f * (float) sr;
            const float d1 = (baseMs * 1.23f + drift * rnd[1].next() + 6.0f * rnd[3].next()) * 0.001f * (float) sr;
            float v0 = eq[1].process (eq[0].process (dl[0].read (std::max (2.0f, d0)), 0), 0);
            float v1 = eq[1].process (eq[0].process (dl[1].read (std::max (2.0f, d1)), 1), 1);
            const float pl = 0.5f + 0.5f * width, pr = 0.5f - 0.5f * width;
            l[i] += lvl * (v0 * pl + v1 * pr);
            r[i] += lvl * (v0 * pr + v1 * pl);
        }
    }
private:
    double sr = 48000; DelayLine dl[2]; SmoothRandom rnd[4] { SmoothRandom (1), SmoothRandom (2), SmoothRandom (3), SmoothRandom (4) };
    Biquad eq[2]; bool lastFemale = true;
};
} // namespace vf
