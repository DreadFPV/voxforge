// VoxForge – shared DSP helpers (pure C++, no JUCE dependency)
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <complex>
#include <array>

namespace vf
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g, float floorDb = -120.0f)
{
    return g > 1.0e-6f ? std::max (floorDb, 20.0f * std::log10 (g)) : floorDb;
}
inline float clampf (float v, float lo, float hi) { return std::min (hi, std::max (lo, v)); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
inline float timeCoef (float ms, double sr)
{
    if (ms <= 0.0f) return 0.0f;
    return (float) std::exp (-1.0 / (0.001 * ms * sr));
}
inline float freqToMidi (float hz) { return 69.0f + 12.0f * std::log2 (hz / 440.0f); }
inline float midiToFreq (float m) { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }

// ---------------------------------------------------------------- smoothing
struct Smoother
{
    float cur = 0, target = 0, coef = 0.999f;
    void setTime (float ms, double sr) { coef = timeCoef (ms, sr); }
    void reset (float v) { cur = target = v; }
    inline float next() { cur = target + (cur - target) * coef; return cur; }
};

// ---------------------------------------------------------------- envelope
struct EnvFollower
{
    float env = 0, att = 0, rel = 0;
    void setup (float attackMs, float releaseMs, double sr)
    {
        att = timeCoef (attackMs, sr);
        rel = timeCoef (releaseMs, sr);
    }
    inline float process (float x)
    {
        const float a = std::fabs (x);
        env = a > env ? a + (env - a) * att : a + (env - a) * rel;
        return env;
    }
    void reset() { env = 0; }
};

// ---------------------------------------------------------------- biquad (RBJ, TDF-II, 2 ch)
struct Biquad
{
    enum Type { LowPass, HighPass, BandPass, Peak, LowShelf, HighShelf, AllPass, Notch };
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] { 0, 0 }, z2[2] { 0, 0 };

    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }

    void set (Type t, double sr, float freq, float q, float gainDb = 0.0f)
    {
        freq = clampf (freq, 10.0f, (float) (sr * 0.49));
        q = std::max (0.05f, q);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * M_PI * freq / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        double B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;
        switch (t)
        {
            case LowPass:  B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = (1 - cw) / 2; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            case HighPass: B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            case BandPass: B0 = alpha; B1 = 0; B2 = -alpha; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            case Notch:    B0 = 1; B1 = -2 * cw; B2 = 1; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            case AllPass:  B0 = 1 - alpha; B1 = -2 * cw; B2 = 1 + alpha; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            case Peak:
                B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A;
                A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A; break;
            case LowShelf:
            {
                const double sq = 2 * std::sqrt (A) * alpha;
                B0 = A * ((A + 1) - (A - 1) * cw + sq);
                B1 = 2 * A * ((A - 1) - (A + 1) * cw);
                B2 = A * ((A + 1) - (A - 1) * cw - sq);
                A0 = (A + 1) + (A - 1) * cw + sq;
                A1 = -2 * ((A - 1) + (A + 1) * cw);
                A2 = (A + 1) + (A - 1) * cw - sq; break;
            }
            case HighShelf:
            {
                const double sq = 2 * std::sqrt (A) * alpha;
                B0 = A * ((A + 1) + (A - 1) * cw + sq);
                B1 = -2 * A * ((A - 1) + (A + 1) * cw);
                B2 = A * ((A + 1) + (A - 1) * cw - sq);
                A0 = (A + 1) - (A - 1) * cw + sq;
                A1 = 2 * ((A - 1) - (A + 1) * cw);
                A2 = (A + 1) - (A - 1) * cw - sq; break;
            }
        }
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }

    inline float process (float x, int ch = 0)
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
};

// ---------------------------------------------------------------- one-pole
struct OnePole
{
    float a = 0, z[2] { 0, 0 };
    void setLowpass (float hz, double sr) { a = (float) std::exp (-2.0 * M_PI * hz / sr); }
    void reset() { z[0] = z[1] = 0; }
    inline float lp (float x, int ch = 0) { z[ch] = x + (z[ch] - x) * a; return z[ch]; }
    inline float hp (float x, int ch = 0) { return x - lp (x, ch); }
};

// ---------------------------------------------------------------- delay line (fractional read)
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void prepare (int maxSamples)
    {
        int n = 1;
        while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1; w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // delay in samples (>= 1)
    inline float read (float d) const
    {
        d = std::max (1.0f, std::min (d, (float) (mask - 2)));
        const float rp = (float) w - d;
        int i0 = (int) std::floor (rp);
        const float f = rp - (float) i0;
        const float a = buf[(size_t) ((i0 - 1) & mask)], b = buf[(size_t) (i0 & mask)];
        const float c = buf[(size_t) ((i0 + 1) & mask)], e = buf[(size_t) ((i0 + 2) & mask)];
        // 4-point cubic (Hermite)
        const float c0 = b, c1 = 0.5f * (c - a), c2 = a - 2.5f * b + 2.0f * c - 0.5f * e;
        const float c3 = 0.5f * (e - a) + 1.5f * (b - c);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }
    inline float readInt (int d) const { return buf[(size_t) ((w - d) & mask)]; }
};

// ---------------------------------------------------------------- allpass diffuser
struct AllpassDiffuser
{
    DelayLine d; int len = 1; float g = 0.6f;
    void prepare (int n, float gain) { d.prepare (n + 2); len = n; g = gain; }
    void set (int n, float gain) { len = std::max (1, std::min (n, d.mask - 2)); g = gain; }
    inline float process (float x)
    {
        const float v = d.readInt (len);
        const float in = x + g * v;
        d.push (in);
        return v - g * in;
    }
};

// ---------------------------------------------------------------- random (xorshift)
struct Rng
{
    uint32_t s = 0x12345678u;
    explicit Rng (uint32_t seed = 0x12345678u) : s (seed ? seed : 1u) {}
    inline float next01() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xFFFFFF) / 16777216.0f; }
    inline float nextBi() { return next01() * 2.0f - 1.0f; }
};

// smooth random walk (for humanise / doubler drift)
struct SmoothRandom
{
    Rng rng; float cur = 0, target = 0, coef = 0.999f; int counter = 0, period = 1000;
    explicit SmoothRandom (uint32_t seed = 1) : rng (seed) {}
    void setup (float rateHz, double sr)
    {
        period = std::max (1, (int) (sr / std::max (0.01f, rateHz)));
        coef = (float) std::exp (-1.0 / (period * 0.5));
    }
    inline float next()
    {
        if (--counter <= 0) { counter = period; target = rng.nextBi(); }
        cur = target + (cur - target) * coef;
        return cur;
    }
};

// ---------------------------------------------------------------- FFT (radix-2, in place)
inline void fft (std::vector<std::complex<float>>& a)
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
        const float ang = -kTwoPi / (float) len;
        const std::complex<float> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<float> w (1.0f, 0.0f);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// ---------------------------------------------------------------- scales
enum class ScaleType
{
    Chromatic = 0, Major, Minor, HarmonicMinor, MelodicMinor, Dorian, Phrygian, Lydian, Mixolydian,
    PentatonicMajor, PentatonicMinor, Blues, PhrygianDominant, Custom, NumScales
};

inline uint16_t scaleMask (ScaleType t)
{
    auto m = [] (std::initializer_list<int> notes) { uint16_t r = 0; for (int n : notes) r |= (uint16_t) (1u << n); return r; };
    switch (t)
    {
        case ScaleType::Chromatic:        return 0x0FFF;
        case ScaleType::Major:            return m ({ 0, 2, 4, 5, 7, 9, 11 });
        case ScaleType::Minor:            return m ({ 0, 2, 3, 5, 7, 8, 10 });
        case ScaleType::HarmonicMinor:    return m ({ 0, 2, 3, 5, 7, 8, 11 });
        case ScaleType::MelodicMinor:     return m ({ 0, 2, 3, 5, 7, 9, 11 });
        case ScaleType::Dorian:           return m ({ 0, 2, 3, 5, 7, 9, 10 });
        case ScaleType::Phrygian:         return m ({ 0, 1, 3, 5, 7, 8, 10 });
        case ScaleType::Lydian:           return m ({ 0, 2, 4, 6, 7, 9, 11 });
        case ScaleType::Mixolydian:       return m ({ 0, 2, 4, 5, 7, 9, 10 });
        case ScaleType::PentatonicMajor:  return m ({ 0, 2, 4, 7, 9 });
        case ScaleType::PentatonicMinor:  return m ({ 0, 3, 5, 7, 10 });
        case ScaleType::Blues:            return m ({ 0, 3, 5, 6, 7, 10 });
        case ScaleType::PhrygianDominant: return m ({ 0, 1, 4, 5, 7, 8, 10 });
        default:                          return 0x0FFF;
    }
}

// Mask (relative to root) → absolute pitch-class mask
inline uint16_t rotateMask (uint16_t rel, int root)
{
    uint16_t r = 0;
    for (int i = 0; i < 12; ++i)
        if (rel & (1u << i)) r |= (uint16_t) (1u << ((i + root) % 12));
    return r;
}

// nearest note (float midi) that is in the absolute mask
inline int quantizeToMask (float midi, uint16_t absMask)
{
    if (absMask == 0) absMask = 0x0FFF;
    const int base = (int) std::floor (midi);
    int best = base; float bestD = 1e9f;
    for (int n = base - 12; n <= base + 13; ++n)
    {
        const int pc = ((n % 12) + 12) % 12;
        if (! (absMask & (1u << pc))) continue;
        const float d = std::fabs ((float) n - midi);
        if (d < bestD) { bestD = d; best = n; }
    }
    return best;
}

// move `steps` scale degrees from note (note should be in mask)
inline int scaleStep (int note, int steps, uint16_t absMask)
{
    if (absMask == 0) absMask = 0x0FFF;
    int n = note;
    const int dir = steps > 0 ? 1 : -1;
    for (int s = 0; s != steps; s += dir)
    {
        int guard = 0;
        do { n += dir; } while (! (absMask & (1u << (((n % 12) + 12) % 12))) && ++guard < 13);
    }
    return n;
}

inline const char* noteName (int pc)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[((pc % 12) + 12) % 12];
}
} // namespace vf
