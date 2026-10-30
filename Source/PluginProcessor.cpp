#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

VoxForgeProcessor::VoxForgeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, &undo, "VoxForge", vfp::createLayout()),
      cache (state),
      engine (std::make_unique<vf::VoxEngine>())
{
}

bool VoxForgeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void VoxForgeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (64, juce::jmin (samplesPerBlock, 2048));
    monoBuf.assign ((size_t) maxBlock, 0.0f);
    tracking = cache.trackingMode();
    cache.fill (params);
    engine->setParams (params);
    engine->prepare (sr, maxBlock, tracking);
    setLatencySamples (engine->getLatency());
}

void VoxForgeProcessor::rebuildEngineIfNeeded()
{
    // Tracking / Mixing changes the analysis window and therefore the latency
    const bool t = cache.trackingMode();
    if (t == tracking) return;
    tracking = t;
    engine->prepare (sr, maxBlock, tracking);
    setLatencySamples (engine->getLatency());
}

static float divisionBeats (int idx)
{
    static const float beats[8] = { 2.0f, 1.5f, 1.0f, 2.0f / 3.0f, 0.75f, 0.5f, 1.0f / 3.0f, 0.25f };
    return beats[juce::jlimit (0, 7, idx)];
}

void VoxForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    rebuildEngineIfNeeded();

    cache.fill (params);
    // tempo-synced delay
    if (state.getRawParameterValue ("dly_sync")->load() > 0.5f)
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm()) bpm = *b;
        const float beatMs = (float) (60000.0 / juce::jlimit (30.0, 300.0, bpm));
        params.dlyTimeL = beatMs * divisionBeats ((int) state.getRawParameterValue ("dly_div")->load());
        params.dlyTimeR = beatMs * divisionBeats ((int) state.getRawParameterValue ("dly_div_r")->load());
    }
    engine->setParams (params);

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) engine->noteOn (m.getNoteNumber());
        else if (m.isNoteOff()) engine->noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine->allNotesOff();
    }

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 1 || n == 0) return;

    float* l = buffer.getWritePointer (0);
    const bool stereoIn = buffer.getNumChannels() > 1 && numIn > 1;
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int pos = 0; pos < n; pos += maxBlock)
    {
        const int len = juce::jmin (maxBlock, n - pos);
        float* rr = r != nullptr ? r + pos : monoBuf.data();
        if (! stereoIn) std::copy (l + pos, l + pos + len, rr); // mono input → both sides
        engine->process (l + pos, rr, len);
        if (r == nullptr) // mono output bus
            for (int i = 0; i < len; ++i) l[pos + i] = 0.5f * (l[pos + i] + rr[i]);
    }
    for (int ch = 2; ch < numOut; ++ch) buffer.clear (ch, 0, n);
}

vf::AutoSettings VoxForgeProcessor::takeAnalysisResult()
{
    auto s = engine->computeAutoSettings();
    engine->acknowledgeAnalysis();
    return s;
}

void VoxForgeProcessor::applyAutoSettings (const vf::AutoSettings& s, bool applySource)
{
    if (! s.valid) return;
    undo.beginNewTransaction ("Auto Setup");
    using vf::SourceType;
    auto set = [this] (const char* id, float v) { vfp::setParam (state, id, v); };

    if (applySource)
    {
        switch (s.source)
        {
            case SourceType::Vocal:
                set ("tune_on", 1); set ("gate_on", 1); set ("sculpt_on", 1); set ("deess_on", 1); set ("eq_on", 1); set ("comp_on", 1);
                set ("comp_mode", 0); set ("dbl_char", s.female ? 1.0f : 0.0f);
                break;
            case SourceType::Synth:
                set ("tune_on", 0); set ("gate_on", 0); set ("deess_on", 0); set ("sculpt_on", 1); set ("eq_on", 1); set ("comp_on", 1);
                set ("comp_mode", 2); set ("comp_depth", 35);
                break;
            case SourceType::Guitar:
                set ("tune_on", 0); set ("gate_on", 1); set ("deess_on", 0); set ("sculpt_on", 1); set ("eq_on", 1); set ("comp_on", 1);
                set ("comp_mode", 1); set ("comp_ratio", 1);
                break;
            case SourceType::Bass:
                set ("tune_on", 0); set ("gate_on", 1); set ("deess_on", 0); set ("sculpt_on", 0); set ("eq_on", 1); set ("comp_on", 1);
                set ("comp_mode", 0); set ("harm_on", 0);
                break;
            case SourceType::Drums:
                set ("tune_on", 0); set ("harm_on", 0); set ("deess_on", 0); set ("sculpt_on", 0); set ("gate_on", 1); set ("eq_on", 1);
                set ("comp_on", 1); set ("comp_mode", 1); set ("comp_ratio", 2);
                break;
            default: break;
        }
    }

    // values from the voice analysis
    set ("gate_thresh", s.gateThresholdDb);
    if (s.source != SourceType::Drums && s.source != SourceType::Bass)
    {
        set ("deess_amount", s.deEssAmount);
        set ("sculpt_tilt", s.sculptTilt);
        set ("eq_hp_on", 1); set ("eq_hp_freq", s.hpFreq);
        set ("eq_b1_freq", s.mudFreq); set ("eq_b1_gain", s.mudCutDb); set ("eq_b1_q", 1.2f);
        set ("eq_b3_freq", 3500); set ("eq_b3_gain", s.presenceDb);
        set ("eq_hs_freq", 11000); set ("eq_hs_gain", s.airDb);
    }
    else
    {
        set ("eq_hp_on", s.source == SourceType::Bass ? 1.0f : 0.0f); set ("eq_hp_freq", 30);
    }
    set ("comp_input", s.compInputDb);

    // detected key (only when the key is set to Auto it is used automatically anyway;
    // writing it here "locks" the result)
    if (s.keyRoot >= 0)
    {
        set ("tune_key", (float) (s.keyRoot + 1));
        set ("tune_scale", s.keyMinor ? 3.0f : 2.0f);
    }
    undo.beginNewTransaction();
}

// ---------------------------------------------------------------- programs
int VoxForgeProcessor::getNumPrograms() { return (int) vfp::factoryPresets().size(); }

void VoxForgeProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms()) return;
    currentProgram = index;
    undo.beginNewTransaction ("Preset");
    vfp::applyFactoryPreset (state, index);
    presetName = vfp::factoryPresets()[(size_t) index].name;
    undo.beginNewTransaction();
}

const juce::String VoxForgeProcessor::getProgramName (int index)
{
    if (index < 0 || index >= getNumPrograms()) return {};
    return vfp::factoryPresets()[(size_t) index].name;
}

// ---------------------------------------------------------------- state
void VoxForgeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto st = state.copyState();
    st.setProperty ("presetName", presetName, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, destData);
}

void VoxForgeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            presetName = tree.getProperty ("presetName").toString();
            state.replaceState (tree);
        }
}

juce::AudioProcessorEditor* VoxForgeProcessor::createEditor() { return new VoxForgeEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VoxForgeProcessor(); }
