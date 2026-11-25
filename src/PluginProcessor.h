/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (GALA SH-1T) */
/* GALA SH-1T as a plugin (VST3, AU on the Mac) and a standalone app: the engine (dsp/Engine.h), every patch parameter
 * for the host (automation, saved with the project: the three tones' "t1_cutoff" ..., the patch's own), the 64
 * factory patches and the user's own as the host's programs, MIDI in, the host's tempo and position. The PHRASE
 * RECORDER's phrases are saved with the project (not with a patch, as on the GAIA); the knobs its playback turns
 * come back to the host's parameters. */
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include "Engine.h"
#include "Presets.h"

class GalaProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    GalaProcessor();
    ~GalaProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "GALA SH-1T"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return gl::NFACTORY + (int)userPatches.size(); }
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    /* the parameters: a patch parameter by its index (gl::toneIdx, gl::comIdx), the plugin's own by id */
    static juce::String paramId(int idx);
    juce::AudioProcessorValueTreeState apvts;
    juce::RangedAudioParameter *param(int idx) const { return pparam[(size_t)idx]; }
    int value(int idx) const;
    void setValue(int idx, int v);              /* (from the editor: a gesture) */
    juce::RangedAudioParameter *global(const char *id) const { return apvts.getParameter(id); }

    /* the panel's commands to the engine (any thread; done at the next block) */
    enum Cmd { C_PH_REC, C_PH_PLAY, C_PH_SELECT, C_PH_LENGTH, C_PH_CLEAR, C_PH_TYPE, C_PH_ERASE, C_HOLD, C_PANIC };
    void command(Cmd c, int v = 0);

    /* the TRANCE GATE's steps as the panel shows them (a pattern's, or the "User" ones), and drawing one */
    std::array<uint8_t, gl::GATE_STEPS> gateSteps() const;
    int gateLength() const;
    void editGateStep(int step, int v);         /* (message thread) a step drawn: the pattern becomes "User" */
    void loadGatePattern(int k);                /* a pattern from the menu: its steps, rate, swing, attack, release */

    /* what the engine shows */
    std::atomic<int> gateStepNow{-1}, arpStepNow{-1}, phraseState{0}, phraseSel{0}, voicesNow{0};
    std::atomic<float> gateLevelNow{1.0f}, phrasePos{0.0f}, bpmNow{120.0f};
    std::atomic<bool> beatNow{false}, holdNow{false}, rehearsing{false};
    std::atomic<float> lfoNow[3] = {}, toneLevel[3] = {};
    std::atomic<int> phraseBars[gl::NPHRASE] = {}, phraseEvents[gl::NPHRASE] = {};
    juce::MidiKeyboardState keyboardState;
    juce::MidiMessageCollector uiMidi;          /* the editor's bender and mod lever */
    std::atomic<float> uiScale{0.85f};
    std::atomic<bool> uiScaleSet{false};
    std::atomic<int> selectedTones{1};          /* the panel's SELECT buttons (a bit a tone) */
    std::atomic<int> fxSelect{0};               /* SELECT CONTROL: which block's knobs the panel shows */
    std::atomic<int> phraseTypeUi{0};

    /* the patch: its name, the user's own patches (WRITE): files in Documents/GALA SH-1T/Patches */
    juce::String patchName() const;
    std::vector<gl::Patch> userPatches;
    static juce::File patchFolder();
    void loadUserPatches();
    int writePatch(const juce::String &name);
    gl::Patch currentPatch() const;             /* the sound as it is now, whole */
    void initPatch();                           /* the INIT PATCH (MANUAL's clean start) */
    void copyTone(int from, int to);            /* TONE COPY */

private:
    std::unique_ptr<gl::Engine> engine;
    std::vector<juce::RangedAudioParameter *> pparam;
    std::vector<int16_t> sent;
    int sentVoices = -1;
    int program = 0;
    float lastGain = -1.0f;
    float dbLast = -1.0f;

    /* a whole patch for the engine (a program change, a project opened): taken at the next block */
    mutable juce::SpinLock patchLock;
    gl::Patch pending, live;
    std::atomic<bool> pendingSet{false};
    uint8_t userGate[gl::GATE_STEPS] = {};      /* (message thread) the "User" gate steps */
    std::atomic<bool> gatePending{false};
    uint8_t gateToSend[gl::GATE_STEPS] = {};

    struct Command {
        int c, v;
    };
    juce::AbstractFifo fifo{256};
    std::array<Command, 256> cmds;
    /* the knobs the phrase playback turned, for the host (audio thread -> message thread) */
    juce::AbstractFifo playedFifo{512};
    std::array<std::pair<int, int>, 512> played;
    void handleAsyncUpdate() override;

    void handleMidi(const juce::MidiMessage &m);
    void applyPatch(const gl::Patch &p, bool toHost);
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static void patchToXml(const gl::Patch &p, juce::XmlElement &x);
    static bool patchFromXml(const juce::XmlElement &x, gl::Patch &p);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GalaProcessor)
};
