#include "Parameters.h"

namespace vfp
{
using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using API = juce::AudioParameterInt;
using NR = juce::NormalisableRange<float>;

const juce::StringArray& keyChoices()
{
    static juce::StringArray k { "Auto", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return k;
}
const juce::StringArray& scaleChoices()
{
    static juce::StringArray s { "Auto", "Chromatic", "Major", "Minor", "Harmonic Minor", "Melodic Minor", "Dorian",
                                 "Phrygian", "Lydian", "Mixolydian", "Pentatonic Maj", "Pentatonic Min", "Blues",
                                 "Phrygian Dom", "Custom" };
    return s;
}

static NR skewed (float lo, float hi, float centre)
{
    NR r (lo, hi); r.setSkewForCentre (centre); return r;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout L;
    auto pid = [] (const juce::String& id) { return juce::ParameterID { id, 1 }; };
    auto fl = [&] (const juce::String& id, const juce::String& name, NR r, float def, const juce::String& unit = {})
    {
        L.add (std::make_unique<APF> (pid (id), name, r, def, juce::AudioParameterFloatAttributes().withLabel (unit)));
    };
    auto bo = [&] (const juce::String& id, const juce::String& name, bool def) { L.add (std::make_unique<APB> (pid (id), name, def)); };
    auto ch = [&] (const juce::String& id, const juce::String& name, const juce::StringArray& c, int def) { L.add (std::make_unique<APC> (pid (id), name, c, def)); };
    auto in = [&] (const juce::String& id, const juce::String& name, int lo, int hi, int def) { L.add (std::make_unique<API> (pid (id), name, lo, hi, def)); };

    // global
    ch ("mode", "Mode", { "Mixing", "Tracking" }, 0);
    fl ("input", "Input", NR (-24, 24, 0.1f), 0, "dB");
    fl ("output", "Output", NR (-24, 24, 0.1f), 0, "dB");
    fl ("mix", "Mix", NR (0, 100, 0.1f), 100, "%");

    // tune
    bo ("tune_on", "Tune On", true);
    ch ("tune_key", "Key", keyChoices(), 0);
    ch ("tune_scale", "Scale", scaleChoices(), 0);
    static const char* pcs[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i)
        bo ("tune_note_" + juce::String (i), juce::String ("Custom ") + pcs[i], (0x0AB5 >> i) & 1);
    fl ("tune_speed", "Retune Speed", skewed (0, 400, 40), 20, "ms");
    fl ("tune_tolerance", "Tolerance", NR (0, 50, 0.1f), 0, "ct");
    fl ("tune_amount", "Amount", NR (0, 100, 0.1f), 100, "%");
    fl ("tune_humanize", "Humanize", NR (0, 100, 0.1f), 0, "%");
    fl ("tune_transpose", "Transpose", NR (-12, 12, 1), 0, "st");
    bo ("tune_midi", "MIDI Target", false);

    // gate
    bo ("gate_on", "Gate On", false);
    fl ("gate_thresh", "Gate Threshold", NR (-90, 0, 0.1f), -55, "dB");
    fl ("gate_range", "Gate Range", NR (0, 90, 0.1f), 40, "dB");
    fl ("gate_attack", "Gate Attack", skewed (0.1f, 50, 3), 1, "ms");
    fl ("gate_release", "Gate Release", skewed (10, 1000, 150), 120, "ms");

    // sculpt
    bo ("sculpt_on", "Sculpt On", false);
    fl ("sculpt_amount", "Sculpt Amount", NR (0, 100, 0.1f), 50, "%");
    fl ("sculpt_tilt", "Sculpt Tilt", NR (-6, 6, 0.1f), 0, "dB");

    // de-esser
    bo ("deess_on", "De-Esser On", false);
    fl ("deess_amount", "De-Ess Amount", NR (0, 100, 0.1f), 40, "%");
    fl ("deess_freq", "De-Ess Freq", skewed (3000, 12000, 6500), 6500, "Hz");

    // eq
    bo ("eq_on", "EQ On", false);
    bo ("eq_hp_on", "HP On", true);
    fl ("eq_hp_freq", "HP Freq", skewed (20, 500, 100), 80, "Hz");
    bo ("eq_lp_on", "LP On", false);
    fl ("eq_lp_freq", "LP Freq", skewed (2000, 20000, 9000), 18000, "Hz");
    fl ("eq_ls_freq", "Low Shelf Freq", skewed (30, 500, 120), 120, "Hz");
    fl ("eq_ls_gain", "Low Shelf Gain", NR (-15, 15, 0.1f), 0, "dB");
    const float bf[4] = { 250, 1000, 3000, 7000 };
    for (int b = 0; b < 4; ++b)
    {
        const juce::String n (b + 1);
        fl ("eq_b" + n + "_freq", "Band " + n + " Freq", skewed (40, 18000, 1000), bf[b], "Hz");
        fl ("eq_b" + n + "_gain", "Band " + n + " Gain", NR (-15, 15, 0.1f), 0, "dB");
        fl ("eq_b" + n + "_q", "Band " + n + " Q", skewed (0.2f, 10, 1), 1);
    }
    fl ("eq_hs_freq", "High Shelf Freq", skewed (2000, 18000, 8000), 10000, "Hz");
    fl ("eq_hs_gain", "High Shelf Gain", NR (-15, 15, 0.1f), 0, "dB");
    ch ("eq_solo", "EQ Solo", { "Off", "Low Shelf", "Band 1", "Band 2", "Band 3", "Band 4", "High Shelf" }, 0);

    // compressor
    bo ("comp_on", "Comp On", false);
    ch ("comp_mode", "Comp Mode", { "Tube", "Stress", "Multi" }, 0);
    fl ("comp_input", "Comp Input", NR (0, 30, 0.1f), 6, "dB");
    fl ("comp_attack", "Comp Attack", skewed (0.05f, 50, 5), 5, "ms");
    fl ("comp_release", "Comp Release", skewed (20, 3000, 200), 120, "ms");
    ch ("comp_ratio", "Comp Ratio", { "2:1", "4:1", "6:1", "10:1", "20:1" }, 1);
    fl ("comp_depth", "Multi Depth", NR (0, 100, 0.1f), 50, "%");
    fl ("comp_mix", "Comp Mix", NR (0, 100, 0.1f), 100, "%");
    fl ("comp_output", "Comp Output", NR (-18, 18, 0.1f), 0, "dB");

    // saturation
    bo ("sat_on", "Sat On", false);
    ch ("sat_mode", "Sat Mode", { "Tape", "Tube", "Digital" }, 0);
    fl ("sat_drive", "Sat Drive", NR (0, 30, 0.1f), 6, "dB");
    fl ("sat_tone", "Sat Tone", NR (0, 100, 0.1f), 50, "%");
    fl ("sat_bits", "Bits", NR (2, 16, 0.1f), 10);
    in ("sat_rate", "Downsample", 1, 32, 1);
    fl ("sat_mix", "Sat Mix", NR (0, 100, 0.1f), 100, "%");
    fl ("sat_output", "Sat Output", NR (-18, 18, 0.1f), 0, "dB");

    // harmonies
    bo ("harm_on", "Harmonies On", false);
    ch ("harm_mode", "Harmony Mode", { "Scale", "Chromatic", "MIDI" }, 0);
    in ("harm_voices", "Voices", 1, 8, 2);
    bo ("harm_link", "Link Pairs", false);
    bo ("harm_dimute", "DI Mute", false);
    fl ("harm_level", "Harmony Level", NR (-36, 6, 0.1f), -6, "dB");
    fl ("harm_humanize", "Harmony Humanize", NR (0, 100, 0.1f), 30, "%");
    fl ("harm_width", "Harmony Width", NR (0, 100, 0.1f), 100, "%");
    const int defIv[8] = { 2, -3, 4, -5, 7, -7, 5, 9 };
    const float defPan[8] = { -60, 60, -30, 30, -90, 90, -15, 15 };
    for (int v = 0; v < 8; ++v)
    {
        const juce::String n (v + 1);
        in (voiceId (v, "interval"), "Voice " + n + " Interval", -14, 14, defIv[v]);
        fl (voiceId (v, "level"), "Voice " + n + " Level", NR (-36, 6, 0.1f), 0, "dB");
        fl (voiceId (v, "pan"), "Voice " + n + " Pan", NR (-100, 100, 1), defPan[v]);
    }

    // delay
    bo ("dly_on", "Delay On", false);
    ch ("dly_mode", "Delay Mode", { "Analog", "Circular", "Tape", "Dual" }, 0);
    bo ("dly_sync", "Delay Sync", true);
    ch ("dly_div", "Delay Division", { "1/2", "1/4.", "1/4", "1/4T", "1/8.", "1/8", "1/8T", "1/16" }, 5);
    ch ("dly_div_r", "Delay Division R", { "1/2", "1/4.", "1/4", "1/4T", "1/8.", "1/8", "1/8T", "1/16" }, 4);
    fl ("dly_time_l", "Delay Time L", skewed (1, 2000, 350), 375, "ms");
    fl ("dly_time_r", "Delay Time R", skewed (1, 2000, 350), 500, "ms");
    fl ("dly_fb", "Delay Feedback", NR (0, 95, 0.1f), 35, "%");
    fl ("dly_tone", "Delay Tone", NR (0, 100, 0.1f), 50, "%");
    fl ("dly_duck", "Delay Duck", NR (0, 100, 0.1f), 50, "%");
    fl ("dly_mix", "Delay Mix", NR (0, 100, 0.1f), 20, "%");

    // reverb
    bo ("rev_on", "Reverb On", false);
    ch ("rev_mode", "Reverb Mode", { "Hall", "Plate", "Ambiance" }, 0);
    fl ("rev_size", "Reverb Size", NR (0, 100, 0.1f), 60, "%");
    fl ("rev_decay", "Reverb Decay", skewed (0.2f, 12, 2.5f), 2.5f, "s");
    fl ("rev_pre", "Pre-Delay", NR (0, 200, 0.1f), 20, "ms");
    fl ("rev_damp", "Damping", NR (0, 100, 0.1f), 50, "%");
    fl ("rev_lowcut", "Reverb Low Cut", skewed (20, 1000, 200), 150, "Hz");
    fl ("rev_duck", "Reverb Duck", NR (0, 100, 0.1f), 30, "%");
    fl ("rev_mix", "Reverb Mix", NR (0, 100, 0.1f), 18, "%");

    // modulation
    bo ("mod_on", "Mod On", false);
    ch ("mod_mode", "Mod Mode", { "Chorus", "Flanger", "Phaser" }, 0);
    fl ("mod_rate", "Mod Rate", skewed (0.05f, 10, 1), 0.6f, "Hz");
    fl ("mod_depth", "Mod Depth", NR (0, 100, 0.1f), 50, "%");
    fl ("mod_fb", "Mod Feedback", NR (-90, 90, 0.1f), 30, "%");
    fl ("mod_mix", "Mod Mix", NR (0, 100, 0.1f), 35, "%");

    // doubler
    bo ("dbl_on", "Doubler On", false);
    fl ("dbl_amount", "Doubler Amount", NR (0, 100, 0.1f), 50, "%");
    fl ("dbl_timing", "Doubler Timing", NR (0, 100, 0.1f), 50, "%");
    fl ("dbl_width", "Doubler Width", NR (0, 100, 0.1f), 80, "%");
    ch ("dbl_char", "Doubler Character", { "Male", "Female" }, 0);

    return L;
}

// ------------------------------------------------------------------ cache
ParamCache::ParamCache (juce::AudioProcessorValueTreeState& s) : state (s)
{
    for (auto* p : s.processor.getParameters())
        if (auto* wp = dynamic_cast<juce::RangedAudioParameter*> (p))
            raw[wp->getParameterID().toStdString()] = s.getRawParameterValue (wp->getParameterID());
    for (int i = 0; i < 12; ++i) noteIds[i] = "tune_note_" + std::to_string (i);
    for (int b = 0; b < 4; ++b)
    {
        const std::string n = std::to_string (b + 1);
        bandIds[b][0] = "eq_b" + n + "_freq"; bandIds[b][1] = "eq_b" + n + "_gain"; bandIds[b][2] = "eq_b" + n + "_q";
    }
    for (int v = 0; v < 8; ++v)
    {
        voiceIds[v][0] = voiceId (v, "interval").toStdString();
        voiceIds[v][1] = voiceId (v, "level").toStdString();
        voiceIds[v][2] = voiceId (v, "pan").toStdString();
    }
}

float ParamCache::get (const std::string& id) const
{
    auto it = raw.find (id);
    jassert (it != raw.end());
    return it != raw.end() && it->second != nullptr ? it->second->load() : 0.0f;
}

bool ParamCache::trackingMode() const { return idx ("mode") == 1; }

void ParamCache::fill (vf::EngineParams& p) const
{
    p.trackingMode = trackingMode();
    p.inputDb = get ("input"); p.outputDb = get ("output"); p.mix = get ("mix") * 0.01f;

    p.tuneOn = on ("tune_on");
    p.keyRoot = idx ("tune_key") - 1;
    p.scale = idx ("tune_scale");
    uint16_t m = 0;
    for (int i = 0; i < 12; ++i) if (on (noteIds[i])) m |= (uint16_t) (1u << i);
    p.customMask = m;
    p.retuneMs = get ("tune_speed"); p.toleranceCents = get ("tune_tolerance");
    p.amount = get ("tune_amount") * 0.01f; p.humanize = get ("tune_humanize") * 0.01f;
    p.transpose = get ("tune_transpose"); p.midiTune = on ("tune_midi");

    p.gateOn = on ("gate_on"); p.gateThreshDb = get ("gate_thresh"); p.gateRangeDb = get ("gate_range");
    p.gateAttackMs = get ("gate_attack"); p.gateReleaseMs = get ("gate_release");

    p.sculptOn = on ("sculpt_on"); p.sculptAmount = get ("sculpt_amount") * 0.01f; p.sculptTilt = get ("sculpt_tilt");
    p.deEssOn = on ("deess_on"); p.deEssAmount = get ("deess_amount") * 0.01f; p.deEssFreq = get ("deess_freq");

    p.eqOn = on ("eq_on");
    p.eq.hpOn = on ("eq_hp_on"); p.eq.hpFreq = get ("eq_hp_freq");
    p.eq.lpOn = on ("eq_lp_on"); p.eq.lpFreq = get ("eq_lp_freq");
    p.eq.lsFreq = get ("eq_ls_freq"); p.eq.lsGain = get ("eq_ls_gain");
    for (int b = 0; b < 4; ++b)
    {
        p.eq.bFreq[b] = get (bandIds[b][0]); p.eq.bGain[b] = get (bandIds[b][1]); p.eq.bQ[b] = get (bandIds[b][2]);
    }
    p.eq.hsFreq = get ("eq_hs_freq"); p.eq.hsGain = get ("eq_hs_gain");
    p.eq.solo = p.eqOn ? idx ("eq_solo") : 0;

    static const float ratios[5] = { 2, 4, 6, 10, 20 };
    p.compOn = on ("comp_on"); p.compMode = idx ("comp_mode"); p.compInput = get ("comp_input");
    p.compAttack = get ("comp_attack"); p.compRelease = get ("comp_release");
    p.compRatio = ratios[juce::jlimit (0, 4, idx ("comp_ratio"))];
    p.compDepth = get ("comp_depth") * 0.01f; p.compMix = get ("comp_mix") * 0.01f; p.compOutput = get ("comp_output");

    p.satOn = on ("sat_on"); p.satMode = idx ("sat_mode"); p.satDrive = get ("sat_drive"); p.satTone = get ("sat_tone") * 0.01f;
    p.satBits = get ("sat_bits"); p.satDownsample = idx ("sat_rate"); p.satMix = get ("sat_mix") * 0.01f; p.satOutput = get ("sat_output");

    p.harmOn = on ("harm_on"); p.harmMode = idx ("harm_mode"); p.harmVoices = idx ("harm_voices");
    p.harmLink = on ("harm_link"); p.diMute = on ("harm_dimute"); p.harmLevelDb = get ("harm_level");
    p.harmHumanize = get ("harm_humanize") * 0.01f; p.harmWidth = get ("harm_width") * 0.01f;
    for (int v = 0; v < 8; ++v)
    {
        p.voiceInterval[v] = idx (voiceIds[v][0]);
        p.voiceLevelDb[v] = get (voiceIds[v][1]);
        p.voicePan[v] = get (voiceIds[v][2]) * 0.01f;
    }

    p.dlyOn = on ("dly_on"); p.dlyMode = idx ("dly_mode");
    p.dlyTimeL = get ("dly_time_l"); p.dlyTimeR = get ("dly_time_r");
    p.dlyFb = get ("dly_fb") * 0.01f; p.dlyTone = get ("dly_tone") * 0.01f; p.dlyDuck = get ("dly_duck") * 0.01f; p.dlyMix = get ("dly_mix") * 0.01f;

    p.revOn = on ("rev_on"); p.revMode = idx ("rev_mode"); p.revSize = get ("rev_size") * 0.01f; p.revDecay = get ("rev_decay");
    p.revPre = get ("rev_pre"); p.revDamp = get ("rev_damp") * 0.01f; p.revLowCut = get ("rev_lowcut");
    p.revDuck = get ("rev_duck") * 0.01f; p.revMix = get ("rev_mix") * 0.01f;

    p.modOn = on ("mod_on"); p.modMode = idx ("mod_mode"); p.modRate = get ("mod_rate"); p.modDepth = get ("mod_depth") * 0.01f;
    p.modFb = get ("mod_fb") * 0.01f; p.modMix = get ("mod_mix") * 0.01f;

    p.dblOn = on ("dbl_on"); p.dblAmount = get ("dbl_amount") * 0.01f; p.dblTiming = get ("dbl_timing") * 0.01f;
    p.dblWidth = get ("dbl_width") * 0.01f; p.dblFemale = idx ("dbl_char") == 1;
}
} // namespace vfp
