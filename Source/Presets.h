#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace vfp
{
struct FactoryPreset
{
    juce::String name, category;
    std::vector<std::pair<juce::String, float>> values; // real (unnormalised) values; everything else = default
};

const std::vector<FactoryPreset>& factoryPresets();

// set a parameter by real value (notifies host, undoable)
void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float realValue);
void resetToDefaults (juce::AudioProcessorValueTreeState& s);
void applyFactoryPreset (juce::AudioProcessorValueTreeState& s, int index);

juce::File userPresetFolder();
bool saveUserPreset (juce::AudioProcessorValueTreeState& s, const juce::String& name);
bool loadUserPreset (juce::AudioProcessorValueTreeState& s, const juce::File& file);
juce::Array<juce::File> userPresetFiles();
} // namespace vfp
