#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

namespace vfui
{
namespace Colours
{
    const juce::Colour bg       { 0xff121418 };
    const juce::Colour panel    { 0xff1b1e24 };
    const juce::Colour panelHi  { 0xff242833 };
    const juce::Colour outline  { 0xff2e333d };
    const juce::Colour text     { 0xffe6e8ec };
    const juce::Colour textDim  { 0xff8a919e };
    const juce::Colour accent   { 0xffff7a2f }; // forge orange
    const juce::Colour accent2  { 0xff38d4c4 }; // teal
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f)); }
    juce::Font getLabelFont (juce::Label&) override { return juce::Font (juce::FontOptions (12.0f)); }
};

// one control bound to a parameter (knob, combo or toggle — picked from the parameter type)
class ParamControl : public juce::Component
{
public:
    ParamControl (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& label);
    void resized() override;
    bool isKnob() const { return slider != nullptr; }
    bool isToggle() const { return toggle != nullptr; }
private:
    juce::Label caption;
    std::unique_ptr<juce::Slider> slider;
    std::unique_ptr<juce::ComboBox> combo;
    std::unique_ptr<juce::ToggleButton> toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
};

// a module (Tune, Gate, EQ ...) with power button and a grid of controls
class ModulePanel : public juce::Component
{
public:
    ModulePanel (juce::AudioProcessorValueTreeState& s, const juce::String& title, const juce::String& powerId, int columns);
    ParamControl& add (const juce::String& id, const juce::String& label);
    void addCustom (juce::Component* c) { addAndMakeVisible (c); customs.push_back (c); }
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<juce::String()> statusText; // e.g. gain reduction
    juce::Rectangle<int> contentArea() const;
private:
    juce::AudioProcessorValueTreeState& state;
    juce::String title;
    int cols;
    std::unique_ptr<juce::ToggleButton> power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    std::vector<std::unique_ptr<ParamControl>> controls;
    std::vector<juce::Component*> customs;
};

// scrolling pitch graph + current note readout
class PitchDisplay : public juce::Component
{
public:
    explicit PitchDisplay (vf::EngineMeters& m) : meters (m) {}
    void paint (juce::Graphics&) override;
private:
    vf::EngineMeters& meters;
};

// 12 note toggles for the custom scale
class NoteGrid : public juce::Component
{
public:
    explicit NoteGrid (juce::AudioProcessorValueTreeState& s);
    void resized() override;
private:
    juce::OwnedArray<juce::TextButton> buttons;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> atts;
};
} // namespace vfui

class VoxForgeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoxForgeEditor (VoxForgeProcessor&);
    ~VoxForgeEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildPresetMenu();
    void loadPresetFromMenu (int id);
    void stepPreset (int delta);

    VoxForgeProcessor& proc;
    juce::AudioProcessorValueTreeState& state;
    vfui::LookAndFeel lnf;

    // header
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "Save" }, undoBtn { "Undo" }, redoBtn { "Redo" };
    std::unique_ptr<vfui::ParamControl> modeCtl, inputCtl, outputCtl, mixCtl;

    // recognition bar
    juce::TextButton analyseBtn { "Auto-Analyse" }, keyResetBtn { "Re-learn Key" };
    juce::ToggleButton applySourceBtn { "Load settings for detected source" };
    juce::Label keyLabel, sourceLabel, analysisInfo;
    vfui::PitchDisplay pitchDisplay;

    // pages
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::Component correctionPage, harmonyPage, effectsPage;
    std::vector<std::unique_ptr<vfui::ModulePanel>> panels;
    std::unique_ptr<vfui::NoteGrid> noteGrid;

    vfui::ModulePanel* tunePanel = nullptr;
    vfui::ModulePanel *gatePanel = nullptr, *sculptPanel = nullptr, *deessPanel = nullptr, *eqPanel = nullptr, *compPanel = nullptr, *satPanel = nullptr;
    vfui::ModulePanel *harmPanel = nullptr;
    std::vector<vfui::ModulePanel*> voicePanels;
    vfui::ModulePanel *dlyPanel = nullptr, *revPanel = nullptr, *modPanel = nullptr, *dblPanel = nullptr;

    std::vector<juce::File> userFiles;
    int lastAnalysisState = 0;
    int transactionCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxForgeEditor)
};
