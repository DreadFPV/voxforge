#include "PluginEditor.h"
#include "Presets.h"

using namespace vfui;

// =====================================================================================  LookAndFeel
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Colours::bg);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, Colours::panelHi);
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::outlineColourId, Colours::outline);
    setColour (juce::ComboBox::arrowColourId, Colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent.withAlpha (0.35f));
    setColour (juce::PopupMenu::headerTextColourId, Colours::accent);
    setColour (juce::TextButton::buttonColourId, Colours::panelHi);
    setColour (juce::TextButton::buttonOnColourId, Colours::accent);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::ToggleButton::textColourId, Colours::text);
    setColour (juce::TabbedButtonBar::tabTextColourId, Colours::textDim);
    setColour (juce::TabbedButtonBar::frontTextColourId, Colours::accent);
    setColour (juce::TabbedComponent::backgroundColourId, Colours::bg);
    setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::AlertWindow::backgroundColourId, Colours::panel);
    setColour (juce::AlertWindow::textColourId, Colours::text);
    setColour (juce::TextEditor::backgroundColourId, Colours::panelHi);
    setColour (juce::TextEditor::textColourId, Colours::text);
    setColour (juce::TextEditor::outlineColourId, Colours::outline);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = start + pos * (end - start);
    const float lw = juce::jmax (2.5f, r * 0.14f);

    g.setColour (Colours::panelHi);
    g.fillEllipse (c.x - r * 0.72f, c.y - r * 0.72f, r * 1.44f, r * 1.44f);

    juce::Path track; track.addCentredArc (c.x, c.y, r - lw, r - lw, 0, start, end, true);
    g.setColour (Colours::outline);
    g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // bipolar knobs fill from the centre
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bipolar ? start + (float) s.valueToProportionOfLength (0.0) * (end - start) : start;
    juce::Path val; val.addCentredArc (c.x, c.y, r - lw, r - lw, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
    g.setColour (s.isEnabled() ? Colours::accent : Colours::textDim);
    g.strokePath (val, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path tick;
    tick.addRoundedRectangle (-1.5f, -r * 0.7f, 3.0f, r * 0.38f, 1.5f);
    g.setColour (Colours::text);
    g.fillPath (tick, juce::AffineTransform::rotation (ang).translated (c));
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    if (b.getButtonText().isEmpty()) // power button style
    {
        const float d = juce::jmin (r.getWidth(), r.getHeight()) - 4.0f;
        auto c = r.withSizeKeepingCentre (d, d);
        g.setColour (on ? Colours::accent : Colours::panelHi);
        g.fillEllipse (c);
        g.setColour (on ? juce::Colours::black : Colours::textDim);
        juce::Path p; const auto cc = c.getCentre(); const float rr = d * 0.27f;
        p.addCentredArc (cc.x, cc.y, rr, rr, 0, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
        g.strokePath (p, juce::PathStrokeType (1.6f));
        g.drawLine (cc.x, cc.y - rr * 1.2f, cc.x, cc.y - rr * 0.2f, 1.6f);
        return;
    }
    auto box = r.removeFromLeft (r.getHeight()).reduced (4.0f);
    g.setColour (on ? Colours::accent : Colours::panelHi);
    g.fillRoundedRectangle (box, 3.0f);
    if (highlighted) { g.setColour (Colours::outline.brighter()); g.drawRoundedRectangle (box, 3.0f, 1.0f); }
    if (on)
    {
        g.setColour (juce::Colours::black);
        juce::Path t; t.startNewSubPath (box.getX() + box.getWidth() * 0.22f, box.getCentreY());
        t.lineTo (box.getX() + box.getWidth() * 0.43f, box.getBottom() - box.getHeight() * 0.25f);
        t.lineTo (box.getRight() - box.getWidth() * 0.2f, box.getY() + box.getHeight() * 0.25f);
        g.strokePath (t, juce::PathStrokeType (2.0f));
    }
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (12.5f));
    g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedLeft (4), juce::Justification::centredLeft, 2);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path a; const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    a.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (Colours::accent);
    g.fillPath (a);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? Colours::accent : base;
    if (down) c = c.darker (0.2f); else if (highlighted) c = c.brighter (0.1f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

// =====================================================================================  ParamControl
ParamControl::ParamControl (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label)
{
    auto* p = s.getParameter (id);
    jassert (p != nullptr);
    caption.setText (label, juce::dontSendNotification);
    caption.setJustificationType (juce::Justification::centred);
    caption.setColour (juce::Label::textColourId, Colours::textDim);
    caption.setFont (juce::FontOptions (11.5f));
    addAndMakeVisible (caption);

    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
    {
        combo = std::make_unique<juce::ComboBox>();
        combo->addItemList (c->choices, 1);
        addAndMakeVisible (*combo);
        ca = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, *combo);
    }
    else if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr)
    {
        toggle = std::make_unique<juce::ToggleButton> ("");
        toggle->setButtonText (label);
        caption.setVisible (false);
        addAndMakeVisible (*toggle);
        ba = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, *toggle);
    }
    else
    {
        slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        slider->setPopupDisplayEnabled (false, false, nullptr);
        addAndMakeVisible (*slider);
        sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, *slider);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            slider->setDoubleClickReturnValue (true, rp->convertFrom0to1 (rp->getDefaultValue()));
            const auto unit = rp->getLabel();
            slider->textFromValueFunction = [unit] (double v)
            {
                const double a = std::abs (v);
                juce::String t = a >= 1000 ? juce::String (v / 1000.0, 1) + "k" : (a >= 100 ? juce::String (juce::roundToInt (v)) : juce::String (v, a >= 10 ? 1 : 2));
                return unit.isEmpty() ? t : t + " " + unit;
            };
            slider->updateText();
        }
    }
}

void ParamControl::resized()
{
    auto r = getLocalBounds();
    if (slider)
    {
        caption.setBounds (r.removeFromTop (14));
        slider->setBounds (r);
    }
    else if (combo)
    {
        caption.setBounds (r.removeFromTop (14));
        combo->setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 4, 130), 24));
    }
    else if (toggle)
        toggle->setBounds (r.withSizeKeepingCentre (r.getWidth() - 4, 24));
}

// =====================================================================================  ModulePanel
ModulePanel::ModulePanel (juce::AudioProcessorValueTreeState& s, const juce::String& t, const juce::String& powerId, int columns)
    : state (s), title (t), cols (columns)
{
    if (powerId.isNotEmpty())
    {
        power = std::make_unique<juce::ToggleButton> ("");
        addAndMakeVisible (*power);
        powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, powerId, *power);
        power->onStateChange = [this] { repaint(); };
    }
}

ParamControl& ModulePanel::add (const juce::String& id, const juce::String& label)
{
    controls.push_back (std::make_unique<ParamControl> (state, id, label));
    addAndMakeVisible (*controls.back());
    return *controls.back();
}

juce::Rectangle<int> ModulePanel::contentArea() const { return getLocalBounds().reduced (8).withTrimmedTop (26); }

void ModulePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r, 8.0f);
    const bool on = power == nullptr || power->getToggleState();
    g.setColour (on ? Colours::accent.withAlpha (0.55f) : Colours::outline);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);
    g.setColour (on ? Colours::text : Colours::textDim);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (title.toUpperCase(), getLocalBounds().reduced (10, 0).withHeight (30).withTrimmedLeft (power ? 26 : 0),
                juce::Justification::centredLeft);
    if (statusText)
    {
        g.setColour (Colours::accent2);
        g.setFont (juce::FontOptions (11.5f));
        g.drawText (statusText(), getLocalBounds().reduced (10, 0).withHeight (30), juce::Justification::centredRight);
    }
}

void ModulePanel::resized()
{
    if (power) power->setBounds (8, 5, 22, 22);
    auto area = contentArea();
    const int customH = customs.empty() ? 0 : 34;
    auto ctrlArea = area.withTrimmedBottom (customH);
    for (auto* c : customs) c->setBounds (area.removeFromBottom (customH).reduced (2, 3));
    const int n = (int) controls.size();
    if (n == 0) return;
    const int rows = (n + cols - 1) / cols;
    const int cw = ctrlArea.getWidth() / cols;
    const int ch = juce::jmin (100, ctrlArea.getHeight() / rows);
    for (int i = 0; i < n; ++i)
        controls[(size_t) i]->setBounds (ctrlArea.getX() + (i % cols) * cw, ctrlArea.getY() + (i / cols) * ch, cw, ch);
}

// =====================================================================================  NoteGrid
NoteGrid::NoteGrid (juce::AudioProcessorValueTreeState& s)
{
    for (int i = 0; i < 12; ++i)
    {
        auto* b = buttons.add (new juce::TextButton (vf::noteName (i)));
        b->setClickingTogglesState (true);
        b->setTooltip ("Custom scale note (Scale = Custom)");
        addAndMakeVisible (b);
        atts.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (s, "tune_note_" + juce::String (i), *b));
    }
}

void NoteGrid::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / 12;
    for (int i = 0; i < 12; ++i) buttons[i]->setBounds (r.getX() + i * w, r.getY(), w - 3, r.getHeight());
}

// =====================================================================================  PitchDisplay
void PitchDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    constexpr int H = vf::EngineMeters::kHist;
    const int w = meters.histWrite.load (std::memory_order_acquire);
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < H; ++i)
    {
        const float v = meters.histOut[i];
        if (v > 0) { lo = juce::jmin (lo, v, meters.histIn[i]); hi = juce::jmax (hi, v, meters.histIn[i]); }
    }
    if (lo > hi) { lo = 55; hi = 67; }
    lo = std::floor (lo) - 2; hi = std::ceil (hi) + 2;
    if (hi - lo < 10) { const float c = 0.5f * (hi + lo); lo = c - 5; hi = c + 5; }

    auto plot = r.reduced (10, 8).withTrimmedRight (150);
    auto yOf = [&] (float note) { return plot.getBottom() - (note - lo) / (hi - lo) * plot.getHeight(); };
    g.setFont (juce::FontOptions (10.0f));
    for (int n = (int) lo; n <= (int) hi; ++n)
    {
        const bool c = (n % 12) == 0;
        g.setColour (c ? Colours::outline.brighter (0.2f) : Colours::outline.withAlpha (0.5f));
        g.drawHorizontalLine ((int) yOf ((float) n), plot.getX(), plot.getRight());
        if (c || (hi - lo) < 16)
        {
            g.setColour (Colours::textDim);
            g.drawText (juce::String (vf::noteName (n)) + juce::String (n / 12 - 1), (int) plot.getX(), (int) yOf ((float) n) - 12, 30, 12, juce::Justification::left);
        }
    }
    auto drawCurve = [&] (const float* data, juce::Colour col, float thick)
    {
        juce::Path p; bool started = false;
        for (int i = 0; i < H; ++i)
        {
            const float v = data[(w + i) % H];
            const float x = plot.getX() + plot.getWidth() * (float) i / (H - 1);
            if (v <= 0) { started = false; continue; }
            const float y = yOf (v);
            if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
        }
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (thick));
    };
    drawCurve (meters.histIn, Colours::textDim.withAlpha (0.8f), 1.2f);
    drawCurve (meters.histOut, Colours::accent, 2.0f);

    // readout
    auto txt = r.removeFromRight (150).reduced (10);
    const bool voiced = meters.voiced.load();
    const float in = meters.inNote.load(), tgt = meters.targetNote.load();
    g.setColour (voiced ? Colours::text : Colours::textDim);
    g.setFont (juce::FontOptions (34.0f, juce::Font::bold));
    const int tn = juce::roundToInt (tgt);
    g.drawText (voiced ? juce::String (vf::noteName (tn)) + juce::String (tn / 12 - 1) : "--", txt.removeFromTop (48), juce::Justification::centred);
    g.setFont (juce::FontOptions (12.0f));
    const float cents = (in - tgt) * 100.0f;
    g.setColour (Colours::textDim);
    g.drawText (voiced ? juce::String ("input ") + (cents >= 0 ? "+" : "") + juce::String (juce::roundToInt (cents)) + " ct" : "no pitch",
                txt.removeFromTop (18), juce::Justification::centred);
    // cents bar
    auto bar = txt.removeFromTop (14).reduced (6, 3).toFloat();
    g.setColour (Colours::panelHi); g.fillRoundedRectangle (bar, 3);
    if (voiced)
    {
        const float x = bar.getCentreX() + juce::jlimit (-50.0f, 50.0f, cents) / 50.0f * bar.getWidth() * 0.5f;
        g.setColour (std::abs (cents) < 10 ? Colours::accent2 : Colours::accent);
        g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (x, bar.getCentreX()), bar.getY(), std::abs (x - bar.getCentreX()) + 2, bar.getHeight()), 3);
    }
    g.setColour (Colours::accent);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("- in   - out", txt.removeFromBottom (16), juce::Justification::centred);
}

// =====================================================================================  Editor
VoxForgeEditor::VoxForgeEditor (VoxForgeProcessor& p)
    : AudioProcessorEditor (&p), proc (p), state (p.getState()), pitchDisplay (p.meters())
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);

    // ---------------- header
    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this] { loadPresetFromMenu (presetBox.getSelectedId()); };
    buildPresetMenu();
    for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &undoBtn, &redoBtn }) addAndMakeVisible (b);
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    undoBtn.onClick = [this] { proc.getUndo().undo(); };
    redoBtn.onClick = [this] { proc.getUndo().redo(); };
    saveBtn.onClick = [this]
    {
        auto* w = new juce::AlertWindow ("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", proc.presetName.isEmpty() ? "My Preset" : proc.presetName);
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<VoxForgeEditor> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
        {
            if (result != 1 || safe == nullptr) return;
            const auto name = w->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;
            if (vfp::saveUserPreset (safe->state, name)) { safe->proc.presetName = name; safe->buildPresetMenu(); }
        }), true);
    };
    modeCtl = std::make_unique<ParamControl> (state, "mode", "Mode");
    inputCtl = std::make_unique<ParamControl> (state, "input", "Input");
    outputCtl = std::make_unique<ParamControl> (state, "output", "Output");
    mixCtl = std::make_unique<ParamControl> (state, "mix", "Mix");
    for (auto* c : { modeCtl.get(), inputCtl.get(), outputCtl.get(), mixCtl.get() }) addAndMakeVisible (c);

    // ---------------- recognition
    addAndMakeVisible (pitchDisplay);
    for (auto* l : { &keyLabel, &sourceLabel, &analysisInfo }) { addAndMakeVisible (l); l->setJustificationType (juce::Justification::centredLeft); }
    keyLabel.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    sourceLabel.setFont (juce::FontOptions (16.0f));
    analysisInfo.setFont (juce::FontOptions (12.0f));
    analysisInfo.setColour (juce::Label::textColourId, Colours::textDim);
    analysisInfo.setText ("Play/sing ~8 s, then VoxForge sets gate, EQ, de-esser, compressor and key.", juce::dontSendNotification);
    addAndMakeVisible (analyseBtn); addAndMakeVisible (keyResetBtn); addAndMakeVisible (applySourceBtn);
    applySourceBtn.setToggleState (true, juce::dontSendNotification);
    analyseBtn.onClick = [this] { proc.startAutoAnalysis(); analyseBtn.setEnabled (false); };
    keyResetBtn.onClick = [this] { proc.resetKeyDetection(); };

    // ---------------- correction page
    auto mk = [this] (juce::Component& page, const juce::String& title, const juce::String& power, int cols)
    {
        panels.push_back (std::make_unique<ModulePanel> (state, title, power, cols));
        page.addAndMakeVisible (*panels.back());
        return panels.back().get();
    };
    tunePanel = mk (correctionPage, "Tune", "tune_on", 4);
    tunePanel->add ("tune_key", "Key"); tunePanel->add ("tune_scale", "Scale");
    tunePanel->add ("tune_speed", "Retune"); tunePanel->add ("tune_tolerance", "Tolerance");
    tunePanel->add ("tune_amount", "Amount"); tunePanel->add ("tune_humanize", "Humanize");
    tunePanel->add ("tune_transpose", "Transpose"); tunePanel->add ("tune_midi", "MIDI target");
    noteGrid = std::make_unique<NoteGrid> (state);
    tunePanel->addCustom (noteGrid.get());

    gatePanel = mk (correctionPage, "Gate", "gate_on", 2);
    gatePanel->add ("gate_thresh", "Threshold"); gatePanel->add ("gate_range", "Range");
    gatePanel->add ("gate_attack", "Attack"); gatePanel->add ("gate_release", "Release");
    gatePanel->statusText = [this] { return juce::String (vf::gainToDb (proc.meters().gateGain.load()), 1) + " dB"; };

    sculptPanel = mk (correctionPage, "Sculpt", "sculpt_on", 2);
    sculptPanel->add ("sculpt_amount", "Amount"); sculptPanel->add ("sculpt_tilt", "Tilt");
    sculptPanel->statusText = [this] { return state.getRawParameterValue ("mode")->load() < 0.5f ? "adaptive + dynamic" : "adaptive"; };

    deessPanel = mk (correctionPage, "De-Esser", "deess_on", 1);
    deessPanel->add ("deess_amount", "Amount"); deessPanel->add ("deess_freq", "Freq");
    deessPanel->statusText = [this] { return "-" + juce::String (proc.meters().deEssGr.load(), 1) + " dB"; };

    eqPanel = mk (correctionPage, "EQ", "eq_on", 7);
    eqPanel->add ("eq_hp_on", "HP"); eqPanel->add ("eq_hp_freq", "HP Freq");
    eqPanel->add ("eq_ls_freq", "LS Freq"); eqPanel->add ("eq_ls_gain", "LS Gain");
    eqPanel->add ("eq_hs_freq", "HS Freq"); eqPanel->add ("eq_hs_gain", "HS Gain"); eqPanel->add ("eq_solo", "Solo");
    for (int b = 1; b <= 4; ++b) eqPanel->add ("eq_b" + juce::String (b) + "_freq", "B" + juce::String (b) + " Freq");
    eqPanel->add ("eq_lp_on", "LP"); eqPanel->add ("eq_lp_freq", "LP Freq");
    for (int b = 1; b <= 4; ++b) eqPanel->add ("eq_b" + juce::String (b) + "_gain", "B" + juce::String (b) + " Gain");
    for (int b = 1; b <= 3; ++b) eqPanel->add ("eq_b" + juce::String (b) + "_q", "B" + juce::String (b) + " Q");
    eqPanel->add ("eq_b4_q", "B4 Q");
    // pad to keep grid tidy
    compPanel = mk (correctionPage, "Compressor", "comp_on", 4);
    compPanel->add ("comp_mode", "Mode"); compPanel->add ("comp_input", "Input");
    compPanel->add ("comp_attack", "Attack"); compPanel->add ("comp_release", "Release");
    compPanel->add ("comp_ratio", "Ratio"); compPanel->add ("comp_depth", "Multi Depth");
    compPanel->add ("comp_mix", "Mix"); compPanel->add ("comp_output", "Output");
    compPanel->statusText = [this] { return "GR " + juce::String (proc.meters().compGr.load(), 1) + " dB"; };

    satPanel = mk (correctionPage, "Saturation", "sat_on", 3);
    satPanel->add ("sat_mode", "Mode"); satPanel->add ("sat_drive", "Drive"); satPanel->add ("sat_tone", "Tone");
    satPanel->add ("sat_bits", "Bits"); satPanel->add ("sat_rate", "Downsample"); satPanel->add ("sat_mix", "Mix");
    satPanel->add ("sat_output", "Output");

    // ---------------- harmony page
    harmPanel = mk (harmonyPage, "Harmonies", "harm_on", 7);
    harmPanel->add ("harm_mode", "Mode"); harmPanel->add ("harm_voices", "Voices");
    harmPanel->add ("harm_link", "Link pairs"); harmPanel->add ("harm_dimute", "DI mute");
    harmPanel->add ("harm_level", "Level"); harmPanel->add ("harm_humanize", "Humanize"); harmPanel->add ("harm_width", "Width");
    harmPanel->statusText = [this]
    {
        const int m = (int) state.getRawParameterValue ("harm_mode")->load();
        return m == 0 ? juce::String ("interval = scale steps") : (m == 1 ? juce::String ("interval = semitones") : juce::String ("voices follow held MIDI notes"));
    };
    for (int v = 0; v < 8; ++v)
    {
        auto* vp = mk (harmonyPage, "Voice " + juce::String (v + 1), {}, 1);
        vp->add (vfp::voiceId (v, "interval"), "Interval");
        vp->add (vfp::voiceId (v, "level"), "Level");
        vp->add (vfp::voiceId (v, "pan"), "Pan");
        voicePanels.push_back (vp);
    }

    // ---------------- effects page
    dlyPanel = mk (effectsPage, "Delay", "dly_on", 5);
    dlyPanel->add ("dly_mode", "Mode"); dlyPanel->add ("dly_sync", "Sync"); dlyPanel->add ("dly_div", "Division L");
    dlyPanel->add ("dly_div_r", "Division R"); dlyPanel->add ("dly_fb", "Feedback");
    dlyPanel->add ("dly_time_l", "Time L"); dlyPanel->add ("dly_time_r", "Time R (Dual)");
    dlyPanel->add ("dly_tone", "Tone"); dlyPanel->add ("dly_duck", "Duck"); dlyPanel->add ("dly_mix", "Mix");

    revPanel = mk (effectsPage, "Reverb", "rev_on", 4);
    revPanel->add ("rev_mode", "Mode"); revPanel->add ("rev_size", "Size"); revPanel->add ("rev_decay", "Decay");
    revPanel->add ("rev_pre", "Pre-Delay"); revPanel->add ("rev_damp", "Damping"); revPanel->add ("rev_lowcut", "Low Cut");
    revPanel->add ("rev_duck", "Duck"); revPanel->add ("rev_mix", "Mix");

    modPanel = mk (effectsPage, "Modulation", "mod_on", 5);
    modPanel->add ("mod_mode", "Mode"); modPanel->add ("mod_rate", "Rate"); modPanel->add ("mod_depth", "Depth");
    modPanel->add ("mod_fb", "Feedback"); modPanel->add ("mod_mix", "Mix");

    dblPanel = mk (effectsPage, "Doubler", "dbl_on", 4);
    dblPanel->add ("dbl_char", "Character"); dblPanel->add ("dbl_amount", "Amount");
    dblPanel->add ("dbl_timing", "Timing"); dblPanel->add ("dbl_width", "Width");

    tabs.addTab ("CORRECTION", Colours::bg, &correctionPage, false);
    tabs.addTab ("HARMONIES", Colours::bg, &harmonyPage, false);
    tabs.addTab ("EFFECTS", Colours::bg, &effectsPage, false);
    tabs.setTabBarDepth (32);
    addAndMakeVisible (tabs);

    setSize (1200, 820);
    startTimerHz (30);
}

VoxForgeEditor::~VoxForgeEditor()
{
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void VoxForgeEditor::buildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& fp = vfp::factoryPresets();
    juce::String cat;
    for (int i = 0; i < (int) fp.size(); ++i)
    {
        if (fp[(size_t) i].category != cat) { cat = fp[(size_t) i].category; presetBox.addSectionHeading (cat); }
        presetBox.addItem (fp[(size_t) i].name, i + 1);
    }
    userFiles.clear();
    for (auto& f : vfp::userPresetFiles()) userFiles.push_back (f);
    if (! userFiles.empty())
    {
        presetBox.addSectionHeading ("User");
        for (int i = 0; i < (int) userFiles.size(); ++i) presetBox.addItem (userFiles[(size_t) i].getFileNameWithoutExtension(), 1000 + i);
    }
    presetBox.setText (proc.presetName, juce::dontSendNotification);
}

void VoxForgeEditor::loadPresetFromMenu (int id)
{
    if (id <= 0) return;
    if (id < 1000) proc.setCurrentProgram (id - 1);
    else if (id - 1000 < (int) userFiles.size())
    {
        proc.getUndo().beginNewTransaction ("Preset");
        vfp::loadUserPreset (state, userFiles[(size_t) (id - 1000)]);
        proc.presetName = userFiles[(size_t) (id - 1000)].getFileNameWithoutExtension();
        proc.getUndo().beginNewTransaction();
    }
}

void VoxForgeEditor::stepPreset (int delta)
{
    const int count = presetBox.getNumItems();
    if (count == 0) return;
    int idx = presetBox.getSelectedItemIndex();
    idx = (idx + delta + count) % count;
    presetBox.setSelectedItemIndex (idx, juce::sendNotification);
}

void VoxForgeEditor::timerCallback()
{
    auto& m = proc.meters();
    const int kr = m.keyRoot.load();
    const int keyParam = (int) state.getRawParameterValue ("tune_key")->load();
    juce::String keyTxt = kr >= 0 ? juce::String (vf::noteName (kr)) + (m.keyMinor.load() ? " minor" : " major") : juce::String ("listening...");
    keyLabel.setText ("Key: " + keyTxt + (keyParam == 0 ? "  (auto)" : "  (manual: " + vfp::keyChoices()[keyParam] + ")"), juce::dontSendNotification);
    const auto src = (vf::SourceType) m.source.load();
    sourceLabel.setText (juce::String ("Source: ") + vf::sourceName (src)
                             + (src != vf::SourceType::Unknown ? "  " + juce::String (juce::roundToInt (m.sourceConfidence.load() * 100)) + "%" : juce::String()),
                         juce::dontSendNotification);

    const auto st = proc.analysisState();
    if (st == vf::VoxEngine::AnalysisState::Running)
    {
        analyseBtn.setButtonText ("Analysing " + juce::String (juce::roundToInt (m.analysisProgress.load() * 100)) + "%");
        analyseBtn.setEnabled (false);
    }
    else if (st == vf::VoxEngine::AnalysisState::Done)
    {
        const auto s = proc.takeAnalysisResult();
        proc.applyAutoSettings (s, applySourceBtn.getToggleState());
        analyseBtn.setButtonText ("Auto-Analyse"); analyseBtn.setEnabled (true);
        if (s.valid)
            analysisInfo.setText ("Applied: " + juce::String (vf::sourceName (s.source)) + ", gate " + juce::String (s.gateThresholdDb, 0)
                                      + " dB, HP " + juce::String (juce::roundToInt (s.hpFreq)) + " Hz, de-ess " + juce::String (juce::roundToInt (s.deEssAmount))
                                      + "%, " + (s.female ? "high" : "low") + " voice" + (s.keyRoot >= 0 ? ", key " + juce::String (vf::noteName (s.keyRoot)) + (s.keyMinor ? "m" : "") : juce::String()),
                                  juce::dontSendNotification);
    }
    else if (! analyseBtn.isEnabled()) { analyseBtn.setEnabled (true); analyseBtn.setButtonText ("Auto-Analyse"); }

    undoBtn.setEnabled (proc.getUndo().canUndo());
    redoBtn.setEnabled (proc.getUndo().canRedo());
    if (++transactionCounter >= 15 && ! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
    {
        transactionCounter = 0;
        proc.getUndo().beginNewTransaction();
    }
    if (presetBox.getText() != proc.presetName && ! presetBox.isPopupActive()) presetBox.setText (proc.presetName, juce::dontSendNotification);

    pitchDisplay.repaint();
    for (auto* p : { gatePanel, deessPanel, compPanel, harmPanel, sculptPanel }) p->repaint (0, 0, p->getWidth(), 30);
}

void VoxForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);
    auto head = getLocalBounds().removeFromTop (60).reduced (14, 0);
    g.setColour (Colours::accent);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("VOX", head.removeFromLeft (58), juce::Justification::centredLeft);
    g.setColour (Colours::text);
    g.drawText ("FORGE", head.removeFromLeft (90), juce::Justification::centredLeft);
    g.setColour (Colours::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("by JustCreateit.ch", head.removeFromLeft (120), juce::Justification::centredLeft);
    g.setColour (Colours::outline);
    g.drawHorizontalLine (60, 0, (float) getWidth());
}

void VoxForgeEditor::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (60).reduced (10, 6);
    head.removeFromLeft (280);
    // right side: mode, in, out, mix
    mixCtl->setBounds (head.removeFromRight (64));
    outputCtl->setBounds (head.removeFromRight (64));
    inputCtl->setBounds (head.removeFromRight (64));
    modeCtl->setBounds (head.removeFromRight (120));
    head.removeFromRight (16);
    auto pb = head.withSizeKeepingCentre (head.getWidth(), 28);
    prevBtn.setBounds (pb.removeFromLeft (28)); pb.removeFromLeft (4);
    auto rightBtns = pb.removeFromRight (200);
    nextBtn.setBounds (pb.removeFromRight (28)); pb.removeFromRight (4);
    presetBox.setBounds (pb);
    rightBtns.removeFromLeft (8);
    saveBtn.setBounds (rightBtns.removeFromLeft (60)); rightBtns.removeFromLeft (4);
    undoBtn.setBounds (rightBtns.removeFromLeft (60)); rightBtns.removeFromLeft (4);
    redoBtn.setBounds (rightBtns.removeFromLeft (60));

    // recognition strip
    auto rec = r.removeFromTop (150).reduced (10, 8);
    pitchDisplay.setBounds (rec.removeFromLeft (640));
    rec.removeFromLeft (14);
    keyLabel.setBounds (rec.removeFromTop (30));
    sourceLabel.setBounds (rec.removeFromTop (24));
    auto btnRow = rec.removeFromTop (34).reduced (0, 3);
    analyseBtn.setBounds (btnRow.removeFromLeft (170)); btnRow.removeFromLeft (8);
    keyResetBtn.setBounds (btnRow.removeFromLeft (120));
    applySourceBtn.setBounds (rec.removeFromTop (24));
    analysisInfo.setBounds (rec);

    tabs.setBounds (r.reduced (6, 0).withTrimmedBottom (6));

    // pages use the tab content size
    const auto page = tabs.getLocalBounds().withTrimmedTop (tabs.getTabBarDepth());
    const int W = page.getWidth() - 8, H = page.getHeight() - 8;
    const int gap = 8;
    {
        auto a = juce::Rectangle<int> (4, 4, W, H);
        auto row1 = a.removeFromTop (H * 46 / 100); a.removeFromTop (gap);
        auto row2 = a;
        tunePanel->setBounds (row1.removeFromLeft (W * 48 / 100)); row1.removeFromLeft (gap);
        gatePanel->setBounds (row1.removeFromLeft (W * 24 / 100)); row1.removeFromLeft (gap);
        sculptPanel->setBounds (row1);
        deessPanel->setBounds (row2.removeFromLeft (W * 11 / 100)); row2.removeFromLeft (gap);
        eqPanel->setBounds (row2.removeFromLeft (W * 45 / 100)); row2.removeFromLeft (gap);
        compPanel->setBounds (row2.removeFromLeft (W * 25 / 100)); row2.removeFromLeft (gap);
        satPanel->setBounds (row2);
    }
    {
        auto a = juce::Rectangle<int> (4, 4, W, H);
        harmPanel->setBounds (a.removeFromTop (130)); a.removeFromTop (gap);
        const int vw = (W - 7 * gap) / 8;
        for (int v = 0; v < 8; ++v) { voicePanels[(size_t) v]->setBounds (a.removeFromLeft (vw)); a.removeFromLeft (gap); }
    }
    {
        auto a = juce::Rectangle<int> (4, 4, W, H);
        auto row1 = a.removeFromTop ((H - gap) / 2); a.removeFromTop (gap);
        dlyPanel->setBounds (row1.removeFromLeft ((W - gap) / 2)); row1.removeFromLeft (gap);
        revPanel->setBounds (row1);
        modPanel->setBounds (a.removeFromLeft ((W - gap) / 2)); a.removeFromLeft (gap);
        dblPanel->setBounds (a);
    }
}
