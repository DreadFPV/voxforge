// VoxForge – real-time TD-PSOLA pitch shifting (formant preserving)
//
// One PsolaAnalyzer holds the input history (L, R, Mono) and the pitch marks.
// Any number of PsolaVoice objects synthesise from it with their own ratio,
// so the lead tune and all harmony voices share one analysis.
#pragma once
#include "Common.h"

namespace vf
{
class PsolaAnalyzer
{
public:
    static constexpr int kRing = 1 << 15;
    static constexpr int kMask = kRing - 1;
    static constexpr int kMarks = 1024;

    struct Mark { int64_t pos = 0; int period = 256; bool voiced = false; };

    void prepare (double sampleRate, float minF0)
    {
        sr = sampleRate;
        maxPeriod = (int) std::ceil (sr / minF0);
        unvoicedPeriod = std::max (32, (int) (sr * 0.004));
        latency = 2 * maxPeriod + 16;
        for (auto& r : ring) std::fill (std::begin (r), std::end (r), 0.0f);
        n = 0; nextMark = 0; markCount = 0; head = 0;
        period = unvoicedPeriod; voiced = false;
    }

    void setPitch (float f0, bool isVoiced)
    {
        voiced = isVoiced && f0 > 0;
        period = voiced ? clampi ((int) std::lround (sr / f0), 16, maxPeriod) : unvoicedPeriod;
    }

    inline void push (float l, float r)
    {
        const int i = (int) (n & kMask);
        ring[0][i] = l; ring[1][i] = r; ring[2][i] = 0.5f * (l + r);
        if (n >= nextMark)
        {
            Mark& m = marks[(size_t) (head & (kMarks - 1))];
            m.pos = n; m.period = period; m.voiced = voiced;
            ++head; markCount = std::min (markCount + 1, kMarks);
            nextMark = n + period;
        }
        ++n;
    }

    // newest sample index that has been written
    int64_t newest() const { return n - 1; }
    int getLatency() const { return latency; }
    int getMaxPeriod() const { return maxPeriod; }
    inline float sample (int ch, int64_t pos) const { return ring[ch][(int) (pos & kMask)]; }

    // nearest mark to t whose grain is fully available
    const Mark* findMark (double t) const
    {
        const Mark* best = nullptr; double bestD = 1e18;
        const int64_t avail = n - 1;
        for (int k = 1; k <= markCount; ++k)
        {
            const Mark& m = marks[(size_t) ((head - k) & (kMarks - 1))];
            if (m.pos + m.period > avail) continue;
            const double d = std::fabs ((double) m.pos - t);
            if (d < bestD) { bestD = d; best = &m; }
            else if ((double) m.pos < t - 2.0 * maxPeriod) break;
        }
        return best;
    }

private:
    static int clampi (int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
    double sr = 48000;
    int maxPeriod = 800, unvoicedPeriod = 192, latency = 1616, period = 192;
    bool voiced = false;
    int64_t n = 0, nextMark = 0;
    int head = 0, markCount = 0;
    float ring[3][kRing] {};
    std::array<Mark, kMarks> marks {};
};

class PsolaVoice
{
public:
    static constexpr int kRing = PsolaAnalyzer::kRing;
    static constexpr int kMask = kRing - 1;

    // channels: 2 = stereo from L/R, 1 = mono (uses analyzer mono channel)
    void prepare (int numChannels)
    {
        chans = numChannels;
        for (auto& a : acc) std::fill (std::begin (a), std::end (a), 0.0f);
        std::fill (std::begin (wsum), std::end (wsum), 0.0f);
        nextTs = -1; ratio = 1.0f;
    }

    void setRatio (float r) { ratio = clampf (r, 0.25f, 4.0f); }
    void setUnvoicedGain (float g) { unvoicedGain = g; }

    // call once per sample AFTER analyzer.push(); writes output for output index = analyzer.newest()
    inline void process (const PsolaAnalyzer& an, float& outL, float& outR)
    {
        const int64_t now = an.newest();
        const int maxP = an.getMaxPeriod();
        const int L = an.getLatency();
        if (nextTs < 0 || nextTs < now - maxP) nextTs = now + maxP;

        // schedule grains whose left edge may reach "now" soon
        while (nextTs - maxP <= now)
        {
            const PsolaAnalyzer::Mark* m = an.findMark ((double) (nextTs - L));
            if (m == nullptr) { nextTs += 64; continue; }
            placeGrain (an, *m, nextTs, now);
            const float r = m->voiced ? ratio : 1.0f;
            nextTs += std::max<int64_t> (8, (int64_t) std::lround (m->period / r));
            lastFloor = m->voiced ? clampf (ratio, 0.4f, 1.0f) : 1.0f;
        }

        const int i = (int) (now & kMask);
        const float w = wsum[i];
        const float norm = 1.0f / std::max (w, lastFloor);
        outL = acc[0][i] * norm;
        outR = chans == 2 ? acc[1][i] * norm : outL;
        acc[0][i] = acc[1][i] = 0.0f; wsum[i] = 0.0f;
    }

private:
    inline void placeGrain (const PsolaAnalyzer& an, const PsolaAnalyzer::Mark& m, int64_t ts, int64_t now)
    {
        const int P = m.period;
        const float invP = kPi / (float) P;
        const float g = m.voiced ? 1.0f : unvoicedGain;
        for (int k = std::max (-P + 1, (int) (now - ts)); k < P; ++k)
        {
            const float w = 0.5f * (1.0f + std::cos (invP * (float) k));
            const int oi = (int) ((ts + k) & kMask);
            const int64_t ip = m.pos + k;
            if (chans == 2)
            {
                acc[0][oi] += w * g * an.sample (0, ip);
                acc[1][oi] += w * g * an.sample (1, ip);
            }
            else
                acc[0][oi] += w * g * an.sample (2, ip);
            wsum[oi] += w;
        }
    }

    int chans = 2;
    float ratio = 1.0f, lastFloor = 1.0f, unvoicedGain = 1.0f;
    int64_t nextTs = -1;
    float acc[2][kRing] {};
    float wsum[kRing] {};
};
} // namespace vf
