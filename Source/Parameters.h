#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Engine.h"

namespace vfp
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

const juce::StringArray& keyChoices();
const juce::StringArray& scaleChoices();

// Fast access to all parameter values on the audio thread
class ParamCache
{
public:
    explicit ParamCache (juce::AudioProcessorValueTreeState& s);
    void fill (vf::EngineParams& p) const;
    bool trackingMode() const;

private:
    juce::AudioProcessorValueTreeState& state;
    // std::string keys (all ids are short → SSO, no allocation on the audio thread)
    std::unordered_map<std::string, std::atomic<float>*> raw;
    std::string noteIds[12], bandIds[4][3], voiceIds[8][3];
    float get (const std::string& id) const;
    bool on (const std::string& id) const { return get (id) > 0.5f; }
    int idx (const std::string& id) const { return (int) std::lround (get (id)); }
};

// voice parameter ids
inline juce::String voiceId (int v, const char* what) { return "v" + juce::String (v + 1) + "_" + what; }
} // namespace vfp
