/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (GALA SH-1T) */
/* The editor: a GAIA SH-01's panel, laid out anew and drawn at 1700 x 1010, scaled to the window (drag the corner).
 * Everything the GAIA hides under SHIFT has its own control here.
 *
 *   top        the name, CIRCUIT MOD, the display (the patch, what was touched last), BANK A-H and NUMBER 1-8,
 *              WRITE, INIT, VOICES, HOST SYNC
 *   middle     D BEAM (a pad for the mouse), TONE 1-3 (SELECT: which tones the knobs edit, several at once;
 *              ON), LFO, OSC, FILTER, AMP, EFFECTS (SELECT CONTROL: a block's four knobs)
 *   below      the MOD LFO, PERFORM (MONO, LEGATO, PORTAMENTO, KEY HOLD, bend, tempo, octave, transpose),
 *              the TRANCE GATE (its steps drawn with the mouse, a pattern menu), the ARPEGGIO (an 8 x 8 grid: A1 .. H8),
 *              the PHRASE RECORDER
 *   bottom     the bender and the mod lever, a keyboard, the patch level and the volume */
#pragma once
#include "PluginProcessor.h"
#include "ui/Look.h"

class GalaPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1700, H = 1010;
    explicit GalaPanel(GalaProcessor &);
    ~GalaPanel() override;
    void paint(juce::Graphics &) override;
    void paintOverChildren(juce::Graphics &) override;

    GalaProcessor &proc;
    int tone() const;                                /* the first tone selected (the one the knobs show) */
    void touch(int idx);                             /* a patch parameter in hand: the display shows it */
    void touchGlobal(juce::RangedAudioParameter *);
    void patchMenu();
    void writeDialog();

    int shownIdx = -1;
    juce::RangedAudioParameter *shownGlobal = nullptr;
    double shownAt = 0.0;

    /* the hand-drawn parts poll what they show, and repaint when it changed */
    struct Live {
        virtual ~Live() = default;
        virtual void poll() = 0;
    };
    struct Legend {
        juce::String text;
        juce::Rectangle<float> r;
        float size;
        juce::Colour c;
    };
    std::vector<Legend> legends;
    std::vector<std::pair<juce::Rectangle<float>, std::pair<juce::String, juce::Colour>>> sections;

private:
    void timerCallback() override;
    void build();

    glui::Look look;
    std::vector<std::unique_ptr<juce::Component>> parts;
    std::vector<Live *> live;
    juce::Image background, dust;
    float backgroundScale = 0.0f;
    std::vector<double> taps;
    juce::Component::SafePointer<juce::AlertWindow> dialog;
    template <class T, class... A> T *add(A &&...a);
};

class GalaEditor : public juce::AudioProcessorEditor
{
public:
    explicit GalaEditor(GalaProcessor &);
    void resized() override;
    bool sized = false;                      /* (resizes during the constructor keep nothing) */

private:
    GalaProcessor &proc;
    juce::SharedResourcePointer<glui::UiResources> resources;   // (before the panel: freed after it)
    GalaPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GalaEditor)
};
