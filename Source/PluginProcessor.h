#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "dsp/Engine.h"

class VoxForgeProcessor : public juce::AudioProcessor
{
public:
    VoxForgeProcessor();
    ~VoxForgeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- used by the editor
    juce::AudioProcessorValueTreeState& getState() { return state; }
    juce::UndoManager& getUndo() { return undo; }
    vf::EngineMeters& meters() { return engine->meters; }
    void startAutoAnalysis() { engine->startAutoAnalysis (8.0f); }
    vf::VoxEngine::AnalysisState analysisState() const { return engine->getAnalysisState(); }
    // apply analysis results (message thread). applySource: load settings for the detected source
    vf::AutoSettings takeAnalysisResult();
    void applyAutoSettings (const vf::AutoSettings& s, bool applySource);
    void resetKeyDetection() { engine->resetKeyDetection(); }
    juce::String presetName;

private:
    void rebuildEngineIfNeeded();
    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState state;
    vfp::ParamCache cache;
    std::unique_ptr<vf::VoxEngine> engine;
    vf::EngineParams params;
    double sr = 48000; int maxBlock = 512;
    std::vector<float> monoBuf;
    bool tracking = false;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxForgeProcessor)
};
