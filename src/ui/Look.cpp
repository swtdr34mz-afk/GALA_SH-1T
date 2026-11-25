/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (GALA SH-1T) */
#include "Look.h"
#include "BinaryData.h"

namespace glui
{
/* the fonts and the tag: plain pointers, made when first drawn and freed when the last editor closes. Never function
 * statics: those are freed while the host unloads the DLL, under Windows' loader lock (PROPHET 67's lesson) */
struct Store
{
    juce::Typeface::Ptr t[4];
    juce::Image tag;
};
static Store *store = nullptr;
static Store &get()
{
    if (store == nullptr)
    {
        store = new Store();
        store->t[0] = juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansSemiBold_ttf, BinaryData::FiraSansSemiBold_ttfSize);
        store->t[1] = juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansBold_ttf, BinaryData::FiraSansBold_ttfSize);
        store->t[2] = juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansExtraBold_ttf, BinaryData::FiraSansExtraBold_ttfSize);
        store->t[3] = juce::Typeface::createSystemTypefaceFor(BinaryData::AnonymousProBold_ttf, BinaryData::AnonymousProBold_ttfSize);
        store->tag = juce::ImageFileFormat::loadFrom(BinaryData::swtdr34mztag_png, BinaryData::swtdr34mztag_pngSize);
    }
    return *store;
}
UiResources::~UiResources()
{
    delete store;
    store = nullptr;
    juce::Typeface::clearTypefaceCache();
    juce::ImageCache::releaseUnusedImages();
}

juce::Font font(Face f, float height) { return juce::Font(juce::FontOptions(get().t[(int)f]).withHeight(height)); }

Look::Look()
{
    using namespace juce;
    setColour(ResizableWindow::backgroundColourId, col::panel);
    setColour(PopupMenu::backgroundColourId, Colour(0xff202226));
    setColour(PopupMenu::textColourId, col::legend);
    setColour(PopupMenu::highlightedBackgroundColourId, col::amber.darker(0.3f));
    setColour(PopupMenu::highlightedTextColourId, Colours::black);
    setColour(PopupMenu::headerTextColourId, col::amber);
    setColour(AlertWindow::backgroundColourId, Colour(0xff202226));
    setColour(AlertWindow::textColourId, col::legend);
    setColour(TextEditor::backgroundColourId, Colour(0xff0b0c0e));
    setColour(TextEditor::textColourId, col::legend);
    setColour(TextButton::buttonColourId, Colour(0xff3a3d42));
}

juce::Typeface::Ptr Look::getTypefaceForFont(const juce::Font &) { return get().t[0]; }

void Look::drawPopupMenuBackground(juce::Graphics &g, int w, int h)
{
    g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    g.setColour(col::amber.withAlpha(0.6f));
    g.drawRect(0, 0, w, h, 1);
}

/* ------------------------------------------------------------------------------- the body --- */
void drawBody(juce::Graphics &g, juce::Rectangle<float> r)
{
    using namespace juce;
    /* the white body: a soft sheen, rounded */
    /* (once white: years of light and cellar air have made it ivory, darker at the edges) */
    g.setGradientFill(ColourGradient(Colour(0xffd9cba0), r.getX(), r.getY(), Colour(0xffa89366), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 26.0f);
    g.setGradientFill(ColourGradient(Colours::transparentBlack, r.getCentreX(), r.getCentreY(), Colour(0x80604a1c), r.getX(), r.getY(), true));
    g.fillRoundedRectangle(r, 26.0f);
    g.setColour(Colour(0xffa89c78));
    g.drawRoundedRectangle(r.reduced(1.0f), 26.0f, 2.0f);
    /* the charcoal panel, with a fine brushed grain */
    auto p = r.reduced(16.0f);
    g.setGradientFill(ColourGradient(Colour(0xff303338), p.getX(), p.getY(), Colour(0xff222428), p.getX(), p.getBottom(), false));
    g.fillRoundedRectangle(p, 14.0f);
    Random rnd(7);
    for (int i = 0; i < 900; i++) {
        float y = p.getY() + rnd.nextFloat() * p.getHeight();
        float x = p.getX() + rnd.nextFloat() * p.getWidth();
        g.setColour(Colours::white.withAlpha(0.012f + 0.02f * rnd.nextFloat()));
        g.drawHorizontalLine((int)y, x, jmin(p.getRight(), x + 40.0f + 160.0f * rnd.nextFloat()));
    }
    g.setColour(Colours::black.withAlpha(0.5f));
    g.drawRoundedRectangle(p, 14.0f, 2.0f);
}

void drawSection(juce::Graphics &g, juce::Rectangle<float> r, const juce::String &title, juce::Colour accent)
{
    using namespace juce;
    g.setColour(col::section);
    g.fillRoundedRectangle(r, 9.0f);
    g.setColour(Colours::black.withAlpha(0.35f));
    g.drawRoundedRectangle(r.translated(0.0f, 1.5f), 9.0f, 1.5f);
    g.setColour(accent);
    g.drawRoundedRectangle(r, 9.0f, 2.2f);
    if (title.isNotEmpty()) {
        /* the title in its tab, cut at a slant as the GAIA's */
        auto f = font(Face::title, 18.0f);
        float w = GlyphArrangement::getStringWidth(f, title) + 26.0f;
        Path tab;
        float x = r.getX(), y = r.getY();
        tab.startNewSubPath(x + 9.0f, y);
        tab.lineTo(x + w + 12.0f, y);
        tab.lineTo(x + w, y + 24.0f);
        tab.lineTo(x, y + 24.0f);
        tab.lineTo(x, y + 9.0f);
        tab.quadraticTo(x, y, x + 9.0f, y);
        tab.closeSubPath();
        g.setColour(col::panelDark);
        g.fillPath(tab);
        g.setColour(accent);
        g.strokePath(tab, PathStrokeType(2.0f));
        g.setFont(f);
        g.drawText(title, Rectangle<float>(x + 10.0f, y + 1.0f, w, 23.0f), Justification::centredLeft, false);
    }
}

void drawLegend(juce::Graphics &g, const juce::String &t, juce::Rectangle<float> r, float size, juce::Colour c, juce::Justification j)
{
    g.setColour(c);
    g.setFont(font(Face::title, size));
    g.drawFittedText(t, r.toNearestInt(), j, 2, 0.8f);
}

/* ------------------------------------------------------------------------------- the knob --- */
void drawKnob(juce::Graphics &g, juce::Rectangle<float> r, float pos, bool bipolar)
{
    using namespace juce;
    const float d = jmin(r.getWidth(), r.getHeight()), R = d * 0.5f;
    const Point<float> c = r.getCentre();
    const float a0 = -2.42f, a1 = 2.42f, ang = a0 + pos * (a1 - a0);
    /* the scale: white ticks */
    g.setColour(col::legend.withAlpha(0.85f));
    for (int i = 0; i <= 10; i++) {
        float a = a0 + (a1 - a0) * (float)i / 10.0f;
        bool big = i == 0 || i == 10 || (bipolar && i == 5);
        g.drawLine(Line<float>(c.getPointOnCircumference(R * 0.83f, a), c.getPointOnCircumference(R * (big ? 1.0f : 0.94f), a)),
                   jmax(1.0f, d * (big ? 0.035f : 0.022f)));
    }
    const float rb = R * 0.72f;
    g.setGradientFill(ColourGradient(Colours::black.withAlpha(0.7f), c.x, c.y + rb * 0.3f, Colours::transparentBlack, c.x, c.y + rb * 1.5f, true));
    g.fillEllipse(Rectangle<float>(rb * 2.6f, rb * 2.6f).withCentre(c.translated(0.0f, rb * 0.25f)));
    /* the black skirt with its grip, the top a little lighter */
    auto body = Rectangle<float>(rb * 2.0f, rb * 2.0f).withCentre(c);
    g.setColour(Colour(0xff0b0c0d));
    g.fillEllipse(body);
    for (int i = 0; i < 28; i++) {
        float a = MathConstants<float>::twoPi * (float)i / 28.0f;
        float lit = 0.5f + 0.5f * std::cos(a + 0.8f);
        g.setColour(Colours::white.withAlpha(0.04f + 0.09f * lit));
        g.drawLine(Line<float>(c.getPointOnCircumference(rb * 0.80f, a), c.getPointOnCircumference(rb * 0.98f, a)), jmax(0.8f, d * 0.02f));
    }
    auto top = body.reduced(rb * 0.22f);
    g.setGradientFill(ColourGradient(Colour(0xff3c3f44), top.getX(), top.getY(), Colour(0xff121315), top.getRight(), top.getBottom(), false));
    g.fillEllipse(top);
    g.setColour(Colours::white.withAlpha(0.12f));
    g.drawEllipse(top, 1.0f);
    /* the white line */
    g.setColour(Colours::white);
    g.drawLine(Line<float>(c.getPointOnCircumference(rb * 0.18f, ang), c.getPointOnCircumference(rb * 0.96f, ang)), jmax(1.6f, d * 0.055f));
}

/* ------------------------------------------------------------------------------- the fader --- */
void drawFader(juce::Graphics &g, juce::Rectangle<float> r, float pos, bool bipolar)
{
    using namespace juce;
    const float cx = r.getCentreX(), capH = jmin(26.0f, r.getHeight() * 0.18f), top = r.getY() + capH * 0.5f,
                bot = r.getBottom() - capH * 0.5f;
    /* the scale lines across */
    g.setColour(col::legend.withAlpha(0.55f));
    for (int i = 0; i <= 10; i++) {
        float y = top + (bot - top) * (float)i / 10.0f;
        float w = (i == 0 || i == 10 || (bipolar && i == 5)) ? r.getWidth() * 0.5f : r.getWidth() * 0.36f;
        g.drawLine(cx - w, y, cx - r.getWidth() * 0.14f, y, 1.0f);
        g.drawLine(cx + r.getWidth() * 0.14f, y, cx + w, y, 1.0f);
    }
    /* the slot */
    auto slot = Rectangle<float>(cx - 3.5f, top - 4.0f, 7.0f, bot - top + 8.0f);
    g.setColour(Colour(0xff050506));
    g.fillRoundedRectangle(slot, 3.5f);
    g.setColour(Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(slot, 3.5f, 1.0f);
    /* the cap: black, a white line across */
    float y = bot - (bot - top) * pos;
    auto cap = Rectangle<float>(r.getWidth() * 0.62f, capH).withCentre({cx, y});
    g.setColour(Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(cap.translated(0.0f, 3.0f).expanded(1.0f), 3.0f);
    g.setGradientFill(ColourGradient(Colour(0xff3a3c40), cap.getX(), cap.getY(), Colour(0xff0d0e10), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle(cap, 3.0f);
    g.setColour(Colour(0xff55585e));
    g.drawHorizontalLine((int)(cap.getY() + 2.0f), cap.getX() + 2.0f, cap.getRight() - 2.0f);
    g.setColour(Colours::white);
    g.fillRect(Rectangle<float>(cap.getWidth() - 8.0f, 2.0f).withCentre(cap.getCentre()));
}

/* ------------------------------------------------------------------------------- buttons, LEDs --- */
void drawButton(juce::Graphics &g, juce::Rectangle<float> r, juce::Colour lit, bool down, bool over)
{
    using namespace juce;
    bool on = !lit.isTransparent();
    g.setColour(Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(r.translated(0.0f, down ? 1.0f : 2.5f), 4.0f);
    auto b = r.translated(0.0f, down ? 1.0f : 0.0f);
    if (on) {
        /* lit through the rubber */
        g.setColour(lit.withAlpha(0.35f));
        g.fillRoundedRectangle(b.expanded(3.0f), 6.0f);
        g.setGradientFill(ColourGradient(lit.brighter(0.5f), b.getCentreX(), b.getCentreY(), lit.darker(0.2f), b.getX(), b.getY(), true));
    } else
        g.setGradientFill(ColourGradient(Colour(over ? 0xff9da1a8 : 0xff8f939a), b.getX(), b.getY(), Colour(0xff6c7077), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle(b, 4.0f);
    g.setColour(Colours::white.withAlpha(on ? 0.35f : 0.25f));
    g.drawHorizontalLine((int)b.getY() + 1, b.getX() + 3.0f, b.getRight() - 3.0f);
    g.setColour(Colours::black.withAlpha(0.4f));
    g.drawRoundedRectangle(b, 4.0f, 1.0f);
}

void drawLed(juce::Graphics &g, juce::Point<float> c, float s, juce::Colour colour, float level)
{
    using namespace juce;
    level = jlimit(0.0f, 1.0f, level);
    auto r = Rectangle<float>(s, s).withCentre(c);
    if (level > 0.02f) {
        g.setColour(colour.withAlpha(0.35f * level));
        g.fillEllipse(r.expanded(s * 0.7f));
    }
    g.setColour(Colour(0xff160909).interpolatedWith(colour, 0.15f + 0.85f * level));
    g.fillEllipse(r);
    g.setColour(Colours::white.withAlpha(0.25f + 0.4f * level));
    g.fillEllipse(r.reduced(s * 0.3f).translated(-s * 0.12f, -s * 0.12f));
}

void drawTag(juce::Graphics &g, juce::Rectangle<float> r)
{
    const juce::Image &tag = get().tag;
    if (tag.isNull())
        return;
    auto fit = juce::RectanglePlacement(juce::RectanglePlacement::centred).appliedTo(tag.getBounds().toFloat(), r);
    g.setColour(col::legend);
    g.drawImage(tag, fit, juce::RectanglePlacement::stretchToFit, true);
}

/* ------------------------------------------------------------------------------- the cellar --- */
static void stain(juce::Graphics &g, juce::Point<float> c, float rad, juce::Colour edge, juce::Random &rnd)
{
    using namespace juce;
    /* a dried drop: faint inside, a darker rim, not quite round */
    Path p;
    const int n = 28;
    for (int i = 0; i <= n; i++) {
        float a = MathConstants<float>::twoPi * (float)i / (float)n;
        float rr = rad * (0.85f + 0.25f * rnd.nextFloat());
        auto q = c.getPointOnCircumference(rr, a);
        if (i == 0)
            p.startNewSubPath(q);
        else
            p.lineTo(q);
    }
    p.closeSubPath();
    p = p.createPathWithRoundedCorners(rad * 0.3f);
    g.setColour(edge.withMultipliedAlpha(0.25f));
    g.fillPath(p);
    g.setColour(edge);
    g.strokePath(p, PathStrokeType(jmax(1.0f, rad * 0.06f)));
}

void drawScrew(juce::Graphics &g, juce::Point<float> c, float rr, int seed)
{
    using namespace juce;
    Random rnd(seed);
    auto r = Rectangle<float>(rr * 2.0f, rr * 2.0f).withCentre(c);
    g.setColour(Colour(0x60000000));
    g.fillEllipse(r.translated(0.0f, 1.5f));
    g.setGradientFill(ColourGradient(Colour(0xffb9b4a8), r.getX(), r.getY(), Colour(0xff5d554a), r.getRight(), r.getBottom(), false));
    g.fillEllipse(r);
    /* rust bleeding out from it */
    for (int i = 0; i < 14; i++) {
        float a = rnd.nextFloat() * MathConstants<float>::twoPi, d = rr * (0.2f + 1.3f * rnd.nextFloat());
        g.setColour(Colour(0xff8a4a1c).withAlpha(0.15f + 0.35f * rnd.nextFloat()));
        g.fillEllipse(Rectangle<float>(rr * 0.5f, rr * 0.5f).withCentre(c.getPointOnCircumference(d, a)));
    }
    g.setColour(Colour(0xff3a3430));
    float a = rnd.nextFloat() * MathConstants<float>::pi;
    g.drawLine(Line<float>(c.getPointOnCircumference(rr * 0.75f, a), c.getPointOnCircumference(rr * 0.75f, a + MathConstants<float>::pi)), rr * 0.25f);
}

void drawAgingUnder(juce::Graphics &g, juce::Rectangle<float> r, const std::vector<juce::Rectangle<float>> &worn)
{
    using namespace juce;
    Random rnd(1971);
    /* the legends rubbed off where hands went for years: patches of the panel's own grey over the print */
    for (auto &w : worn) {
        for (int i = 0; i < 3; i++) {
            auto c = Point<float>(w.getX() + rnd.nextFloat() * w.getWidth(), w.getY() + rnd.nextFloat() * w.getHeight());
            float rad = 4.0f + rnd.nextFloat() * 9.0f;
            g.setGradientFill(ColourGradient(col::section.withAlpha(0.92f), c.x, c.y, col::section.withAlpha(0.0f), c.x + rad, c.y, true));
            g.fillEllipse(Rectangle<float>(rad * 2.0f, rad * 2.0f).withCentre(c));
        }
    }
    /* water marks and a coffee ring on the panel, a tide line on the body */
    stain(g, {r.getX() + r.getWidth() * 0.62f, r.getY() + r.getHeight() * 0.47f}, 46.0f, Colour(0x90a0783c), rnd);
    stain(g, {r.getX() + r.getWidth() * 0.08f, r.getY() + r.getHeight() * 0.86f}, 120.0f, Colour(0x70b49a64), rnd);
    stain(g, {r.getX() + r.getWidth() * 0.11f, r.getY() + r.getHeight() * 0.88f}, 80.0f, Colour(0x60b49a64), rnd);
    stain(g, {r.getX() + r.getWidth() * 0.52f, r.getY() + r.getHeight() * 0.10f}, 30.0f, Colour(0x80a0783c), rnd);
    stain(g, {r.getX() + r.getWidth() * 0.23f, r.getY() + r.getHeight() * 0.70f}, 26.0f, Colour(0x80c8b48a), rnd);
    stain(g, {r.getX() + r.getWidth() * 0.86f, r.getY() + r.getHeight() * 0.25f}, 18.0f, Colour(0x70c8b48a), rnd);
    {
        /* the coffee ring: a mug stood here once */
        auto c = Point<float>(r.getX() + r.getWidth() * 0.415f, r.getY() + r.getHeight() * 0.33f);
        g.setColour(Colour(0x9a5a3214));
        g.drawEllipse(Rectangle<float>(78.0f, 74.0f).withCentre(c), 3.5f);
        g.setColour(Colour(0x505a3214));
        g.drawEllipse(Rectangle<float>(84.0f, 80.0f).withCentre(c.translated(2.0f, 1.0f)), 6.0f);
    }
    /* the bottom of the body: where the cellar floor's damp crept up */
    g.setGradientFill(ColourGradient(Colour(0x00705a30), r.getX(), r.getBottom() - 110.0f, Colour(0x90705a30), r.getX(), r.getBottom(), false));
    g.fillRect(r.withTop(r.getBottom() - 110.0f));
    for (int i = 0; i < 40; i++) {
        float x = r.getX() + rnd.nextFloat() * r.getWidth();
        g.setColour(Colour(0xff6a5228).withAlpha(0.08f + 0.12f * rnd.nextFloat()));
        g.fillEllipse(x, r.getBottom() - 6.0f - rnd.nextFloat() * 40.0f, 3.0f + rnd.nextFloat() * 9.0f, 2.0f + rnd.nextFloat() * 6.0f);
    }
}

void drawAgingOver(juce::Graphics &g, juce::Rectangle<float> r)
{
    using namespace juce;
    Random rnd(1983);
    /* grime in the corners and along the edges */
    for (auto c : {r.getTopLeft(), r.getTopRight(), r.getBottomLeft(), r.getBottomRight()}) {
        g.setGradientFill(ColourGradient(Colour(0xa0362a14), c.x, c.y, Colour(0x00362a14), c.x + (c.x < r.getCentreX() ? 330.0f : -330.0f), c.y, true));
        g.fillEllipse(Rectangle<float>(660.0f, 660.0f).withCentre(c));
    }
    /* dust: thousands of specks, a few fluffs, thicker in the lower half (it settled) */
    for (int i = 0; i < 22000; i++) {
        float x = r.getX() + rnd.nextFloat() * r.getWidth();
        float y = r.getY() + std::sqrt(rnd.nextFloat()) * r.getHeight();
        float s = 0.6f + rnd.nextFloat() * rnd.nextFloat() * 2.4f;
        bool light = rnd.nextFloat() < 0.65f;
        g.setColour((light ? Colour(0xffd8cfb8) : Colour(0xff4a3e2a)).withAlpha(0.16f + 0.4f * rnd.nextFloat()));
        g.fillEllipse(x, y, s, s);
    }
    for (int i = 0; i < 80; i++) {
        Path p;
        float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
        p.startNewSubPath(x, y);
        for (int k = 0; k < 5; k++)
            p.lineTo(x += rnd.nextFloat() * 10.0f - 5.0f, y += rnd.nextFloat() * 8.0f - 4.0f);
        g.setColour(Colour(0xffcfc6b0).withAlpha(0.25f + 0.2f * rnd.nextFloat()));
        g.strokePath(p, PathStrokeType(0.7f));
    }
    /* a film of grey over all of it */
    g.setColour(Colour(0x26a89c80));
    g.fillRect(r);
    /* a dusty haze, thicker in patches */
    for (int i = 0; i < 18; i++) {
        auto c = Point<float>(r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight());
        float rad = 80.0f + rnd.nextFloat() * 220.0f;
        g.setGradientFill(ColourGradient(Colour(0x22b8ab8c), c.x, c.y, Colour(0x00b8ab8c), c.x + rad, c.y, true));
        g.fillEllipse(Rectangle<float>(rad * 2.0f, rad * 2.0f).withCentre(c));
    }
    /* scratches: long fine ones, a few deep */
    for (int i = 0; i < 140; i++) {
        float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
        float a = rnd.nextFloat() * MathConstants<float>::pi, len = 15.0f + rnd.nextFloat() * rnd.nextFloat() * 220.0f;
        Path p;
        p.startNewSubPath(x, y);
        p.quadraticTo(x + std::cos(a) * len * 0.5f + rnd.nextFloat() * 8.0f, y + std::sin(a) * len * 0.5f,
                      x + std::cos(a) * len, y + std::sin(a) * len);
        bool deep = rnd.nextFloat() < 0.12f;
        g.setColour(Colours::white.withAlpha(deep ? 0.3f : 0.09f + 0.1f * rnd.nextFloat()));
        g.strokePath(p, PathStrokeType(deep ? 1.4f : 0.6f));
        if (deep) {
            g.setColour(Colours::black.withAlpha(0.25f));
            g.strokePath(p, PathStrokeType(0.6f), AffineTransform::translation(0.6f, 0.8f));
        }
    }
    /* a cobweb in the top right corner, and one low on the left */
    auto web = [&](Point<float> c, float size, float a0, float a1) {
        g.setColour(Colour(0xffe8e2d4).withAlpha(0.45f));
        const int spokes = 7;
        for (int k = 0; k < spokes; k++) {
            float a = a0 + (a1 - a0) * (float)k / (float)(spokes - 1);
            g.drawLine(Line<float>(c, c.getPointOnCircumference(size * (0.8f + 0.2f * rnd.nextFloat()), a)), 0.7f);
        }
        for (float rr = size * 0.15f; rr < size; rr += size * (0.09f + 0.05f * rnd.nextFloat())) {
            Path p;
            for (int k = 0; k < spokes; k++) {
                float a = a0 + (a1 - a0) * (float)k / (float)(spokes - 1);
                auto q = c.getPointOnCircumference(rr * (0.92f + 0.12f * rnd.nextFloat()), a);
                if (k == 0)
                    p.startNewSubPath(q);
                else
                    p.quadraticTo(c.getPointOnCircumference(rr * 0.86f, a - (a1 - a0) / (float)(spokes - 1) * 0.5f), q);
            }
            g.strokePath(p, PathStrokeType(0.6f));
        }
    };
    /* an old strip of masking tape, someone's note on it */
    {
        Graphics::ScopedSaveState st(g);
        auto c = Point<float>(r.getX() + 254.0f, r.getY() + 492.0f);
        g.addTransform(AffineTransform::rotation(-0.06f, c.x, c.y));
        auto tape = Rectangle<float>(124.0f, 26.0f).withCentre(c);
        g.setColour(Colour(0xd8d7c38c));
        g.fillRect(tape);
        g.setColour(Colour(0x40ffffff));
        g.fillRect(tape.withHeight(4.0f));
        g.setColour(Colour(0x60806a3a));
        for (float x = tape.getX(); x < tape.getRight(); x += 5.0f) {
            g.drawLine(x, tape.getY(), x + 2.0f, tape.getY() - 1.5f, 1.0f);
            g.drawLine(x, tape.getBottom(), x + 2.0f, tape.getBottom() + 1.5f, 1.0f);
        }
        g.setColour(Colour(0xd0202a4a));
        g.setFont(font(Face::mono, 14.0f).italicised());
        g.drawText("TRANCE '09 !!", tape, Justification::centred, false);
    }
    web({r.getRight() - 6.0f, r.getY() + 6.0f}, 150.0f, MathConstants<float>::pi, MathConstants<float>::pi * 1.5f);
    web({r.getX() + 6.0f, r.getBottom() - 6.0f}, 90.0f, 0.0f, MathConstants<float>::pi * 0.5f);
}
} // namespace glui
