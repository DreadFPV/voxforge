#include "Presets.h"

namespace vfp
{
const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets {
        { "Init", "Basic", {} },
        { "Natural Lead", "Vocal",
          { { "tune_speed", 45 }, { "tune_humanize", 40 }, { "tune_tolerance", 8 },
            { "gate_on", 1 }, { "gate_thresh", -55 }, { "sculpt_on", 1 }, { "sculpt_amount", 40 },
            { "deess_on", 1 }, { "deess_amount", 35 }, { "eq_on", 1 }, { "eq_hp_freq", 90 },
            { "comp_on", 1 }, { "comp_mode", 0 }, { "comp_input", 8 }, { "rev_on", 1 }, { "rev_mode", 1 }, { "rev_mix", 12 } } },
        { "Hard Tune", "Vocal",
          { { "tune_speed", 0 }, { "tune_humanize", 0 }, { "tune_amount", 100 },
            { "deess_on", 1 }, { "deess_amount", 45 }, { "comp_on", 1 }, { "comp_mode", 1 }, { "comp_input", 12 },
            { "comp_ratio", 2 }, { "dly_on", 1 }, { "dly_div", 5 }, { "dly_mix", 14 }, { "rev_on", 1 }, { "rev_mix", 14 } } },
        { "Pop Lead Polish", "Vocal",
          { { "tune_speed", 15 }, { "tune_humanize", 25 }, { "gate_on", 1 }, { "sculpt_on", 1 }, { "sculpt_amount", 60 },
            { "sculpt_tilt", 1.5f }, { "deess_on", 1 }, { "deess_amount", 50 }, { "eq_on", 1 }, { "eq_hp_freq", 100 },
            { "eq_b3_gain", 2 }, { "eq_hs_gain", 3 }, { "comp_on", 1 }, { "comp_mode", 0 }, { "comp_input", 10 },
            { "dbl_on", 1 }, { "dbl_amount", 30 }, { "dly_on", 1 }, { "dly_div", 4 }, { "dly_mix", 12 }, { "dly_duck", 70 },
            { "rev_on", 1 }, { "rev_mode", 1 }, { "rev_mix", 16 }, { "rev_duck", 50 } } },
        { "Choir Stack", "Harmonies",
          { { "tune_speed", 25 }, { "harm_on", 1 }, { "harm_voices", 6 }, { "harm_mode", 0 },
            { "v1_interval", 2 }, { "v2_interval", 4 }, { "v3_interval", -3 }, { "v4_interval", 7 }, { "v5_interval", -7 }, { "v6_interval", 9 },
            { "harm_level", -8 }, { "harm_humanize", 60 }, { "rev_on", 1 }, { "rev_mode", 0 }, { "rev_mix", 28 }, { "rev_decay", 4 } } },
        { "Wide Thirds", "Harmonies",
          { { "harm_on", 1 }, { "harm_voices", 2 }, { "harm_link", 1 }, { "v1_interval", 2 }, { "v1_pan", -80 },
            { "harm_level", -7 }, { "harm_humanize", 45 }, { "dbl_on", 1 }, { "dbl_amount", 35 } } },
        { "MIDI Harmonizer", "Harmonies",
          { { "harm_on", 1 }, { "harm_mode", 2 }, { "harm_voices", 4 }, { "harm_level", -5 }, { "harm_humanize", 25 } } },
        { "Psy Vocal FX", "FX",
          { { "tune_speed", 0 }, { "tune_scale", 7 }, { "comp_on", 1 }, { "comp_mode", 2 }, { "comp_depth", 60 },
            { "mod_on", 1 }, { "mod_mode", 2 }, { "mod_rate", 0.3f }, { "mod_depth", 70 }, { "mod_mix", 45 },
            { "dly_on", 1 }, { "dly_mode", 1 }, { "dly_div", 4 }, { "dly_fb", 55 }, { "dly_mix", 28 }, { "dly_duck", 60 },
            { "rev_on", 1 }, { "rev_mode", 0 }, { "rev_decay", 6 }, { "rev_mix", 25 }, { "rev_duck", 60 } } },
        { "Radio Voice", "FX",
          { { "tune_on", 0 }, { "eq_on", 1 }, { "eq_hp_on", 1 }, { "eq_hp_freq", 400 }, { "eq_lp_on", 1 }, { "eq_lp_freq", 3500 },
            { "sat_on", 1 }, { "sat_mode", 2 }, { "sat_bits", 8 }, { "sat_rate", 3 }, { "sat_drive", 9 },
            { "comp_on", 1 }, { "comp_mode", 1 }, { "comp_ratio", 3 }, { "comp_input", 14 } } },
        { "Tape Warmth", "Character",
          { { "tune_speed", 60 }, { "tune_humanize", 50 }, { "sat_on", 1 }, { "sat_mode", 0 }, { "sat_drive", 10 }, { "sat_tone", 40 },
            { "comp_on", 1 }, { "comp_mode", 0 }, { "comp_input", 9 }, { "dly_on", 1 }, { "dly_mode", 2 }, { "dly_mix", 15 } } },
        { "Dark Ambient", "FX",
          { { "tune_speed", 80 }, { "sculpt_on", 1 }, { "sculpt_tilt", -3 }, { "dbl_on", 1 }, { "dbl_amount", 50 },
            { "mod_on", 1 }, { "mod_mode", 0 }, { "mod_mix", 40 }, { "rev_on", 1 }, { "rev_mode", 0 }, { "rev_size", 95 },
            { "rev_decay", 9 }, { "rev_mix", 40 }, { "rev_duck", 20 } } },
        { "Synth Polish", "Instrument",
          { { "tune_on", 0 }, { "sculpt_on", 1 }, { "sculpt_amount", 35 }, { "comp_on", 1 }, { "comp_mode", 2 }, { "comp_depth", 35 },
            { "mod_on", 1 }, { "mod_mode", 0 }, { "mod_mix", 25 }, { "dly_on", 1 }, { "dly_mode", 3 }, { "dly_mix", 15 } } },
        { "Guitar Tight", "Instrument",
          { { "tune_on", 0 }, { "gate_on", 1 }, { "gate_thresh", -50 }, { "eq_on", 1 }, { "eq_hp_freq", 90 }, { "eq_b2_gain", -2 },
            { "comp_on", 1 }, { "comp_mode", 1 }, { "comp_input", 8 }, { "rev_on", 1 }, { "rev_mode", 2 }, { "rev_mix", 15 } } },
        { "Drum Crush", "Instrument",
          { { "tune_on", 0 }, { "comp_on", 1 }, { "comp_mode", 1 }, { "comp_ratio", 4 }, { "comp_input", 14 }, { "comp_mix", 60 },
            { "sat_on", 1 }, { "sat_mode", 1 }, { "sat_drive", 12 }, { "sat_mix", 60 } } },
    };
    return presets;
}

void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float realValue)
{
    if (auto* p = s.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

void resetToDefaults (juce::AudioProcessorValueTreeState& s)
{
    for (auto* p : s.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (rp->getParameterID() == "mode") continue; // keep tracking / mixing
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
}

void applyFactoryPreset (juce::AudioProcessorValueTreeState& s, int index)
{
    const auto& all = factoryPresets();
    if (index < 0 || index >= (int) all.size()) return;
    resetToDefaults (s);
    for (auto& [id, v] : all[(size_t) index].values) setParam (s, id, v);
}

juce::File userPresetFolder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                 .getChildFile ("Library/Audio/Presets/JustCreateit/VoxForge");
    f.createDirectory();
    return f;
}

bool saveUserPreset (juce::AudioProcessorValueTreeState& s, const juce::String& name)
{
    auto xml = s.copyState().createXml();
    if (! xml) return false;
    xml->setAttribute ("presetName", name);
    return xml->writeTo (userPresetFolder().getChildFile (juce::File::createLegalFileName (name) + ".vfpreset"));
}

bool loadUserPreset (juce::AudioProcessorValueTreeState& s, const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (! xml || ! xml->hasTagName (s.state.getType())) return false;
    auto tree = juce::ValueTree::fromXml (*xml);
    // apply parameter by parameter so the host sees the changes
    for (auto child : tree)
    {
        const auto id = child.getProperty ("id").toString();
        if (id == "mode" || id.isEmpty()) continue;
        if (auto* p = s.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) child.getProperty ("value")));
            p->endChangeGesture();
        }
    }
    return true;
}

juce::Array<juce::File> userPresetFiles()
{
    auto files = userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.vfpreset");
    files.sort();
    return files;
}
} // namespace vfp
