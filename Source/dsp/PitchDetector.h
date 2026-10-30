// VoxForge – streaming YIN pitch detector (decimated)
#pragma once
#include "Common.h"

namespace vf
{
class PitchDetector
{
public:
    struct Result { float f0 = 0; float confidence = 0; bool voiced = false; float rmsDb = -120; };

    // allocates for the lowest supported pitch, then configures
    void prepare (double sampleRate, float minF0, float maxF0 = 1100.0f)
    {
        sr = sampleRate;
        decim = std::max (1, (int) std::lround (sr / 22050.0));
        srD = sr / decim;
        const int tMaxAlloc = (int) std::ceil (srD / 45.0) + 2;
        const int lenAlloc = 2 * tMaxAlloc + 2;
        ring.assign ((size_t) lenAlloc * 2, 0.0f);
        frame.assign ((size_t) lenAlloc, 0.0f);
        diff.assign ((size_t) tMaxAlloc + 2, 0.0f);
        configure (minF0, maxF0);
    }

    // real-time safe (no allocation) as long as minF0 >= 45 Hz
    void configure (float minF0, float maxF0 = 1100.0f)
    {
        tauMin = std::max (2, (int) std::floor (srD / maxF0));
        tauMax = std::min ((int) diff.size() - 2, (int) std::ceil (srD / std::max (45.0f, minF0)) + 2);
        win = tauMax;
        bufLen = win + tauMax + 2;
        std::fill (ring.begin(), ring.end(), 0.0f);
        hop = std::max (32, (int) (srD * 0.004)); // ~4 ms
        aa1.set (Biquad::LowPass, sr, (float) (srD * 0.42), 0.54f);
        aa2.set (Biquad::LowPass, sr, (float) (srD * 0.42), 1.31f);
        hpf.set (Biquad::HighPass, sr, 50.0f, 0.707f);
        aa1.reset(); aa2.reset(); hpf.reset();
        wpos = 0; decCount = 0; hopCount = 0; hist[0] = hist[1] = hist[2] = 0;
        last = {};
    }

    // threshold: YIN aperiodicity threshold (0.05..0.3); gateDb: below -> unvoiced
    void setThreshold (float t) { threshold = t; }
    void setGateDb (float db) { gateDb = db; }

    // returns true when a new estimate is ready
    inline bool push (float x)
    {
        x = aa2.process (aa1.process (hpf.process (x)));
        if (++decCount < decim) return false;
        decCount = 0;
        ring[(size_t) wpos] = x;
        ring[(size_t) (wpos + bufLen)] = x;
        wpos = (wpos + 1) % bufLen;
        if (++hopCount < hop) return false;
        hopCount = 0;
        analyse();
        return true;
    }

    const Result& result() const { return last; }
    int decimation() const { return decim; }
    float hopSeconds() const { return (float) (hop / srD); }

private:
    void analyse()
    {
        // linearise ring → frame (oldest first)
        const float* src = &ring[(size_t) wpos];
        std::copy (src, src + bufLen, frame.begin());

        // rms of most recent window
        double e = 0;
        for (int i = bufLen - win; i < bufLen; ++i) e += (double) frame[(size_t) i] * frame[(size_t) i];
        const float rms = (float) std::sqrt (e / win);
        Result r; r.rmsDb = gainToDb (rms);
        if (r.rmsDb < gateDb) { pushHist (0); last = r; return; }

        // difference function on the most recent part of the frame
        const int start = bufLen - win - tauMax;
        diff[0] = 1.0f;
        float running = 0;
        int bestTau = -1;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double s = 0;
            const float* a = &frame[(size_t) (start + tauMax)];
            const float* b = &frame[(size_t) (start + tauMax - tau)];
            for (int j = 0; j < win; ++j) { const float d = a[j] - b[j]; s += d * d; }
            running += (float) s;
            diff[(size_t) tau] = running > 0 ? (float) s * tau / running : 1.0f;
        }
        // absolute threshold
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (diff[(size_t) tau] < threshold)
            {
                while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                bestTau = tau; break;
            }
        }
        if (bestTau < 0)
        {
            // fallback: global min, low confidence
            float mn = 1e9f;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (diff[(size_t) tau] < mn) { mn = diff[(size_t) tau]; bestTau = tau; }
            r.confidence = clampf (1.0f - mn, 0, 1) * 0.5f;
            r.voiced = false;
            r.f0 = bestTau > 0 ? (float) (srD / bestTau) : 0;
            pushHist (0);
            last = r; return;
        }
        // parabolic interpolation
        float t = (float) bestTau;
        if (bestTau > 1 && bestTau < tauMax - 1)
        {
            const float s0 = diff[(size_t) bestTau - 1], s1 = diff[(size_t) bestTau], s2 = diff[(size_t) bestTau + 1];
            const float den = 2.0f * (2.0f * s1 - s2 - s0);
            if (std::fabs (den) > 1e-9f) t += (s2 - s0) / den;
        }
        r.f0 = (float) (srD / t);
        r.confidence = clampf (1.0f - diff[(size_t) bestTau], 0, 1);
        r.voiced = true;

        // median-of-3 smoothing + octave-jump guard
        pushHist (r.f0);
        float a = hist[0], b = hist[1], c = hist[2];
        if (a > 0 && b > 0 && c > 0)
        {
            const float med = std::max (std::min (a, b), std::min (std::max (a, b), c));
            const float ratio = r.f0 / med;
            if (ratio > 1.8f || ratio < 0.55f) r.f0 = med;
        }
        last = r;
    }

    void pushHist (float f) { hist[0] = hist[1]; hist[1] = hist[2]; hist[2] = f; }

    double sr = 48000, srD = 24000;
    int decim = 2, tauMin = 20, tauMax = 400, win = 400, bufLen = 802, hop = 96;
    int wpos = 0, decCount = 0, hopCount = 0;
    float threshold = 0.15f, gateDb = -55.0f;
    float hist[3] {};
    std::vector<float> ring, frame, diff;
    Biquad aa1, aa2, hpf;
    Result last;
};
} // namespace vf
