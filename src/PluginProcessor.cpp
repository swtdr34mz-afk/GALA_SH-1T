/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (GALA SH-1T) */
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace gl;

/* ------------------------------------------------------------------------------- parameters --- */
juce::String GalaProcessor::paramId(int idx)
{
    if (idx < 3 * NTONE)
        return "t" + juce::String(idx / NTONE + 1) + "_" + TONE[idx % NTONE].id;
    return COMMON[idx - 3 * NTONE].id;
}

juce::AudioProcessorValueTreeState::ParameterLayout GalaProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int t = 0; t < 3; t++) {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup>("tone" + juce::String(t + 1),
                                                                          "Tone " + juce::String(t + 1), " | ");
        for (int p = 0; p < NTONE; p++) {
            int idx = toneIdx(t, p);
            const ParamDef &d = TONE[p];
            group->addChild(std::make_unique<juce::AudioParameterInt>(
                juce::ParameterID{paramId(idx), 1}, "T" + juce::String(t + 1) + " " + d.name, d.lo, d.hi, d.def,
                juce::AudioParameterIntAttributes().withStringFromValueFunction([idx](int v, int) {
                    char s[48];
                    paramText(idx, v, s, sizeof s);
                    return juce::String(s);
                })));
        }
        layout.add(std::move(group));
    }
    auto common = std::make_unique<juce::AudioProcessorParameterGroup>("common", "Common", " | ");
    for (int p = 0; p < NCOMMON; p++) {
        int idx = comIdx(p);
        const ParamDef &d = COMMON[p];
        common->addChild(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{paramId(idx), 1}, d.name, d.lo, d.hi, d.def,
            juce::AudioParameterIntAttributes().withStringFromValueFunction([idx](int v, int) {
                char s[48];
                paramText(idx, v, s, sizeof s);
                return juce::String(s);
            })));
    }
    layout.add(std::move(common));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"voices", 1}, "Voices",
                                                            juce::StringArray{"8", "16", "32", "64"}, 2));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"host_sync", 1}, "Host Sync", true));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"velocity", 1}, "Keyboard Velocity",
                                                            juce::StringArray{"Real", "Fix"}, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"transpose", 1}, "Transpose", -5, 6, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"key_hold", 1}, "Key Hold", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"dbeam", 1}, "D Beam",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"metronome", 1}, "Metronome",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"volume", 1}, "Master Volume", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) {
            return v <= 0.0001f ? juce::String("-inf") : juce::String(20.0f * std::log10(v * v * 2.0f), 1) + " dB";
        })));
    return layout;
}

GalaProcessor::GalaProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "GALA", createLayout()), engine(std::make_unique<Engine>())
{
    pparam.resize(NPATCH);
    sent.assign(NPATCH, 0);
    for (int i = 0; i < NPATCH; i++)
        pparam[(size_t)i] = apvts.getParameter(paramId(i));
    loadUserPatches();
    live = factory(0);
    std::memcpy(userGate, live.gate, sizeof userGate);
    applyPatch(factory(0), true);
}

GalaProcessor::~GalaProcessor() { cancelPendingUpdate(); }

int GalaProcessor::value(int idx) const
{
    auto *q = pparam[(size_t)idx];
    return (int)std::lround(q->convertFrom0to1(q->getValue()));
}
void GalaProcessor::setValue(int idx, int v)
{
    auto *q = pparam[(size_t)idx];
    const ParamDef &d = patchDef(idx);
    v = juce::jlimit(d.lo, d.hi, v);
    q->beginChangeGesture();
    q->setValueNotifyingHost(q->convertTo0to1((float)v));
    q->endChangeGesture();
}

void GalaProcessor::command(Cmd c, int v)
{
    int s1, n1, s2, n2;
    fifo.prepareToWrite(1, s1, n1, s2, n2);
    if (n1 > 0)
        cmds[(size_t)s1] = {(int)c, v};
    else if (n2 > 0)
        cmds[(size_t)s2] = {(int)c, v};
    fifo.finishedWrite(n1 + n2);
}

/* ------------------------------------------------------------------------------- the gate --- */
std::array<uint8_t, GATE_STEPS> GalaProcessor::gateSteps() const
{
    std::array<uint8_t, GATE_STEPS> s{};
    int pat = value(comIdx(C_GATE_PAT));
    if (pat < NGATEPAT)
        gatePatternSteps(pat, s.data());
    else
        std::memcpy(s.data(), userGate, GATE_STEPS);
    return s;
}
int GalaProcessor::gateLength() const
{
    int pat = value(comIdx(C_GATE_PAT));
    if (pat < NGATEPAT) {
        uint8_t s[GATE_STEPS];
        return gatePatternSteps(pat, s);
    }
    return juce::jlimit(1, GATE_STEPS, value(comIdx(C_GATE_LEN)));
}

void GalaProcessor::editGateStep(int step, int v)
{
    if (step < 0 || step >= GATE_STEPS)
        return;
    int pat = value(comIdx(C_GATE_PAT));
    if (pat < NGATEPAT) {                         /* drawing starts from the pattern shown */
        int len = gatePatternSteps(pat, userGate);
        setValue(comIdx(C_GATE_LEN), len);
        setValue(comIdx(C_GATE_PAT), NGATEPAT);
    }
    userGate[step] = (uint8_t)v;
    std::memcpy(gateToSend, userGate, GATE_STEPS);
    gatePending.store(true);
}

void GalaProcessor::loadGatePattern(int k)
{
    if (k < 0 || k >= NGATEPAT)
        return;
    const GatePattern &g = GATEPAT[k];
    gatePatternSteps(k, userGate);              /* (the "User" steps start from it, if drawn on) */
    std::memcpy(gateToSend, userGate, GATE_STEPS);
    gatePending.store(true);
    setValue(comIdx(C_GATE_PAT), k);
    setValue(comIdx(C_GATE_RATE), g.rate);
    setValue(comIdx(C_GATE_SWING), g.swing);
    setValue(comIdx(C_GATE_ATT), g.att);
    setValue(comIdx(C_GATE_REL), g.rel);
    setValue(comIdx(C_GATE_ON), 1);
}

/* ------------------------------------------------------------------------------- patches --- */
void GalaProcessor::applyPatch(const Patch &p, bool toHost)
{
    {
        juce::SpinLock::ScopedLockType lock(patchLock);
        pending = p;
        live = p;
    }
    std::memcpy(userGate, p.gate, sizeof userGate);
    pendingSet.store(true);
    if (toHost)
        for (int i = 0; i < NPATCH; i++) {
            auto *q = pparam[(size_t)i];
            q->setValueNotifyingHost(q->convertTo0to1((float)patchGet(p, i)));
        }
}

void GalaProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    program = index;
    applyPatch(index < NFACTORY ? factory(index) : userPatches[(size_t)(index - NFACTORY)], true);
    updateHostDisplay(ChangeDetails().withProgramChanged(true));
}

const juce::String GalaProcessor::getProgramName(int index)
{
    if (index >= 0 && index < NFACTORY)
        return juce::String(factory(index).name);
    if (index >= NFACTORY && index < getNumPrograms())
        return juce::String(userPatches[(size_t)(index - NFACTORY)].name);
    return {};
}

juce::String GalaProcessor::patchName() const
{
    juce::SpinLock::ScopedLockType lock(patchLock);
    return juce::String(live.name);
}

Patch GalaProcessor::currentPatch() const
{
    Patch p;
    {
        juce::SpinLock::ScopedLockType lock(patchLock);
        p = live;
    }
    for (int i = 0; i < NPATCH; i++)
        patchSet(p, i, value(i));
    std::memcpy(p.gate, userGate, GATE_STEPS);
    return p;
}

void GalaProcessor::initPatch()
{
    Patch p;
    patchInit(p);
    applyPatch(p, true);
}

void GalaProcessor::copyTone(int from, int to)
{
    if (from == to || from < 0 || to < 0 || from > 2 || to > 2)
        return;
    for (int p = 0; p < NTONE; p++)
        setValue(toneIdx(to, p), value(toneIdx(from, p)));
}

juce::File GalaProcessor::patchFolder()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("GALA SH-1T").getChildFile("Patches");
}

void GalaProcessor::patchToXml(const Patch &p, juce::XmlElement &x)
{
    x.setAttribute("name", juce::String(p.name));
    x.setAttribute("version", 1);
    for (int i = 0; i < NPATCH; i++)
        x.setAttribute(paramId(i), patchGet(p, i));
    juce::MemoryBlock mb(p.gate, sizeof p.gate);
    x.setAttribute("gate", mb.toBase64Encoding());
}

bool GalaProcessor::patchFromXml(const juce::XmlElement &x, Patch &p)
{
    patchInit(p);
    std::snprintf(p.name, NAME_LEN, "%s", x.getStringAttribute("name", "Patch").substring(0, NAME_LEN - 1).toRawUTF8());
    for (int i = 0; i < NPATCH; i++)
        patchSet(p, i, x.getIntAttribute(paramId(i), patchDef(i).def));
    juce::MemoryBlock mb;
    if (mb.fromBase64Encoding(x.getStringAttribute("gate")) && mb.getSize() == sizeof p.gate)
        std::memcpy(p.gate, mb.getData(), sizeof p.gate);
    return true;
}

void GalaProcessor::loadUserPatches()
{
    userPatches.clear();
    auto files = patchFolder().findChildFiles(juce::File::findFiles, false, "*.gala");
    files.sort();
    for (auto &f : files)
        if (auto xml = juce::XmlDocument::parse(f))
            if (xml->hasTagName("GALAPATCH")) {
                Patch p;
                if (patchFromXml(*xml, p))
                    userPatches.push_back(p);
            }
}

int GalaProcessor::writePatch(const juce::String &name)
{
    auto clean = name.trim().substring(0, NAME_LEN - 1);
    if (clean.isEmpty())
        return -1;
    Patch p = currentPatch();
    std::snprintf(p.name, NAME_LEN, "%s", clean.toRawUTF8());
    juce::XmlElement xml("GALAPATCH");
    patchToXml(p, xml);
    auto folder = patchFolder();
    folder.createDirectory();
    if (!xml.writeTo(folder.getChildFile(juce::File::createLegalFileName(clean) + ".gala")))
        return -1;
    {
        juce::SpinLock::ScopedLockType lock(patchLock);
        std::snprintf(live.name, NAME_LEN, "%s", clean.toRawUTF8());
    }
    loadUserPatches();
    for (size_t i = 0; i < userPatches.size(); i++)
        if (juce::String(userPatches[i].name) == clean) {
            program = NFACTORY + (int)i;
            updateHostDisplay(ChangeDetails().withProgramChanged(true));
            return program;
        }
    return -1;
}

/* ------------------------------------------------------------------------------- the block --- */
bool GalaProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void GalaProcessor::prepareToPlay(double sampleRate, int)
{
    uiMidi.reset(sampleRate);
    engine->prepare(sampleRate);
    Patch cur = currentPatch();
    {
        juce::SpinLock::ScopedLockType lock(patchLock);
        pending = cur;
    }
    pendingSet.store(true);
    sentVoices = -1;
    lastGain = -1.0f;
}

void GalaProcessor::handleMidi(const juce::MidiMessage &m)
{
    Engine &e = *engine;
    if (m.isNoteOn())
        e.noteOn(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        e.noteOff(m.getNoteNumber());
    else if (m.isPitchWheel())
        e.bend((float)(m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isSustainPedalOn() || m.isSustainPedalOff())
        e.sustain(m.isSustainPedalOn());
    else if (m.isController()) {
        int cc = m.getControllerNumber();
        float v = (float)m.getControllerValue() / 127.0f;
        switch (cc) {
        case 1: e.modWheel(v); break;
        case 11: e.expression(v); break;
        case 120: e.panic(); break;
        case 123: e.allOff(); break;
        case 121:
            e.bend(0.0f);
            e.modWheel(0.0f);
            e.expression(1.0f);
            e.sustain(false);
            break;
        default: break;
        }
    } else if (m.isAllNotesOff() || m.isAllSoundOff())
        e.allOff();
}

void GalaProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi)
{
    juce::ScopedNoDenormals noDenormals;
    Engine &e = *engine;
    const int n = buffer.getNumSamples();
    float *L = buffer.getWritePointer(0), *R = buffer.getWritePointer(buffer.getNumChannels() > 1 ? 1 : 0);

    /* a whole patch (a program change, a project), then the parameters, when they change */
    if (pendingSet.exchange(false)) {
        Patch p;
        {
            juce::SpinLock::ScopedLockType lock(patchLock);
            p = pending;
        }
        e.setPatch(p);
        for (int i = 0; i < NPATCH; i++)
            sent[(size_t)i] = (int16_t)patchGet(p, i);
    }
    if (gatePending.exchange(false))
        e.setGate(gateToSend);
    for (int i = 0; i < NPATCH; i++) {
        auto *q = pparam[(size_t)i];
        int v = (int)std::lround(q->convertFrom0to1(q->getValue()));
        if (v != sent[(size_t)i]) {
            e.setParam(i, v);
            sent[(size_t)i] = (int16_t)v;
        }
    }
    static const int VOICES[4] = {8, 16, 32, 64};
    int voices = VOICES[juce::jlimit(0, 3, (int)std::lround(apvts.getRawParameterValue("voices")->load()))];
    if (voices != sentVoices) {
        e.setVoices(voices);
        sentVoices = voices;
    }
    e.setTranspose((int)std::lround(apvts.getRawParameterValue("transpose")->load()));
    e.setVelocityFix(apvts.getRawParameterValue("velocity")->load() > 0.5f);
    e.keyHold(apvts.getRawParameterValue("key_hold")->load() > 0.5f);
    e.setMetronome(apvts.getRawParameterValue("metronome")->load());
    float db = apvts.getRawParameterValue("dbeam")->load();
    if (db != dbLast) {
        e.dbeam(db > 0.001f, db);
        dbLast = db;
    }

    /* the panel's commands */
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead(fifo.getNumReady(), s1, n1, s2, n2);
        auto run = [&](const Command &c) {
            switch (c.c) {
            case C_PH_REC: e.phraseRec(); break;
            case C_PH_PLAY: e.phrasePlay(); break;
            case C_PH_SELECT: e.phraseSelect(c.v); break;
            case C_PH_LENGTH: e.phraseLength(c.v); break;
            case C_PH_CLEAR: e.phraseClear(); break;
            case C_PH_TYPE: e.phraseType(c.v); break;
            case C_PH_ERASE: e.phraseErase(c.v != 0); break;
            case C_PANIC: e.panic(); break;
            default: break;
            }
        };
        for (int i = 0; i < n1; i++)
            run(cmds[(size_t)(s1 + i)]);
        for (int i = 0; i < n2; i++)
            run(cmds[(size_t)(s2 + i)]);
        fifo.finishedRead(n1 + n2);
    }

    /* the host's tempo and position */
    bool sync = apvts.getRawParameterValue("host_sync")->load() > 0.5f;
    double bpm = 0.0, ppq = 0.0;
    bool playing = false;
    if (auto *ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto b = pos->getBpm())
                bpm = *b;
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition())
                ppq = *q;
        }
    e.host(playing, ppq, bpm > 1.0 ? bpm : 0.0, sync && bpm > 1.0);

    uiMidi.removeNextBlockOfMessages(midi, n);
    keyboardState.processNextMidiBuffer(midi, 0, n, true);

    int done = 0;
    for (const auto meta : midi) {
        int at = juce::jlimit(0, n, meta.samplePosition);
        if (at > done) {
            e.render(L + done, R + done, at - done);
            done = at;
        }
        handleMidi(meta.getMessage());
    }
    if (n > done)
        e.render(L + done, R + done, n - done);

    /* the knobs the phrase playback turned: the engine has them; the host's parameters follow */
    {
        int idx, v, got = 0;
        while (e.pollPlayedParam(idx, v)) {
            sent[(size_t)idx] = (int16_t)v;
            int s1, n1, s2, n2;
            playedFifo.prepareToWrite(1, s1, n1, s2, n2);
            if (n1 > 0)
                played[(size_t)s1] = {idx, v};
            else if (n2 > 0)
                played[(size_t)s2] = {idx, v};
            playedFifo.finishedWrite(n1 + n2);
            got++;
        }
        if (got)
            triggerAsyncUpdate();
    }

    /* for the editor */
    gateStepNow.store(e.gateStep(), std::memory_order_relaxed);
    gateLevelNow.store(e.gateLevel(), std::memory_order_relaxed);
    arpStepNow.store(e.arpStepShown(), std::memory_order_relaxed);
    beatNow.store(e.beat(), std::memory_order_relaxed);
    bpmNow.store(e.bpmNow(), std::memory_order_relaxed);
    holdNow.store(e.holding(), std::memory_order_relaxed);
    phraseState.store(e.phraseState(), std::memory_order_relaxed);
    phraseSel.store(e.phraseSelected(), std::memory_order_relaxed);
    phrasePos.store(e.phrasePos(), std::memory_order_relaxed);
    rehearsing.store(e.phraseRehearsing(), std::memory_order_relaxed);
    voicesNow.store(e.voicesSounding(), std::memory_order_relaxed);
    for (int t = 0; t < 3; t++) {
        lfoNow[t].store(e.lfoValue(t), std::memory_order_relaxed);
        toneLevel[t].store(e.toneLevel(t), std::memory_order_relaxed);
    }
    for (int k = 0; k < NPHRASE; k++) {
        phraseBars[k].store(e.phrase(k).bars, std::memory_order_relaxed);
        phraseEvents[k].store(e.phrase(k).n, std::memory_order_relaxed);
    }

    float vol = apvts.getRawParameterValue("volume")->load();
    float gain = vol * vol * 2.0f;
    if (lastGain < 0.0f)
        lastGain = gain;
    buffer.applyGainRamp(0, 0, n, lastGain, gain);
    if (buffer.getNumChannels() > 1)
        buffer.applyGainRamp(1, 0, n, lastGain, gain);
    lastGain = gain;
    for (int c = 2; c < buffer.getNumChannels(); c++)
        buffer.clear(c, 0, n);
}

void GalaProcessor::handleAsyncUpdate()
{
    int s1, n1, s2, n2;
    playedFifo.prepareToRead(playedFifo.getNumReady(), s1, n1, s2, n2);
    auto apply = [&](const std::pair<int, int> &pv) {
        auto *q = pparam[(size_t)pv.first];
        q->setValueNotifyingHost(q->convertTo0to1((float)pv.second));
    };
    for (int i = 0; i < n1; i++)
        apply(played[(size_t)(s1 + i)]);
    for (int i = 0; i < n2; i++)
        apply(played[(size_t)(s2 + i)]);
    playedFifo.finishedRead(n1 + n2);
}

/* ------------------------------------------------------------------------------ state --- */
void GalaProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = apvts.copyState();
    state.setProperty("program", program, nullptr);
    state.setProperty("uiScale", (double)uiScale.load(), nullptr);
    state.setProperty("uiScaleSet", uiScaleSet.load(), nullptr);
    state.setProperty("selectedTones", selectedTones.load(), nullptr);
    state.setProperty("fxSelect", fxSelect.load(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    auto *pg = xml->createNewChildElement("GALAPATCH");
    patchToXml(currentPatch(), *pg);
    /* the phrases (the engine's: read while it does not render) */
    auto *ph = xml->createNewChildElement("PHRASES");
    {
        const juce::ScopedLock sl(getCallbackLock());
        for (int k = 0; k < NPHRASE; k++) {
            const Phrase &p = engine->phrase(k);
            auto *x = ph->createNewChildElement("PHRASE");
            x->setAttribute("bars", (int)p.bars);
            juce::MemoryBlock mb(p.ev, sizeof(PhraseEvent) * p.n);
            x->setAttribute("events", mb.toBase64Encoding());
        }
    }
    copyXmlToBinary(*xml, destData);
}

void GalaProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (!xml || !xml->hasTagName(apvts.state.getType()))
        return;
    Patch p;
    bool hasPatch = false;
    if (auto *pg = xml->getChildByName("GALAPATCH")) {
        hasPatch = patchFromXml(*pg, p);
        xml->removeChildElement(pg, true);
    }
    if (auto *ph = xml->getChildByName("PHRASES")) {
        const juce::ScopedLock sl(getCallbackLock());
        int k = 0;
        for (auto *x : ph->getChildWithTagNameIterator("PHRASE")) {
            if (k >= NPHRASE)
                break;
            Phrase &q = engine->phrase(k);
            q.bars = (uint8_t)juce::jlimit(1, 8, x->getIntAttribute("bars", 4));
            juce::MemoryBlock mb;
            q.n = 0;
            if (mb.fromBase64Encoding(x->getStringAttribute("events")) && mb.getSize() % sizeof(PhraseEvent) == 0) {
                size_t n = juce::jmin((size_t)PHRASE_EVENTS, mb.getSize() / sizeof(PhraseEvent));
                std::memcpy(q.ev, mb.getData(), n * sizeof(PhraseEvent));
                q.n = (uint16_t)n;
            }
            k++;
        }
        xml->removeChildElement(ph, true);
    }
    auto state = juce::ValueTree::fromXml(*xml);
    program = (int)state.getProperty("program", 0);
    uiScale = (float)(double)state.getProperty("uiScale", 0.85);
    uiScaleSet = (bool)state.getProperty("uiScaleSet", false);
    selectedTones = juce::jlimit(1, 7, (int)state.getProperty("selectedTones", 1));
    fxSelect = juce::jlimit(0, 3, (int)state.getProperty("fxSelect", 0));
    apvts.replaceState(state);
    if (hasPatch) {
        for (int i = 0; i < NPATCH; i++)
            patchSet(p, i, value(i));
        applyPatch(p, false);
    }
}

juce::AudioProcessorEditor *GalaProcessor::createEditor() { return new GalaEditor(*this); }

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new GalaProcessor(); }
