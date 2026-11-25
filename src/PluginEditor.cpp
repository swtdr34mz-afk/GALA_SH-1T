/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (GALA SH-1T) */
#include "PluginEditor.h"

using namespace glui;
using namespace gl;
using Panel = GalaPanel;

namespace
{
/* ------------------------------------------------------------------------------- what a control turns --- */
/* a tone's parameter (the tones selected: shown the first's, written to all), one of the patch's own, or one of
 * the plugin's (by id); NONE: nothing (the control hides) */
struct Bind {
    enum Kind { NONE, TONE, COM, GLOB } kind = NONE;
    int p = 0;
    const char *id = nullptr;
    static Bind T(int p) { return {TONE, p, nullptr}; }
    static Bind C(int p) { return {COM, p, nullptr}; }
    static Bind G(const char *id) { return {GLOB, 0, id}; }
    static Bind none() { return {}; }
    bool operator!=(const Bind &o) const { return kind != o.kind || p != o.p || id != o.id; }
};

juce::RangedAudioParameter *shownParam(Panel &P, const Bind &b)
{
    switch (b.kind) {
    case Bind::TONE: return P.proc.param(toneIdx(P.tone(), b.p));
    case Bind::COM: return P.proc.param(comIdx(b.p));
    case Bind::GLOB: return P.proc.global(b.id);
    default: return nullptr;
    }
}
void rangeOf(Panel &P, const Bind &b, int &lo, int &hi)
{
    lo = 0;
    hi = 127;
    if (b.kind == Bind::TONE || b.kind == Bind::COM) {
        const ParamDef &d = b.kind == Bind::TONE ? TONE[b.p] : COMMON[b.p];
        lo = d.lo;
        hi = d.hi;
    } else if (auto *q = shownParam(P, b)) {
        if (auto *ip = dynamic_cast<juce::AudioParameterInt *>(q)) {
            lo = ip->getRange().getStart();
            hi = ip->getRange().getEnd();
        } else if (auto *cp = dynamic_cast<juce::AudioParameterChoice *>(q))
            hi = cp->choices.size() - 1;
        else if (dynamic_cast<juce::AudioParameterBool *>(q))
            hi = 1;
    }
}
bool isFloat(Panel &P, const Bind &b) { return b.kind == Bind::GLOB && dynamic_cast<juce::AudioParameterFloat *>(shownParam(P, b)) != nullptr; }
int ival(Panel &P, const Bind &b)
{
    auto *q = shownParam(P, b);
    if (!q)
        return 0;
    if (isFloat(P, b))
        return (int)std::lround(q->getValue() * 127.0f);
    return (int)std::lround(q->convertFrom0to1(q->getValue()));
}
void setv(Panel &P, const Bind &b, int v)
{
    int lo, hi;
    rangeOf(P, b, lo, hi);
    v = juce::jlimit(lo, hi, v);
    switch (b.kind) {
    case Bind::TONE: {
        int sel = P.proc.selectedTones.load();
        for (int t = 0; t < 3; t++)
            if (sel & (1 << t))
                P.proc.setValue(toneIdx(t, b.p), v);
        P.touch(toneIdx(P.tone(), b.p));
        break;
    }
    case Bind::COM:
        P.proc.setValue(comIdx(b.p), v);
        P.touch(comIdx(b.p));
        break;
    case Bind::GLOB: {
        auto *q = shownParam(P, b);
        if (!q)
            return;
        q->beginChangeGesture();
        q->setValueNotifyingHost(isFloat(P, b) ? (float)v / 127.0f : q->convertTo0to1((float)v));
        q->endChangeGesture();
        P.touchGlobal(q);
        break;
    }
    default: break;
    }
}
int defOf(Panel &P, const Bind &b)
{
    if (b.kind == Bind::TONE)
        return TONE[b.p].def;
    if (b.kind == Bind::COM)
        return COMMON[b.p].def;
    if (auto *q = shownParam(P, b))
        return isFloat(P, b) ? (int)std::lround(q->getDefaultValue() * 127.0f) : (int)std::lround(q->convertFrom0to1(q->getDefaultValue()));
    return 0;
}

/* ------------------------------------------------------------------------------- a knob --- */
class Knob : public juce::Component, public Panel::Live
{
public:
    Knob(Panel &p, std::function<Bind()> b) : P(p), bind(std::move(b)) {}
    void paint(juce::Graphics &g) override
    {
        if (cur.kind == Bind::NONE)
            return;
        drawKnob(g, getLocalBounds().toFloat(), pos, bipolar);
    }
    void poll() override
    {
        Bind b = bind();
        bool vis = b.kind != Bind::NONE;
        if (vis != isVisible())
            setVisible(vis);
        cur = b;
        if (!vis)
            return;
        int lo, hi;
        rangeOf(P, b, lo, hi);
        float np = hi > lo ? (float)(ival(P, b) - lo) / (float)(hi - lo) : 0.0f;
        bool bp = lo < 0;
        if (np != pos || bp != bipolar) {
            pos = np;
            bipolar = bp;
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        startY = e.position.y;
        startV = ival(P, cur);
        touchIt();
    }
    void mouseDrag(const juce::MouseEvent &e) override
    {
        int lo, hi;
        rangeOf(P, cur, lo, hi);
        float px = juce::jlimit(1.2f, 16.0f, 220.0f / (float)juce::jmax(1, hi - lo)) * (e.mods.isShiftDown() ? 4.0f : 1.0f);
        int v = juce::jlimit(lo, hi, startV + (int)std::lround((startY - e.position.y) / px));
        if (v != ival(P, cur))
            setv(P, cur, v);
    }
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &w) override
    {
        int d = w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0;
        if (d)
            setv(P, cur, ival(P, cur) + d);
    }
    void mouseDoubleClick(const juce::MouseEvent &) override { setv(P, cur, defOf(P, cur)); }

private:
    void touchIt()
    {
        if (cur.kind == Bind::TONE)
            P.touch(toneIdx(P.tone(), cur.p));
        else if (cur.kind == Bind::COM)
            P.touch(comIdx(cur.p));
        else if (auto *q = shownParam(P, cur))
            P.touchGlobal(q);
    }
    Panel &P;
    std::function<Bind()> bind;
    Bind cur;
    float pos = -1.0f, startY = 0.0f;
    int startV = 0;
    bool bipolar = false;
};

/* ------------------------------------------------------------------------------- a fader --- */
class Fader : public juce::Component, public Panel::Live
{
public:
    Fader(Panel &p, Bind b) : P(p), bind(b) {}
    void paint(juce::Graphics &g) override { drawFader(g, getLocalBounds().toFloat(), pos, bipolar); }
    void poll() override
    {
        int lo, hi;
        rangeOf(P, bind, lo, hi);
        float np = (float)(ival(P, bind) - lo) / (float)juce::jmax(1, hi - lo);
        if (np != pos) {
            pos = np;
            bipolar = lo < 0;
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent &e) override { at(e); }
    void mouseDrag(const juce::MouseEvent &e) override { at(e); }
    void mouseDoubleClick(const juce::MouseEvent &) override { setv(P, bind, defOf(P, bind)); }
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &w) override
    {
        int d = w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0;
        if (d)
            setv(P, bind, ival(P, bind) + d);
    }

private:
    void at(const juce::MouseEvent &e)
    {
        int lo, hi;
        rangeOf(P, bind, lo, hi);
        float capH = juce::jmin(26.0f, (float)getHeight() * 0.18f), top = capH * 0.5f, bot = (float)getHeight() - capH * 0.5f;
        float f = juce::jlimit(0.0f, 1.0f, (bot - e.position.y) / (bot - top));
        int v = lo + (int)std::lround(f * (float)(hi - lo));
        if (v != ival(P, bind))
            setv(P, bind, v);
        else if (bind.kind == Bind::TONE)
            P.touch(toneIdx(P.tone(), bind.p));
        else if (bind.kind == Bind::COM)
            P.touch(comIdx(bind.p));
    }
    Panel &P;
    Bind bind;
    float pos = -1.0f;
    bool bipolar = false;
};

/* ------------------------------------------------------------------------------- a button --- */
class Btn : public juce::Component, public Panel::Live
{
public:
    Btn(std::function<juce::Colour()> l, std::function<void(const juce::MouseEvent &)> c, juce::String t = {})
        : lit(std::move(l)), click(std::move(c)), text(std::move(t))
    {
    }
    std::function<void(bool)> momentary;           /* (held: down true, up false) */
    std::function<juce::String()> dynText;
    void paint(juce::Graphics &g) override
    {
        auto r = getLocalBounds().toFloat().reduced(2.0f, 3.0f);
        drawButton(g, r, shown, down, isMouseOver());
        juce::String t = dynText ? dynText() : text;
        if (t.isNotEmpty()) {
            g.setColour(shown.isTransparent() ? juce::Colour(0xff1c1d20) : juce::Colours::black.withAlpha(0.8f));
            g.setFont(font(Face::title, juce::jmin(15.0f, r.getHeight() * 0.62f)));
            g.drawFittedText(t, r.translated(0.0f, down ? 1.0f : 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
        }
    }
    void poll() override
    {
        auto c = lit();
        juce::String t = dynText ? dynText() : text;
        if (c != shown || t != shownText) {
            shown = c;
            shownText = t;
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        down = true;
        repaint();
        if (momentary)
            momentary(true);
        else if (click)
            click(e);
    }
    void mouseUp(const juce::MouseEvent &) override
    {
        down = false;
        repaint();
        if (momentary)
            momentary(false);
    }
    void mouseEnter(const juce::MouseEvent &) override { repaint(); }
    void mouseExit(const juce::MouseEvent &) override { repaint(); }

    std::function<juce::Colour()> lit;
    std::function<void(const juce::MouseEvent &)> click;
    juce::String text, shownText;
    juce::Colour shown = juce::Colours::transparentBlack;
    bool down = false;
};

/* ------------------------------------------------------------------------------- an LED list --- */
class Leds : public juce::Component, public Panel::Live
{
public:
    Leds(Panel &p, Bind b, juce::StringArray n, juce::Colour c, bool h = false) : P(p), bind(b), names(std::move(n)), colour(c), horiz(h) {}
    void paint(juce::Graphics &g) override
    {
        int n = names.size();
        for (int i = 0; i < n; i++) {
            auto r = cell(i);
            float led = juce::jmin(10.0f, (horiz ? r.getWidth() : r.getHeight()) * 0.55f);
            juce::Point<float> c = horiz ? juce::Point<float>(r.getCentreX(), r.getY() + led * 0.6f) : juce::Point<float>(r.getX() + led * 0.6f, r.getCentreY());
            drawLed(g, c, led, colour, i == shown ? 1.0f : 0.0f);
            g.setColour(i == shown ? col::legend : col::dim);
            g.setFont(font(Face::title, 12.5f));
            if (horiz)
                g.drawFittedText(names[i], r.withTrimmedTop((int)(led * 1.3f)).toNearestInt(), juce::Justification::centredTop, 1, 0.7f);
            else
                g.drawFittedText(names[i], r.withTrimmedLeft((int)(led * 1.5f)).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        }
    }
    void poll() override
    {
        int lo, hi;
        rangeOf(P, bind, lo, hi);
        int v = ival(P, bind) - lo;
        if (v != shown) {
            shown = v;
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        int lo, hi;
        rangeOf(P, bind, lo, hi);
        for (int i = 0; i < names.size(); i++)
            if (cell(i).contains(e.position))
                setv(P, bind, lo + i);
    }

private:
    juce::Rectangle<float> cell(int i) const
    {
        int n = juce::jmax(1, names.size());
        auto r = getLocalBounds().toFloat();
        return horiz ? r.withWidth(r.getWidth() / (float)n).withX(r.getWidth() * (float)i / (float)n)
                     : r.withHeight(r.getHeight() / (float)n).withY(r.getHeight() * (float)i / (float)n);
    }
    Panel &P;
    Bind bind;
    juce::StringArray names;
    juce::Colour colour;
    bool horiz;
    int shown = -1;
};

/* ------------------------------------------------------------------------------- a text --- */
class TextBox : public juce::Component, public Panel::Live
{
public:
    TextBox(std::function<juce::String()> t, std::function<void()> c = {}, juce::Colour col = col::lcd, bool inset = true)
        : text(std::move(t)), click(std::move(c)), colour(col), box(inset)
    {
    }
    float size = 14.0f;
    juce::Justification just = juce::Justification::centred;
    void paint(juce::Graphics &g) override
    {
        auto r = getLocalBounds().toFloat();
        if (box) {
            g.setColour(col::lcdBg);
            g.fillRoundedRectangle(r, 3.0f);
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.drawRoundedRectangle(r, 3.0f, 1.2f);
        }
        g.setColour(colour);
        g.setFont(font(box ? Face::mono : Face::title, size));
        g.drawFittedText(shown, r.reduced(4.0f, 1.0f).toNearestInt(), just, 1, 0.65f);
    }
    void poll() override
    {
        auto t = text();
        if (t != shown) {
            shown = t;
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent &) override
    {
        if (click)
            click();
    }

private:
    std::function<juce::String()> text;
    std::function<void()> click;
    juce::Colour colour;
    bool box;
    juce::String shown;
};

/* ------------------------------------------------------------------------------- the display --- */
class Lcd : public juce::Component, public Panel::Live
{
public:
    explicit Lcd(Panel &p) : P(p) {}
    void paint(juce::Graphics &g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff15171a));
        g.fillRoundedRectangle(r, 6.0f);
        auto s = r.reduced(6.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0d1a22), s.getX(), s.getY(), juce::Colour(0xff060b0f), s.getX(), s.getBottom(), false));
        g.fillRoundedRectangle(s, 3.0f);
        /* (years old: a dead column, a dim corner) */
        g.setColour(juce::Colour(0x18ffffff));
        g.fillRect(s.getX() + s.getWidth() * 0.71f, s.getY() + 2.0f, 1.5f, s.getHeight() - 4.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0x70000000), s.getRight(), s.getBottom(), juce::Colour(0x00000000), s.getRight() - 120.0f, s.getBottom() - 30.0f, true));
        g.fillRoundedRectangle(s, 3.0f);
        g.setColour(col::lcd);
        g.setFont(font(Face::mono, 30.0f));
        g.drawText(l1a, s.withWidth(70.0f).withHeight(s.getHeight() * 0.55f), juce::Justification::centred, false);
        g.setFont(font(Face::mono, 25.0f));
        g.drawFittedText(l1b, s.withTrimmedLeft(76.0f).withHeight(s.getHeight() * 0.55f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        g.setColour(col::lcd.withAlpha(0.75f));
        g.setFont(font(Face::mono, 17.0f));
        g.drawFittedText(l2, s.withTrimmedTop(s.getHeight() * 0.56f).reduced(6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    }
    void poll() override
    {
        int prog = P.proc.getCurrentProgram();
        juce::String a = prog < NFACTORY ? juce::String::charToString((juce::juce_wchar)('A' + prog / 8)) + juce::String(prog % 8 + 1) : "U" + juce::String(prog - NFACTORY + 1);
        juce::String b = P.proc.patchName();
        juce::String c;
        double now = juce::Time::getMillisecondCounterHiRes();
        if (now - P.shownAt < 2500.0 && (P.shownIdx >= 0 || P.shownGlobal)) {
            if (P.shownIdx >= 0) {
                const ParamDef &d = patchDef(P.shownIdx);
                char t[48];
                int v = P.proc.value(P.shownIdx);
                bool synced = false;
                if (P.shownIdx < 3 * NTONE) {
                    int tt = P.shownIdx / NTONE, p = P.shownIdx % NTONE;
                    if (p == T_LFO_RATE)
                        synced = P.proc.value(toneIdx(tt, T_LFO_SYNC)) != 0;
                    if (p == T_MLFO_RATE)
                        synced = P.proc.value(toneIdx(tt, T_MLFO_SYNC)) != 0;
                } else if (P.shownIdx == comIdx(C_DL_TIME) || P.shownIdx == comIdx(C_PD_TIME))
                    synced = P.proc.value(comIdx(C_DELAY_SYNC)) != 0;
                paramText(P.shownIdx, v, t, sizeof t, synced);
                juce::String who = P.shownIdx < 3 * NTONE ? "T" + juce::String(P.shownIdx / NTONE + 1) + " " : juce::String();
                c = who + juce::String(d.name).toUpperCase() + "  " + juce::String(t);
            } else
                c = P.shownGlobal->getName(40).toUpperCase() + "  " + P.shownGlobal->getCurrentValueAsText();
        } else {
            c = juce::String(P.proc.bpmNow.load(), 1) + " BPM   " + juce::String(P.proc.voicesNow.load()) + " VOICES";
        }
        if (a != l1a || b != l1b || c != l2) {
            l1a = a;
            l1b = b;
            l2 = c;
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent &) override { P.patchMenu(); }

private:
    Panel &P;
    juce::String l1a, l1b, l2;
};

/* ------------------------------------------------------------------------------- the TRANCE GATE's steps --- */
class GateGrid : public juce::Component, public Panel::Live
{
public:
    explicit GateGrid(Panel &p) : P(p) {}
    void paint(juce::Graphics &g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0a1014));
        g.fillRoundedRectangle(r, 4.0f);
        float w = r.getWidth() / (float)GATE_STEPS;
        bool on = P.proc.value(comIdx(C_GATE_ON)) != 0;
        for (int i = 0; i < GATE_STEPS; i++) {
            auto c = juce::Rectangle<float>(r.getX() + w * (float)i, r.getY(), w, r.getHeight()).reduced(1.5f, 4.0f);
            bool inLen = i < len;
            if (i % 4 == 0) {
                g.setColour(juce::Colours::white.withAlpha(0.06f));
                g.fillRect(c.withWidth(w * 4.0f - 3.0f).expanded(0.0f, 3.0f));
            }
            int lv = steps[(size_t)i] & 0x7f;
            bool tie = (steps[(size_t)i] & GATE_TIE) != 0;
            auto bar = c.withTrimmedTop(c.getHeight() * (1.0f - (float)lv / 100.0f));
            juce::Colour cc = col::cyan.withAlpha(inLen ? (on ? 0.9f : 0.45f) : 0.12f);
            if (i == step && on)
                cc = juce::Colours::white;
            if (lv > 0) {
                g.setColour(cc.withMultipliedAlpha(0.3f));
                g.fillRect(bar.expanded(1.5f, 0.0f));
                g.setColour(cc);
                g.fillRect(tie ? bar.withX(bar.getX() - 3.0f).withWidth(bar.getWidth() + 3.0f) : bar);
            } else {
                g.setColour(juce::Colours::white.withAlpha(inLen ? 0.12f : 0.04f));
                g.fillRect(c.withTop(c.getBottom() - 3.0f));
            }
            if (i == step && on) {
                g.setColour(juce::Colours::white.withAlpha(0.25f));
                g.drawRect(c, 1.0f);
            }
        }
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawRoundedRectangle(r, 4.0f, 1.5f);
    }
    void poll() override
    {
        auto s = P.proc.gateSteps();
        int l = P.proc.gateLength(), st = P.proc.gateStepNow.load();
        if (s != steps || l != len || st != step) {
            steps = s;
            len = l;
            step = st;
            repaint();
        }
    }
    /* left: the level where the mouse is (drag across to draw); right (or Ctrl): a tie; Shift: a rest */
    void mouseDown(const juce::MouseEvent &e) override
    {
        mode = e.mods.isPopupMenu() || e.mods.isCtrlDown() ? 1 : e.mods.isShiftDown() ? 2 : 0;
        lastStep = -1;
        draw(e);
    }
    void mouseDrag(const juce::MouseEvent &e) override { draw(e); }

private:
    void draw(const juce::MouseEvent &e)
    {
        int i = juce::jlimit(0, GATE_STEPS - 1, (int)(e.position.x / ((float)getWidth() / (float)GATE_STEPS)));
        if (mode == 1) {
            if (i == lastStep)
                return;
            lastStep = i;
            int cur = steps[(size_t)i];
            int lv = cur & 0x7f;
            P.proc.editGateStep(i, (cur & GATE_TIE) ? lv : ((lv ? lv : 100) | GATE_TIE));
        } else if (mode == 2) {
            P.proc.editGateStep(i, 0);
        } else {
            float f = 1.0f - juce::jlimit(0.0f, 1.0f, (e.position.y - 4.0f) / ((float)getHeight() - 8.0f));
            int lv = (int)std::lround(f * 100.0f);
            lv = lv < 8 ? 0 : lv;
            P.proc.editGateStep(i, lv | (steps[(size_t)i] & GATE_TIE));
        }
        poll();
    }
    Panel &P;
    std::array<uint8_t, GATE_STEPS> steps{};
    int len = 16, step = -1, mode = 0, lastStep = -1;
};

/* ------------------------------------------------------------------------------- the ARPEGGIO's 8 x 8 --- */
class ArpGrid : public juce::Component, public Panel::Live
{
public:
    explicit ArpGrid(Panel &p) : P(p) { setRepaintsOnMouseActivity(true); }
    void paint(juce::Graphics &g) override
    {
        auto r = getLocalBounds().toFloat();
        float cw = r.getWidth() / 8.0f, ch = r.getHeight() / 8.0f;
        bool on = P.proc.value(comIdx(C_ARP_ON)) != 0;
        for (int k = 0; k < NARP; k++) {
            auto c = juce::Rectangle<float>(r.getX() + cw * (float)(k % 8), r.getY() + ch * (float)(k / 8), cw, ch).reduced(1.5f);
            bool sel = k == var;
            g.setColour(sel ? (on ? col::amber : col::amber.withAlpha(0.55f)) : juce::Colour(0xff4a4d53));
            g.fillRoundedRectangle(c, 2.0f);
            if (sel && on && beat) {
                g.setColour(juce::Colours::white.withAlpha(0.5f));
                g.fillRoundedRectangle(c.reduced(c.getWidth() * 0.3f), 1.0f);
            }
            g.setColour(juce::Colours::black.withAlpha(0.4f));
            g.drawRoundedRectangle(c, 2.0f, 0.8f);
        }
    }
    void poll() override
    {
        int v = P.proc.value(comIdx(C_ARP_VAR)), s = P.proc.arpStepNow.load();
        bool b = s >= 0 && (s % 2) == 0;
        if (v != var || b != beat) {
            var = v;
            beat = b;
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        int c = juce::jlimit(0, 7, (int)(e.position.x / ((float)getWidth() / 8.0f)));
        int r = juce::jlimit(0, 7, (int)(e.position.y / ((float)getHeight() / 8.0f)));
        setv(P, Bind::C(C_ARP_VAR), r * 8 + c);
    }

private:
    Panel &P;
    int var = -1;
    bool beat = false;
};

/* ------------------------------------------------------------------------------- the D BEAM --- */
/* the hand over the sensor: the mouse over the pad, higher up more (it lets go when the mouse leaves) */
class DBeamPad : public juce::Component, public Panel::Live
{
public:
    explicit DBeamPad(Panel &p) : P(p) {}
    void paint(juce::Graphics &g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0d0e10));
        g.fillRoundedRectangle(r, 6.0f);
        auto glass = r.reduced(8.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1c1214), glass.getCentreX(), glass.getBottom(), juce::Colour(0xff060607), glass.getCentreX(), glass.getY(), false));
        g.fillRoundedRectangle(glass, 4.0f);
        /* the beam: rises with the hand */
        if (level > 0.01f) {
            auto beam = glass.withTop(glass.getBottom() - glass.getHeight() * level);
            g.setGradientFill(juce::ColourGradient(col::red.withAlpha(0.65f), beam.getCentreX(), beam.getBottom(), col::red.withAlpha(0.0f), beam.getCentreX(), beam.getY(), false));
            g.fillRoundedRectangle(beam, 4.0f);
        }
        /* the two sensor eyes */
        for (int i = 0; i < 2; i++) {
            auto c = juce::Point<float>(glass.getCentreX() + (i ? 14.0f : -14.0f), glass.getBottom() - 16.0f);
            drawLed(g, c, 9.0f, col::red, 0.25f + 0.75f * level);
        }
        g.setColour(col::dim);
        g.setFont(font(Face::title, 12.0f));
        g.drawText(mode == DB_OFF ? "OFF" : "HAND (MOUSE) HERE", glass.withHeight(20.0f), juce::Justification::centred, false);
    }
    void poll() override
    {
        float l = P.proc.global("dbeam")->getValue();
        int m = P.proc.value(comIdx(C_DB_MODE));
        if (std::abs(l - level) > 0.004f || m != mode) {
            level = l;
            mode = m;
            repaint();
        }
    }
    void mouseMove(const juce::MouseEvent &e) override { hand(e); }
    void mouseDrag(const juce::MouseEvent &e) override { hand(e); }
    void mouseExit(const juce::MouseEvent &) override
    {
        auto *q = P.proc.global("dbeam");
        q->beginChangeGesture();
        q->setValueNotifyingHost(0.0f);
        q->endChangeGesture();
    }

private:
    void hand(const juce::MouseEvent &e)
    {
        if (mode == DB_OFF)
            return;
        float v = juce::jlimit(0.02f, 1.0f, 1.0f - (e.position.y - 8.0f) / ((float)getHeight() - 16.0f));
        auto *q = P.proc.global("dbeam");
        q->beginChangeGesture();
        q->setValueNotifyingHost(v);
        q->endChangeGesture();
    }
    Panel &P;
    float level = -1.0f;
    int mode = -1;
};

/* ------------------------------------------------------------------------------- the levers --- */
class Lever : public juce::Component
{
public:
    Lever(Panel &p, bool bendLever) : P(p), bend(bendLever) {}
    void paint(juce::Graphics &g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0b0c0e));
        g.fillRoundedRectangle(r, 5.0f);
        auto slot = r.reduced(10.0f, r.getHeight() * 0.38f);
        g.setColour(juce::Colour(0xff26282c));
        g.fillRoundedRectangle(slot, 3.0f);
        float x = bend ? slot.getCentreX() + v * slot.getWidth() * 0.5f : slot.getX() + v * slot.getWidth();
        auto cap = juce::Rectangle<float>(16.0f, r.getHeight() - 10.0f).withCentre({x, r.getCentreY()});
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a4c50), cap.getX(), cap.getY(), juce::Colour(0xff101113), cap.getRight(), cap.getY(), false));
        g.fillRoundedRectangle(cap, 3.0f);
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawVerticalLine((int)cap.getCentreX(), cap.getY() + 4.0f, cap.getBottom() - 4.0f);
    }
    void mouseDown(const juce::MouseEvent &e) override { at(e); }
    void mouseDrag(const juce::MouseEvent &e) override { at(e); }
    void mouseUp(const juce::MouseEvent &) override
    {
        if (bend) {
            v = 0.0f;
            P.proc.uiMidi.addMessageToQueue(juce::MidiMessage::pitchWheel(1, 8192).withTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001));
            repaint();
        }
    }

private:
    void at(const juce::MouseEvent &e)
    {
        auto slot = getLocalBounds().toFloat().reduced(10.0f, 0.0f);
        float f = juce::jlimit(0.0f, 1.0f, (e.position.x - slot.getX()) / slot.getWidth());
        double ts = juce::Time::getMillisecondCounterHiRes() * 0.001;
        if (bend) {
            v = f * 2.0f - 1.0f;
            P.proc.uiMidi.addMessageToQueue(juce::MidiMessage::pitchWheel(1, juce::jlimit(0, 16383, (int)std::lround(8192.0f + v * 8191.0f))).withTimeStamp(ts));
        } else {
            v = f;
            P.proc.uiMidi.addMessageToQueue(juce::MidiMessage::controllerEvent(1, 1, (int)std::lround(v * 127.0f)).withTimeStamp(ts));
        }
        repaint();
    }
    Panel &P;
    bool bend;
    float v = 0.0f;
};

/* the keys: dark ivory, as old as the rest */
class Keys : public juce::MidiKeyboardComponent
{
public:
    Keys(juce::MidiKeyboardState &s) : juce::MidiKeyboardComponent(s, horizontalKeyboard)
    {
        setColour(whiteNoteColourId, juce::Colour(0xffe6dcc2));
        setColour(blackNoteColourId, juce::Colour(0xff141416));
        setColour(keySeparatorLineColourId, juce::Colour(0xff7a705c));
        setColour(keyDownOverlayColourId, col::amber.withAlpha(0.55f));
        setColour(mouseOverKeyOverlayColourId, col::amber.withAlpha(0.18f));
        setColour(shadowColourId, juce::Colour(0x50000000));
        setColour(upDownButtonBackgroundColourId, juce::Colour(0xff2a2c30));
        setColour(upDownButtonArrowColourId, col::legend);
        setScrollButtonsVisible(false);
        setAvailableRange(36, 96);
        setOctaveForMiddleC(4);
        setWantsKeyboardFocus(false);
    }
};
} // namespace

/* ============================================================================== the panel === */
template <class T, class... A> T *Panel::add(A &&...a)
{
    auto c = std::make_unique<T>(std::forward<A>(a)...);
    T *p = c.get();
    addAndMakeVisible(*p);
    if (auto *lv = dynamic_cast<Live *>(p))
        live.push_back(lv);
    parts.push_back(std::move(c));
    return p;
}

Panel::GalaPanel(GalaProcessor &p) : proc(p)
{
    setLookAndFeel(&look);
    setSize(W, H);
    build();
    for (auto *l : live)
        l->poll();
    startTimerHz(30);
}

Panel::~GalaPanel()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

int Panel::tone() const
{
    int s = proc.selectedTones.load();
    return (s & 1) ? 0 : (s & 2) ? 1 : (s & 4) ? 2 : 0;
}

void Panel::touch(int idx)
{
    shownIdx = idx;
    shownGlobal = nullptr;
    shownAt = juce::Time::getMillisecondCounterHiRes();
}
void Panel::touchGlobal(juce::RangedAudioParameter *q)
{
    shownGlobal = q;
    shownIdx = -1;
    shownAt = juce::Time::getMillisecondCounterHiRes();
}

void Panel::timerCallback()
{
    for (auto *l : live)
        l->poll();
}

void Panel::patchMenu()
{
    juce::PopupMenu m;
    for (int b = 0; b < 8; b++) {
        juce::PopupMenu sub;
        for (int k = 0; k < 8; k++) {
            int i = b * 8 + k;
            sub.addItem(1 + i, juce::String::charToString((juce::juce_wchar)('A' + b)) + juce::String(k + 1) + "  " + factory(i).name, true, proc.getCurrentProgram() == i);
        }
        m.addSubMenu(juce::String::charToString((juce::juce_wchar)('A' + b)) + "  " + BANK_NAME[b], sub);
    }
    if (!proc.userPatches.empty()) {
        juce::PopupMenu u;
        for (size_t i = 0; i < proc.userPatches.size(); i++)
            u.addItem(1 + NFACTORY + (int)i, proc.userPatches[i].name, true, proc.getCurrentProgram() == NFACTORY + (int)i);
        m.addSubMenu("USER", u);
    }
    m.addSeparator();
    m.addItem(1000, "WRITE (save as user patch) ...");
    m.addItem(1001, "INIT PATCH");
    m.addItem(1002, "Open the patch folder");
    juce::Component::SafePointer<Panel> self(this);
    m.showMenuAsync(juce::PopupMenu::Options(), [self](int r) {
        if (!self || r <= 0)
            return;
        if (r == 1000)
            self->writeDialog();
        else if (r == 1001)
            self->proc.initPatch();
        else if (r == 1002) {
            GalaProcessor::patchFolder().createDirectory();
            GalaProcessor::patchFolder().revealToUser();
        } else
            self->proc.setCurrentProgram(r - 1);
    });
}

void Panel::writeDialog()
{
    auto *w = new juce::AlertWindow("WRITE", "Save the sound as a user patch:", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel(&look);
    w->addTextEditor("name", proc.patchName(), "Name");
    w->addButton("WRITE", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog = w;
    juce::Component::SafePointer<Panel> self(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([self, w](int r) {
        if (self && r == 1)
            self->proc.writePatch(w->getTextEditorContents("name"));
    }), true);
}

/* ------------------------------------------------------------------------------- the layout --- */
void Panel::build()
{
    using juce::Colour;
    namespace Colours = juce::Colours;
    GalaProcessor &pr = proc;
    auto none = Colours::transparentBlack;
    auto L = [this](const juce::String &t, float x, float y, float w, float size = 13.0f, Colour c = col::legend) {
        legends.push_back({t, {x - w * 0.5f, y, w, size + 4.0f}, size, c});
    };
    auto S = [this](float x0, float y0, float x1, float y1, const char *t, Colour c) {
        sections.push_back({{x0, y0, x1 - x0, y1 - y0}, {t, c}});
    };
    /* a knob at its centre, its legend over it */
    auto K = [&](float cx, float cy, float d, Bind b, const juce::String &leg, Colour lc = col::legend) {
        auto *k = add<Knob>(*this, [b] { return b; });
        k->setBounds(juce::Rectangle<int>((int)d, (int)d).withCentre({(int)cx, (int)cy}));
        if (leg.isNotEmpty())
            L(leg, cx, cy - d * 0.5f - 18.0f, d + 40.0f, d < 50.0f ? 12.0f : 13.5f, lc);
        return k;
    };
    auto F = [&](float cx, float y0, float y1, Bind b, const juce::String &leg, Colour lc = col::legend) {
        auto *f = add<Fader>(*this, b);
        f->setBounds((int)(cx - 20.0f), (int)y0, 40, (int)(y1 - y0));
        L(leg, cx, y0 - 19.0f, 56.0f, 12.5f, lc);
        return f;
    };
    auto B = [&](float x, float y, float w, float h, std::function<Colour()> lit, std::function<void(const juce::MouseEvent &)> click,
                 const juce::String &text = {}) {
        auto *b = add<Btn>(std::move(lit), std::move(click), text);
        b->setBounds((int)x, (int)y, (int)w, (int)h);
        return b;
    };
    auto toggle = [&](Bind b) {
        return [this, b](const juce::MouseEvent &) { setv(*this, b, ival(*this, b) ? 0 : 1); };
    };
    auto litIf = [this](Bind b, Colour c) { return [this, b, c] { return ival(*this, b) ? c : Colours::transparentBlack; }; };
    auto cycle = [this](Bind b) {
        return [this, b](const juce::MouseEvent &e) {
            int lo, hi;
            rangeOf(*this, b, lo, hi);
            int v = ival(*this, b) + (e.mods.isShiftDown() ? -1 : 1);
            setv(*this, b, v > hi ? lo : v < lo ? hi : v);
        };
    };

    /* ============================================================ the top: name, CIRCUIT MOD, display, patches */
    S(372, 36, 664, 118, "", col::orange);
    {
        /* CIRCUIT MOD: its button flickers like a tired lamp, now and then even when off, to be noticed */
        struct Flicker {
            float level = 0.0f;
            int burst = 0;
            juce::Random rnd;
        };
        auto fl = std::make_shared<Flicker>();
        Bind cm = Bind::C(C_CMOD);
        auto *b = B(384, 64, 92, 34, [this, fl, cm] {
            bool on = ival(*this, cm) != 0;
            auto &r = fl->rnd;
            if (on) {
                /* on: lit, but never quite steady */
                fl->level = 0.55f + 0.45f * r.nextFloat();
                if (r.nextFloat() < 0.06f)
                    fl->level = 0.1f;
                return col::orange.withMultipliedBrightness(fl->level).withAlpha(1.0f);
            }
            /* off: dark, but every few seconds a stutter of light */
            if (fl->burst > 0) {
                fl->burst--;
                return r.nextFloat() < 0.5f ? col::orange.withMultipliedBrightness(0.4f + 0.6f * r.nextFloat()) : Colours::transparentBlack;
            }
            if (r.nextFloat() < 0.012f)
                fl->burst = 4 + r.nextInt(10);
            return Colours::transparentBlack;
        }, toggle(cm), "CIRCUIT MOD");
        L("CIRCUIT MOD", 430, 44, 100, 12.0f, col::orange);
        K(520, 86, 40, Bind::C(C_CMOD_AGE), "AGE", col::orange);
        K(576, 86, 40, Bind::C(C_CMOD_DRIFT), "DRIFT", col::orange);
        K(632, 86, 40, Bind::C(C_CMOD_NOISE), "NOISE", col::orange);
    }
    {
        auto *lcd = add<Lcd>(*this);
        lcd->setBounds(676, 38, 380, 78);
        B(1062, 40, 40, 36, [] { return Colours::transparentBlack; }, [&pr](const juce::MouseEvent &) {
            pr.setCurrentProgram((pr.getCurrentProgram() + pr.getNumPrograms() - 1) % pr.getNumPrograms());
        }, "<");
        B(1062, 78, 40, 36, [] { return Colours::transparentBlack; }, [&pr](const juce::MouseEvent &) {
            pr.setCurrentProgram((pr.getCurrentProgram() + 1) % pr.getNumPrograms());
        }, ">");
        L("BANK", 1132, 36, 50, 11.0f, col::dim);
        L("NUMBER", 1140, 76, 60, 11.0f, col::dim);
        for (int i = 0; i < 8; i++) {
            float x = 1112 + i * 46.0f;
            B(x, 48, 44, 26, [&pr, i] { int p = pr.getCurrentProgram(); return p < NFACTORY && p / 8 == i ? col::red : Colours::transparentBlack; },
              [&pr, i](const juce::MouseEvent &) { int p = pr.getCurrentProgram(); pr.setCurrentProgram(i * 8 + (p < NFACTORY ? p % 8 : 0)); },
              juce::String::charToString((juce::juce_wchar)('A' + i)));
            B(x, 88, 44, 26, [&pr, i] { int p = pr.getCurrentProgram(); return p < NFACTORY && p % 8 == i ? col::red : Colours::transparentBlack; },
              [&pr, i](const juce::MouseEvent &) { int p = pr.getCurrentProgram(); pr.setCurrentProgram((p < NFACTORY ? p / 8 : 0) * 8 + i); },
              juce::String(i + 1));
        }
        B(1490, 48, 62, 26, [] { return Colours::transparentBlack; }, [this](const juce::MouseEvent &) { writeDialog(); }, "WRITE");
        B(1490, 88, 62, 26, [] { return Colours::transparentBlack; }, [&pr](const juce::MouseEvent &) { pr.initPatch(); }, "INIT");
        auto *v = B(1560, 48, 96, 26, [] { return Colours::transparentBlack; }, cycle(Bind::G("voices")));
        v->dynText = [&pr] { return pr.global("voices")->getCurrentValueAsText() + " VOICES"; };
        B(1560, 88, 96, 26, litIf(Bind::G("host_sync"), col::green), toggle(Bind::G("host_sync")), "HOST SYNC");
    }

    /* ============================================================ D BEAM */
    const float Y0 = 132, Y1 = 512;
    S(34, Y0, 184, Y1, "D BEAM", Colour(0xffd9d9d9));
    {
        auto *pad = add<DBeamPad>(*this);
        pad->setBounds(46, 166, 126, 150);
        Bind m = Bind::C(C_DB_MODE);
        const char *nm[3] = {"PITCH", "VOLUME", "ASSIGN"};
        for (int i = 0; i < 3; i++) {
            float x = 46 + i * 43.0f;
            L(nm[i], x + 20, 326, 50, 11.0f);
            B(x, 344, 40, 24, [this, m, i] { return ival(*this, m) == i + 1 ? col::red : Colours::transparentBlack; },
              [this, m, i](const juce::MouseEvent &) { setv(*this, m, ival(*this, m) == i + 1 ? 0 : i + 1); });
        }
        L("ASSIGN TARGET", 109, 380, 120, 11.0f, col::dim);
        auto *t = add<TextBox>([&pr] { return juce::String(DBTARGET[pr.value(comIdx(C_DB_ASSIGN)) % NDBTARGET].name); }, [this] {
            juce::PopupMenu m;
            for (int i = 0; i < NDBTARGET; i++)
                m.addItem(1 + i, DBTARGET[i].name, true, proc.value(comIdx(C_DB_ASSIGN)) == i);
            juce::Component::SafePointer<Panel> self(this);
            m.showMenuAsync(juce::PopupMenu::Options(), [self](int r) {
                if (self && r > 0)
                    setv(*self, Bind::C(C_DB_ASSIGN), r - 1);
            });
        });
        t->setBounds(44, 398, 130, 24);
        t->size = 13.0f;
        L("POLARITY", 70, 432, 60, 11.0f, col::dim);
        auto *pb = B(46, 450, 48, 24, [] { return Colours::transparentBlack; }, cycle(Bind::C(C_DB_POL)));
        pb->dynText = [&pr] { return pr.value(comIdx(C_DB_POL)) ? juce::String("-") : juce::String("+"); };
        L("TONES", 136, 432, 60, 11.0f, col::dim);
        auto *tb = B(100, 450, 76, 24, [] { return Colours::transparentBlack; }, cycle(Bind::C(C_DB_TONES)));
        tb->dynText = [&pr] { static const char *n[4] = {"ALL", "T1", "T2", "T3"}; return juce::String(n[pr.value(comIdx(C_DB_TONES)) & 3]); };
    }

    /* ============================================================ TONE */
    S(194, Y0, 324, Y1, "TONE", Colour(0xffd9d9d9));
    for (int t = 0; t < 3; t++) {
        float y = 176 + t * 86.0f;
        L("TONE " + juce::String(t + 1), 259, y, 110, 15.0f);
        L("SELECT", 228, y + 50, 56, 10.5f, col::dim);
        L("ON", 290, y + 50, 56, 10.5f, col::dim);
        B(206, y + 22, 46, 28, [&pr, t] { return (pr.selectedTones.load() & (1 << t)) ? col::green : Colours::transparentBlack; },
          [&pr, t](const juce::MouseEvent &e) {
              /* (Shift: several tones at once, as pressing two SELECTs together on the GAIA) */
              int s = pr.selectedTones.load();
              if (e.mods.isShiftDown() || e.mods.isCommandDown()) {
                  s ^= 1 << t;
                  if (s == 0)
                      s = 1 << t;
              } else
                  s = 1 << t;
              pr.selectedTones = s;
          });
        Bind on = Bind::C(0);
        B(268, y + 22, 46, 28, [&pr, t] {
            if (!pr.value(toneIdx(t, T_ON)))
                return Colours::transparentBlack;
            return (t < 2 && pr.value(comIdx(C_MOD)) != MM_OFF) ? col::orange : col::red;
        }, [&pr, t](const juce::MouseEvent &) { pr.setValue(toneIdx(t, T_ON), pr.value(toneIdx(t, T_ON)) ? 0 : 1); });
        (void)on;
    }
    L("TONE COPY", 259, 436, 100, 11.5f, col::dim);
    B(224, 454, 70, 26, [] { return Colours::transparentBlack; }, [this](const juce::MouseEvent &) {
        juce::PopupMenu m;
        for (int a = 0; a < 3; a++)
            for (int b = 0; b < 3; b++)
                if (a != b)
                    m.addItem(1 + a * 3 + b, "Tone " + juce::String(a + 1) + "  ->  Tone " + juce::String(b + 1));
        juce::Component::SafePointer<Panel> self(this);
        m.showMenuAsync(juce::PopupMenu::Options(), [self](int r) {
            if (self && r > 0)
                self->proc.copyTone((r - 1) / 3, (r - 1) % 3);
        });
    }, "COPY");

    /* ============================================================ LFO */
    S(334, Y0, 624, Y1, "LFO", col::blue);
    {
        L("SHAPE", 372, 160, 60, 12.0f);
        B(348, 176, 50, 26, [] { return Colours::transparentBlack; }, cycle(Bind::T(T_LFO_SHAPE)));
        add<Leds>(*this, Bind::T(T_LFO_SHAPE), juce::StringArray{"TRI", "SIN", "SAW", "SQR", "S&H", "RND"}, col::red)->setBounds(410, 160, 66, 108);
        L("TEMPO SYNC", 373, 212, 80, 11.0f);
        B(348, 228, 50, 26, litIf(Bind::T(T_LFO_SYNC), col::red), toggle(Bind::T(T_LFO_SYNC)));
        L("KEY TRIG", 373, 262, 80, 11.0f);
        B(348, 278, 50, 26, litIf(Bind::C(C_LFO_KEYTRIG), col::red), toggle(Bind::C(C_LFO_KEYTRIG)));
        K(552, 214, 82, Bind::T(T_LFO_RATE), "RATE");
        L("SLOW", 522, 262, 40, 10.0f, col::dim);
        L("FAST", 584, 262, 40, 10.0f, col::dim);
        F(366, 334, 492, Bind::T(T_LFO_FADE), "FADE TIME");
        F(420, 334, 492, Bind::T(T_LFO_PITCH), "PITCH", col::amber);
        F(474, 334, 492, Bind::T(T_LFO_FILTER), "FILTER", col::amber);
        F(528, 334, 492, Bind::T(T_LFO_AMP), "AMP", col::amber);
        F(582, 334, 492, Bind::T(T_LFO_PAN), "PAN", col::amber);
        L("DEPTH", 501, 298, 200, 11.0f, col::amber);
    }

    /* ============================================================ OSC */
    S(634, Y0, 994, Y1, "OSC", col::amber);
    {
        L("WAVE", 672, 160, 60, 12.0f);
        B(648, 176, 50, 26, [] { return Colours::transparentBlack; }, cycle(Bind::T(T_WAVE)));
        add<Leds>(*this, Bind::T(T_WAVE), juce::StringArray{"SAW", "SQUARE", "PULSE", "TRI", "SINE", "NOISE", "SUPER SAW"}, col::red)
            ->setBounds(708, 156, 94, 130);
        L("VARIATION", 673, 216, 80, 11.0f);
        B(648, 232, 50, 26, [] { return Colours::transparentBlack; }, cycle(Bind::T(T_VAR)));
        add<Leds>(*this, Bind::T(T_VAR), juce::StringArray{"1", "2", "3"}, col::green, true)->setBounds(644, 264, 60, 30);
        K(852, 196, 70, Bind::T(T_PITCH), "PITCH");
        K(940, 196, 70, Bind::T(T_DETUNE), "DETUNE");
        L("MOD (TONE 2 > 1)", 896, 248, 160, 11.0f, col::amber);
        B(820, 266, 50, 26, [] { return Colours::transparentBlack; }, cycle(Bind::C(C_MOD)));
        add<Leds>(*this, Bind::C(C_MOD), juce::StringArray{"OFF", "SYNC", "RING"}, col::orange, true)->setBounds(874, 262, 112, 34);
        F(662, 334, 492, Bind::T(T_PWM), "PWM", juce::Colour(0xff6aa8ff));
        F(718, 334, 492, Bind::T(T_PW), "PW");
        F(800, 334, 492, Bind::T(T_PENV_A), "A");
        F(856, 334, 492, Bind::T(T_PENV_D), "D");
        F(946, 334, 492, Bind::T(T_PENV_DEPTH), "ENV DEPTH");
        L("PITCH ENV", 870, 298, 200, 11.0f, col::amber);
    }

    /* ============================================================ FILTER */
    S(1004, Y0, 1274, Y1, "FILTER", col::amber);
    {
        L("MODE", 1042, 160, 60, 12.0f);
        B(1018, 176, 50, 26, [] { return Colours::transparentBlack; }, cycle(Bind::T(T_FMODE)));
        add<Leds>(*this, Bind::T(T_FMODE), juce::StringArray{"LPF", "HPF", "BPF", "PKG", "BYPASS"}, col::red)->setBounds(1076, 156, 74, 96);
        L("SLOPE", 1042, 248, 60, 12.0f);
        B(1018, 264, 50, 26, [] { return Colours::transparentBlack; }, cycle(Bind::T(T_SLOPE)));
        add<Leds>(*this, Bind::T(T_SLOPE), juce::StringArray{"-12 dB", "-24 dB"}, col::red)->setBounds(1076, 258, 74, 40);
        K(1186, 192, 64, Bind::T(T_CUTOFF), "CUTOFF");
        K(1186, 278, 44, Bind::T(T_KEYF), "KEY FOLLOW");
        K(1246, 278, 40, Bind::C(C_FENV_VEL), "ENV VEL", col::dim);
        K(1246, 192, 50, Bind::T(T_RESO), "RESO");
        F(1032, 334, 492, Bind::T(T_FENV_A), "A");
        F(1082, 334, 492, Bind::T(T_FENV_D), "D");
        F(1132, 334, 492, Bind::T(T_FENV_S), "S");
        F(1182, 334, 492, Bind::T(T_FENV_R), "R");
        F(1242, 334, 492, Bind::T(T_FENV_DEPTH), "DEPTH");
    }

    /* ============================================================ AMP */
    S(1284, Y0, 1444, Y1, "AMP", col::amber);
    {
        K(1330, 214, 62, Bind::T(T_LEVEL), "LEVEL");
        K(1408, 196, 42, Bind::T(T_PAN), "PAN");
        K(1408, 276, 42, Bind::C(C_LEVEL_VEL), "VELOCITY", col::dim);
        F(1308, 334, 492, Bind::T(T_AENV_A), "A");
        F(1350, 334, 492, Bind::T(T_AENV_D), "D");
        F(1392, 334, 492, Bind::T(T_AENV_S), "S");
        F(1432, 334, 492, Bind::T(T_AENV_R), "R");
    }

    /* ============================================================ EFFECTS */
    S(1454, Y0, 1666, Y1, "EFFECTS", Colour(0xffd9d9d9));
    {
        /* SELECT CONTROL: the block whose knobs show (it follows the block last pressed) */
        static const char *BLOCK[5] = {"DIST", "FLANGER", "DELAY", "REVERB", "LOW BOOST"};
        auto *sel = add<TextBox>([&pr] {
            int s = pr.fxSelect.load();
            if (s == 0) { static const char *n[4] = {"DIST", "DIST", "FUZZ", "BIT CRASH"}; return juce::String(n[pr.value(comIdx(C_DIST)) & 3]); }
            if (s == 1) { static const char *n[4] = {"FLANGER", "FLANGER", "PHASER", "PITCH SHIFTER"}; return juce::String(n[pr.value(comIdx(C_MODFX)) & 3]); }
            if (s == 2) return juce::String(pr.value(comIdx(C_DELAY)) == DL_PANNING ? "PANNING DELAY" : "DELAY");
            return juce::String(BLOCK[s]);
        }, [&pr] { pr.fxSelect = (pr.fxSelect.load() + 1) % 5; });
        sel->setBounds(1560, 138, 98, 22);
        sel->size = 12.5f;
        auto fxBind = [&pr](int k) {
            return [&pr, k]() -> Bind {
                int s = pr.fxSelect.load();
                if (s == 0) {
                    int d = pr.value(comIdx(C_DIST));
                    int base = d == DT_FUZZ ? C_FUZZ_DRIVE : d == DT_BITCRASH ? C_BC_RATE : C_DIST_DRIVE;
                    return Bind::C(base + k);
                }
                if (s == 1) {
                    int m = pr.value(comIdx(C_MODFX));
                    int base = m == MF_PHASER ? C_PH_RESO : m == MF_PITCH ? C_PS_PITCH : C_FL_FB;
                    return Bind::C(base + k);
                }
                if (s == 2) {
                    int base = pr.value(comIdx(C_DELAY)) == DL_PANNING ? C_PD_TIME : C_DL_TIME;
                    return Bind::C(base + k);
                }
                if (s == 3)
                    return Bind::C(C_RV_TIME + k);
                return k == 0 ? Bind::C(C_LB_AMOUNT) : Bind::none();
            };
        };
        auto fxName = [&pr](int k) {
            return [&pr, k]() -> juce::String {
                static const char *N[9][4] = {{"DRIVE", "TYPE", "PRESENCE", "LEVEL"},  {"DRIVE", "TYPE", "PRESENCE", "LEVEL"},
                                              {"RATE", "BIT DOWN", "FILTER", "LEVEL"}, {"FEEDBACK", "DEPTH", "RATE", "LEVEL"},
                                              {"RESO", "DEPTH", "RATE", "LEVEL"},      {"PITCH", "DETUNE", "PITCH 2", "LEVEL"},
                                              {"TIME", "FEEDBACK", "HI DAMP", "LEVEL"}, {"TIME", "TYPE", "HI DAMP", "LEVEL"},
                                              {"AMOUNT", "", "", ""}};
                int s = pr.fxSelect.load(), row;
                if (s == 0) row = juce::jmax(0, pr.value(comIdx(C_DIST)) - 1);
                else if (s == 1) row = 3 + juce::jmax(0, pr.value(comIdx(C_MODFX)) - 1);
                else row = 4 + s;
                return N[row][k];
            };
        };
        for (int k = 0; k < 4; k++) {
            float cx = 1482 + k * 52.0f;
            auto *kn = add<Knob>(*this, fxBind(k));
            kn->setBounds(juce::Rectangle<int>(42, 42).withCentre({(int)cx, 204}));
            auto *t = add<TextBox>(fxName(k), std::function<void()>{}, col::legend, false);
            t->setBounds((int)cx - 26, 166, 52, 16);
            t->size = 11.0f;
        }
        auto blk = [&](int col, int row, const char *leg, std::function<Colour()> lit, std::function<void()> act) {
            float x = 1462 + col * 51.0f, y = 268 + row * 54.0f;
            L(leg, x + 23, y - 16, 52, 10.0f);
            B(x, y, 46, 26, std::move(lit), [act](const juce::MouseEvent &) { act(); });
        };
        auto typeBtn = [&](int c, int row, const char *leg, int param, int val, int block) {
            blk(c, row, leg, [&pr, param, val] { return pr.value(comIdx(param)) == val ? col::red : Colours::transparentBlack; },
                [&pr, param, val, block] {
                    pr.setValue(comIdx(param), pr.value(comIdx(param)) == val ? 0 : val);
                    pr.fxSelect = block;
                });
        };
        typeBtn(0, 0, "DIST", C_DIST, DT_DIST, 0);
        typeBtn(0, 1, "FUZZ", C_DIST, DT_FUZZ, 0);
        typeBtn(0, 2, "BIT CRASH", C_DIST, DT_BITCRASH, 0);
        typeBtn(1, 0, "FLANGER", C_MODFX, MF_FLANGER, 1);
        typeBtn(1, 1, "PHASER", C_MODFX, MF_PHASER, 1);
        typeBtn(1, 2, "PITCH SH.", C_MODFX, MF_PITCH, 1);
        typeBtn(2, 0, "DELAY", C_DELAY, DL_DELAY, 2);
        typeBtn(2, 1, "PAN DELAY", C_DELAY, DL_PANNING, 2);
        blk(2, 2, "TEMPO SYNC", [&pr] { return pr.value(comIdx(C_DELAY_SYNC)) ? col::red : Colours::transparentBlack; },
            [&pr] { pr.setValue(comIdx(C_DELAY_SYNC), pr.value(comIdx(C_DELAY_SYNC)) ? 0 : 1); pr.fxSelect = 2; });
        blk(3, 0, "REVERB", [&pr] { return pr.value(comIdx(C_REVERB)) ? col::red : Colours::transparentBlack; },
            [&pr] { pr.setValue(comIdx(C_REVERB), pr.value(comIdx(C_REVERB)) ? 0 : 1); pr.fxSelect = 3; });
        blk(3, 1, "LOW BOOST", [&pr] { return pr.value(comIdx(C_LOWBOOST)) ? col::red : Colours::transparentBlack; },
            [&pr] { pr.setValue(comIdx(C_LOWBOOST), pr.value(comIdx(C_LOWBOOST)) ? 0 : 1); pr.fxSelect = 4; });
        blk(3, 2, "EFX ON", [&pr] { return pr.value(comIdx(C_FX_ON)) ? col::red : Colours::transparentBlack; },
            [&pr] { pr.setValue(comIdx(C_FX_ON), pr.value(comIdx(C_FX_ON)) ? 0 : 1); });
        L("SELECT CONTROL: the block last pressed", 1560, 432, 200, 9.5f, col::dim);
        L("(click the name above to step)", 1560, 446, 200, 9.5f, col::dim);
    }

    /* ============================================================ MOD LFO */
    const float Z0 = 524, Z1 = 772;
    S(34, Z0, 324, Z1, "MOD LFO", col::blue);
    {
        L("for the mod lever", 220, 532, 150, 11.0f, col::dim);
        L("SHAPE", 72, 560, 60, 11.5f);
        B(48, 576, 50, 26, [] { return Colours::transparentBlack; }, cycle(Bind::T(T_MLFO_SHAPE)));
        auto *t = add<TextBox>([this] { return juce::String(LSHAPE_NAME[proc.value(toneIdx(tone(), T_MLFO_SHAPE)) % NLSHAPE]).toUpperCase(); });
        t->setBounds(104, 578, 80, 22);
        t->size = 13.0f;
        L("TEMPO SYNC", 73, 612, 80, 11.0f);
        B(48, 628, 50, 26, litIf(Bind::T(T_MLFO_SYNC), col::red), toggle(Bind::T(T_MLFO_SYNC)));
        K(252, 616, 64, Bind::T(T_MLFO_RATE), "RATE");
        K(72, 720, 44, Bind::T(T_MLFO_PITCH), "PITCH", col::amber);
        K(138, 720, 44, Bind::T(T_MLFO_FILTER), "FILTER", col::amber);
        K(204, 720, 44, Bind::T(T_MLFO_AMP), "AMP", col::amber);
        K(270, 720, 44, Bind::T(T_MLFO_PAN), "PAN", col::amber);
    }

    /* ============================================================ PERFORM */
    S(334, Z0, 624, Z1, "PERFORM", Colour(0xffd9d9d9));
    {
        struct Tg { const char *n; Bind b; Colour c; };
        Tg tg[5] = {{"MONO", Bind::C(C_MONO), col::red}, {"LEGATO", Bind::C(C_LEGATO), col::red},
                    {"PORTAMENTO", Bind::C(C_PORTA), col::red}, {"KEY HOLD", Bind::G("key_hold"), col::green},
                    {"VELO FIX", Bind::G("velocity"), col::red}};
        for (int i = 0; i < 5; i++) {
            float x = 346 + i * 55.0f;
            L(tg[i].n, x + 25, 562, 60, 10.0f);
            B(x, 578, 50, 26, litIf(tg[i].b, tg[i].c), toggle(tg[i].b));
        }
        K(372, 660, 44, Bind::C(C_PORTA_TIME), "PORTA TIME");
        K(436, 660, 44, Bind::C(C_BEND_UP), "BEND UP");
        K(500, 660, 44, Bind::C(C_BEND_DOWN), "BEND DOWN");
        K(570, 660, 44, Bind::C(C_TEMPO), "TEMPO");
        /* OCTAVE, TRANSPOSE: - and + with the value between; TAP TEMPO */
        auto step = [&](float x, const char *leg, Bind b) {
            L(leg, x + 54, 694, 110, 11.0f);
            B(x, 712, 36, 26, [] { return Colours::transparentBlack; }, [this, b](const juce::MouseEvent &) { setv(*this, b, ival(*this, b) - 1); }, "-");
            auto *t = add<TextBox>([this, b] { int v = ival(*this, b); return (v > 0 ? "+" : "") + juce::String(v); });
            t->setBounds((int)x + 38, 714, 32, 22);
            t->size = 14.0f;
            B(x + 72, 712, 36, 26, [] { return Colours::transparentBlack; }, [this, b](const juce::MouseEvent &) { setv(*this, b, ival(*this, b) + 1); }, "+");
        };
        step(344, "OCTAVE", Bind::C(C_OCTAVE));
        step(458, "TRANSPOSE", Bind::G("transpose"));
        L("TAP", 594, 694, 40, 11.0f);
        B(572, 712, 44, 26, [&pr] { return pr.beatNow.load() ? col::green : Colours::transparentBlack; }, [this](const juce::MouseEvent &) {
            double now = juce::Time::getMillisecondCounterHiRes();
            if (!taps.empty() && now - taps.back() > 2000.0)
                taps.clear();
            taps.push_back(now);
            if (taps.size() > 5)
                taps.erase(taps.begin());
            if (taps.size() >= 2) {
                double avg = (taps.back() - taps.front()) / (double)(taps.size() - 1);
                setv(*this, Bind::C(C_TEMPO), (int)std::lround(60000.0 / avg));
            }
        });
    }

    /* ============================================================ TRANCE GATE */
    S(634, Z0, 1274, Z1, "TRANCE GATE", col::cyan);
    {
        B(804, 528, 54, 28, litIf(Bind::C(C_GATE_ON), col::cyan), toggle(Bind::C(C_GATE_ON)), "ON");
        auto *pat = add<TextBox>([&pr] {
            int k = pr.value(comIdx(C_GATE_PAT));
            return "PATTERN:  " + juce::String(GATEPAT_NAME[juce::jlimit(0, NGATEPAT, k)]).toUpperCase() + "   v";
        }, [this] {
            juce::PopupMenu m;
            m.addSectionHeader("TRANCE GATE PATTERNS");
            static const char *GROUPS[5] = {"Classic", "Rhythmic", "Rolling & Fast", "Smooth & Pump", "Build & FX"};
            static const int GROUP_OF[NGATEPAT] = {0, 0, 0, 3, 1, 1, 2, 4, 1, 2, 1, 3, 1, 1, 1, 1, 1, 1, 4, 2,
                                                   2, 1, 0, 2, 0, 1, 3, 4, 0, 1, 1, 1, 2, 1, 1, 3, 3, 2, 4, 3};
            juce::PopupMenu sub[5];
            int cur = proc.value(comIdx(C_GATE_PAT));
            for (int k = 0; k < NGATEPAT; k++)
                sub[GROUP_OF[k]].addItem(1 + k, juce::String(GATEPAT[k].name) + "   (" + GATERATE_NAME[GATEPAT[k].rate] + ")", true, cur == k);
            for (int g = 0; g < 5; g++)
                m.addSubMenu(GROUPS[g], sub[g]);
            m.addSeparator();
            m.addItem(500, "User (draw the steps)", true, cur == NGATEPAT);
            juce::Component::SafePointer<Panel> self(this);
            m.showMenuAsync(juce::PopupMenu::Options(), [self](int r) {
                if (!self || r <= 0)
                    return;
                if (r == 500) {
                    self->proc.editGateStep(0, self->proc.gateSteps()[0]);
                } else
                    self->proc.loadGatePattern(r - 1);
            });
        }, col::cyan);
        pat->setBounds(866, 530, 250, 26);
        pat->size = 14.0f;
        L("TONES", 1146, 532, 50, 10.0f, col::dim);
        for (int t = 0; t < 3; t++)
            B(1172 + t * 32.0f, 528, 30, 28, litIf(Bind::C(C_GATE_T1 + t), col::cyan), toggle(Bind::C(C_GATE_T1 + t)), juce::String(t + 1));
        auto *grid = add<GateGrid>(*this);
        grid->setBounds(648, 564, 612, 112);
        L("drag: level   right-click: tie   shift: rest", 954, 676, 400, 10.0f, col::dim);
        struct Gk { const char *n; int p; };
        Gk gk[7] = {{"RATE", C_GATE_RATE}, {"LENGTH", C_GATE_LEN}, {"SWING", C_GATE_SWING}, {"ATTACK", C_GATE_ATT},
                    {"RELEASE", C_GATE_REL}, {"DEPTH", C_GATE_DEPTH}, {"FILTER", C_GATE_FILTER}};
        for (int i = 0; i < 7; i++)
            K(690 + i * 86.0f, 740, 38, Bind::C(gk[i].p), gk[i].n, col::cyan);
    }

    /* ============================================================ ARPEGGIO */
    S(1284, Z0, 1444, Z1, "ARPEGGIO", col::amber);
    {
        B(1388, 528, 50, 24, litIf(Bind::C(C_ARP_ON), col::red), toggle(Bind::C(C_ARP_ON)), "ON");
        auto *nm = add<TextBox>([&pr] {
            int v = pr.value(comIdx(C_ARP_VAR));
            return juce::String::charToString((juce::juce_wchar)('A' + v / 8)) + juce::String(v % 8 + 1) + " " + ARP[v % NARP].name;
        });
        nm->setBounds(1292, 558, 146, 22);
        nm->size = 12.5f;
        for (int i = 0; i < 8; i++) {
            L(juce::String::charToString((juce::juce_wchar)('A' + i)), 1298, 588 + i * 20.0f, 14, 10.0f, col::dim);
            L(juce::String(i + 1), 1316 + i * 15.5f, 748, 14, 10.0f, col::dim);
        }
        auto *grid = add<ArpGrid>(*this);
        grid->setBounds(1308, 588, 124, 160);
    }

    /* ============================================================ PHRASE RECORDER */
    S(1454, Z0, 1666, Z1, "PHRASE REC", Colour(0xffd9d9d9));
    {
        auto blink = [] { return (juce::Time::getMillisecondCounter() / 250) % 2 == 0; };
        L("PLAY/STOP", 1490, 548, 70, 10.0f);
        B(1462, 562, 56, 26, [&pr, blink] {
            int s = pr.phraseState.load();
            if (s == PH_PLAYING) return col::green;
            if (s == PH_RECORDING) return col::green.withMultipliedBrightness(0.7f);
            if (s == PH_COUNTIN) return blink() ? col::green : Colours::transparentBlack;
            return Colours::transparentBlack;
        }, [&pr](const juce::MouseEvent &) { pr.command(GalaProcessor::C_PH_PLAY); });
        L("REC", 1552, 548, 50, 10.0f);
        B(1524, 562, 56, 26, [&pr, blink] {
            int s = pr.phraseState.load();
            if (s == PH_RECORDING) return pr.rehearsing.load() ? (blink() ? col::red : Colours::transparentBlack) : col::red;
            if (s == PH_STANDBY || s == PH_COUNTIN) return blink() ? col::red : Colours::transparentBlack;
            return Colours::transparentBlack;
        }, [&pr](const juce::MouseEvent &) { pr.command(GalaProcessor::C_PH_REC); });
        L("ERASE", 1614, 548, 50, 10.0f);
        auto *er = B(1586, 562, 56, 26, [] { return Colours::transparentBlack; }, {});
        er->momentary = [&pr](bool d) { pr.command(GalaProcessor::C_PH_ERASE, d ? 1 : 0); };
        for (int k = 0; k < 8; k++) {
            auto *b = B(1462 + (k % 4) * 46.0f, 598 + (k / 4) * 32.0f, 44, 28, [&pr, k] {
                return pr.phraseSel.load() == k ? col::green : Colours::transparentBlack;
            }, [&pr, k](const juce::MouseEvent &) { pr.command(GalaProcessor::C_PH_SELECT, k); });
            b->dynText = [&pr, k] { return juce::String(k + 1) + (pr.phraseEvents[k].load() > 0 ? "*" : ""); };
        }
        L("LENGTH", 1500, 664, 80, 10.0f);
        B(1462, 680, 30, 24, [] { return Colours::transparentBlack; }, [&pr](const juce::MouseEvent &) {
            pr.command(GalaProcessor::C_PH_LENGTH, juce::jmax(1, pr.phraseBars[pr.phraseSel.load() & 7].load() - 1));
        }, "-");
        auto *len = add<TextBox>([&pr] { return juce::String(pr.phraseBars[pr.phraseSel.load() & 7].load()) + " BAR"; });
        len->setBounds(1494, 681, 52, 22);
        len->size = 12.0f;
        B(1548, 680, 30, 24, [] { return Colours::transparentBlack; }, [&pr](const juce::MouseEvent &) {
            pr.command(GalaProcessor::C_PH_LENGTH, juce::jmin(8, pr.phraseBars[pr.phraseSel.load() & 7].load() + 1));
        }, "+");
        L("RECORD", 1622, 664, 70, 10.0f);
        auto *ty = B(1586, 680, 72, 24, [] { return Colours::transparentBlack; }, [&pr](const juce::MouseEvent &) {
            int t = (pr.phraseTypeUi.load() + 1) % 4;
            pr.phraseTypeUi = t;
            pr.command(GalaProcessor::C_PH_TYPE, t);
        });
        ty->dynText = [&pr] { static const char *n[4] = {"ALL", "BENDER", "KNOBS", "NOTES"}; return juce::String(n[pr.phraseTypeUi.load() & 3]); };
        B(1462, 718, 56, 24, [] { return Colours::transparentBlack; }, [&pr](const juce::MouseEvent &) { pr.command(GalaProcessor::C_PH_CLEAR); }, "CLEAR");
        auto *st = add<TextBox>([&pr] {
            int s = pr.phraseState.load();
            float pos = pr.phrasePos.load();
            static const char *n[5] = {"STOP", "STANDBY", "COUNT-IN", "REC", "PLAY"};
            juce::String t = n[juce::jlimit(0, 4, s)];
            if (s == PH_RECORDING || s == PH_PLAYING)
                t << " " << (int)(pos / 4.0f) + 1 << "." << (int)std::fmod(pos, 4.0f) + 1;
            return t;
        });
        st->setBounds(1522, 719, 76, 22);
        st->size = 12.0f;
        K(1642, 736, 30, Bind::G("metronome"), "");
        L("CLICK", 1612, 729, 30, 9.5f, col::dim);
    }

    /* ============================================================ the bottom: levers, keys, output */
    {
        L("BENDER", 109, 790, 100, 11.0f, col::dim);
        add<Lever>(*this, true)->setBounds(42, 808, 134, 50);
        L("MODULATION", 109, 870, 100, 11.0f, col::dim);
        add<Lever>(*this, false)->setBounds(42, 888, 134, 50);
        auto *k = add<Keys>(proc.keyboardState);
        k->setBounds(196, 790, 1248, 190);
        k->setKeyWidth(1248.0f / 36.0f);
        S(1454, 784, 1666, 980, "OUTPUT", Colour(0xffd9d9d9));
        K(1504, 860, 54, Bind::C(C_PATCH_LEVEL), "PATCH LEVEL");
        K(1610, 860, 68, Bind::G("volume"), "VOLUME");
    }
}

/* ------------------------------------------------------------------------------- the background --- */
void Panel::paint(juce::Graphics &g)
{
    float sc = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (background.isNull() || sc != backgroundScale) {
        backgroundScale = sc;
        int w = (int)std::ceil(W * sc), h = (int)std::ceil(H * sc);
        background = juce::Image(juce::Image::ARGB, w, h, true);
        {
            juce::Graphics bg(background);
            bg.addTransform(juce::AffineTransform::scale(sc));
            auto all = juce::Rectangle<float>(0, 0, (float)W, (float)H);
            drawBody(bg, all);
            for (auto &s : sections)
                drawSection(bg, s.first, s.second.first, s.second.second);
            /* the name: GALA in the panel's own type, SH-1T, SYNTHESIZER */
            bg.setColour(col::legend.withAlpha(0.92f));
            bg.setFont(font(Face::logo, 56.0f));
            bg.drawText("GALA", juce::Rectangle<float>(46, 30, 190, 64), juce::Justification::centredLeft, false);
            bg.setColour(col::amber);
            bg.setFont(font(Face::logo, 34.0f));
            bg.drawText("SH-1T", juce::Rectangle<float>(214, 44, 140, 44), juce::Justification::centredLeft, false);
            bg.setColour(col::dim);
            bg.setFont(font(Face::title, 13.0f));
            bg.drawText("SYNTHESIZER   -   TRANCE GATE", juce::Rectangle<float>(50, 92, 300, 18), juce::Justification::centredLeft, false);
            std::vector<juce::Rectangle<float>> worn;
            for (auto &l : legends) {
                drawLegend(bg, l.text, l.r, l.size, l.c);
                worn.push_back(l.r);
            }
            /* (the most-used: the gate's, the cutoff's, the arp's; the rest rubbed off a little at random) */
            std::vector<juce::Rectangle<float>> rubbed;
            juce::Random rnd(42);
            for (auto &w : worn)
                if (rnd.nextFloat() < 0.5f)
                    rubbed.push_back(w);
            drawAgingUnder(bg, all.reduced(4.0f), rubbed);
            int seed = 3;
            for (auto p : {juce::Point<float>(30, 30), juce::Point<float>(W - 30.0f, 30), juce::Point<float>(30, H - 30.0f),
                           juce::Point<float>(W - 30.0f, H - 30.0f), juce::Point<float>(W * 0.5f, 26), juce::Point<float>(W * 0.5f, H - 26.0f)})
                drawScrew(bg, p, 7.5f, seed++);
            drawTag(bg, juce::Rectangle<float>(1470, 922, 180, 44));
        }
        dust = juce::Image(juce::Image::ARGB, w, h, true);
        {
            juce::Graphics dg(dust);
            dg.addTransform(juce::AffineTransform::scale(sc));
            drawAgingOver(dg, juce::Rectangle<float>(0, 0, (float)W, (float)H).reduced(6.0f));
        }
    }
    g.drawImage(background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
}

void Panel::paintOverChildren(juce::Graphics &g)
{
    if (!dust.isNull())
        g.drawImage(dust, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
}

/* ============================================================================== the editor === */
GalaEditor::GalaEditor(GalaProcessor &p) : AudioProcessorEditor(&p), proc(p), panel(p)
{
    addAndMakeVisible(panel);
    panel.setBounds(0, 0, GalaPanel::W, GalaPanel::H);
    const double W = GalaPanel::W, H = GalaPanel::H;
    auto area = juce::Rectangle<int>(0, 0, 1920, 1080);
    if (auto *d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        area = d->userArea;
    double fit = juce::jmin((area.getWidth() - 40) / W, (area.getHeight() - 90) / H);
    fit = juce::jlimit(0.45, 1.6, fit);
    setResizable(true, true);
    setResizeLimits((int)(W * 0.45), (int)(H * 0.45), (int)(W * juce::jmax(0.5, fit)), (int)(H * juce::jmax(0.5, fit)));
    getConstrainer()->setFixedAspectRatio(W / H);
    double s = proc.uiScaleSet ? (double)proc.uiScale.load() : juce::jmin(fit, 1.0);
    s = juce::jlimit(0.45, fit, s);
    setSize((int)std::lround(W * s), (int)std::lround(H * s));
}

void GalaEditor::resized()
{
    float s = (float)getWidth() / (float)GalaPanel::W;
    panel.setTransform(juce::AffineTransform::scale(s));
    proc.uiScale = s;
    proc.uiScaleSet = true;
}
