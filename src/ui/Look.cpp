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
    g.setGradientFill(ColourGradient(Colour(0xffe8dcb8), r.getX(), r.getY(), Colour(0xffcdb98c), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 26.0f);
    g.setGradientFill(ColourGradient(Colours::transparentBlack, r.getCentreX(), r.getCentreY(), Colour(0x50704e1c), r.getX(), r.getY(), true));
    g.fillRoundedRectangle(r, 26.0f);
    g.setColour(Colour(0xffa89c78));
    g.drawRoundedRectangle(r.reduced(1.0f), 26.0f, 2.0f);
    /* the charcoal panel, with a fine brushed grain */
    auto p = r.reduced(16.0f);
    g.setGradientFill(ColourGradient(Colour(0xff303338), p.getX(), p.getY(), Colour(0xff222428), p.getX(), p.getBottom(), false));
    g.fillRoundedRectangle(p, 14.0f);
    Random rnd(7);
    for (int i = 0; i < 1400; i++) {
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
/* matte black plastic, seen from a little in front: the knurled skirt's side, the cap's soft sheen, the white mark
 * painted in a groove. Each its own small faults (seed): the cap a hair off its shaft, the mark a degree off, the
 * printed scale turned a little */
void drawKnob(juce::Graphics &g, juce::Rectangle<float> r, float pos, bool bipolar, int seed)
{
    using namespace juce;
    Random rnd(seed * 7919 + 13);
    const float d = jmin(r.getWidth(), r.getHeight()), R = d * 0.5f;
    const Point<float> c = r.getCentre();
    const float skew = (rnd.nextFloat() - 0.5f) * 0.03f;                 /* the print */
    const float a0 = -2.42f + skew, a1 = 2.42f + skew, ang = -2.42f + pos * 4.84f + (rnd.nextFloat() - 0.5f) * 0.035f;
    /* the scale, printed on the panel */
    g.setColour(col::legend.withAlpha(0.82f));
    for (int i = 0; i <= 10; i++) {
        float a = a0 + (a1 - a0) * (float)i / 10.0f;
        bool big = i == 0 || i == 10 || (bipolar && i == 5);
        g.drawLine(Line<float>(c.getPointOnCircumference(R * 0.84f, a), c.getPointOnCircumference(R * (big ? 1.0f : 0.95f), a)),
                   jmax(1.0f, d * (big ? 0.034f : 0.022f)));
    }
    const float rb = R * 0.72f;
    const Point<float> off((rnd.nextFloat() - 0.5f) * 1.1f, (rnd.nextFloat() - 0.5f) * 1.1f);   /* (not quite on its shaft) */
    const Point<float> base = c + off + Point<float>(0.0f, rb * 0.06f), top = c + off - Point<float>(0.0f, rb * 0.05f);
    /* its shadow on the panel, soft, down and right */
    for (int k = 3; k >= 1; k--) {
        g.setColour(Colours::black.withAlpha(0.12f));
        g.fillEllipse(Rectangle<float>(rb * 2.0f + (float)k * 3.0f, rb * 2.0f + (float)k * 3.0f).withCentre(base.translated(rb * 0.10f, rb * 0.16f)));
    }
    /* the skirt: its side (seen below the top), then its knurled top edge */
    auto skirt = Rectangle<float>(rb * 2.0f, rb * 2.0f).withCentre(base);
    g.setGradientFill(ColourGradient(Colour(0xff1d1f22), skirt.getX(), skirt.getY(), Colour(0xff040405), skirt.getRight(), skirt.getBottom(), false));
    g.fillEllipse(skirt);
    auto skirtTop = Rectangle<float>(rb * 2.0f, rb * 2.0f).withCentre(top.translated(0.0f, rb * 0.03f));
    g.setColour(Colour(0xff0e0f11));
    g.fillEllipse(skirtTop);
    const int ribs = 44;
    for (int i = 0; i < ribs; i++) {
        float a = MathConstants<float>::twoPi * (float)i / (float)ribs;
        float lit = 0.5f + 0.5f * std::cos(a + 0.75f);                       /* the light from the top left */
        g.setColour(Colours::white.withAlpha(0.02f + 0.16f * lit * lit));
        g.drawLine(Line<float>(skirtTop.getCentre().getPointOnCircumference(rb * 0.80f, a), skirtTop.getCentre().getPointOnCircumference(rb * 0.985f, a)),
                   jmax(0.7f, d * 0.016f));
        g.setColour(Colours::black.withAlpha(0.5f));
        g.drawLine(Line<float>(skirtTop.getCentre().getPointOnCircumference(rb * 0.80f, a + 0.07f), skirtTop.getCentre().getPointOnCircumference(rb * 0.985f, a + 0.07f)),
                   jmax(0.5f, d * 0.01f));
    }
    g.setColour(Colours::white.withAlpha(0.10f));
    g.drawEllipse(skirtTop.reduced(0.5f), 0.8f);
    /* the cap: matte, a soft sheen at the top left, the bevel catching the light */
    auto cap = Rectangle<float>(rb * 1.52f, rb * 1.52f).withCentre(top);
    g.setColour(Colours::black.withAlpha(0.6f));
    g.fillEllipse(cap.expanded(1.2f).translated(0.0f, 0.8f));
    float shade = 0.92f + 0.16f * rnd.nextFloat();                         /* (each cap from another batch) */
    g.setGradientFill(ColourGradient(Colour(0xff35383c).withMultipliedBrightness(shade), cap.getX(), cap.getY(), Colour(0xff0f1012), cap.getRight(), cap.getBottom(), false));
    g.fillEllipse(cap);
    ColourGradient sheen(Colours::white.withAlpha(0.16f), cap.getX() + cap.getWidth() * 0.32f, cap.getY() + cap.getHeight() * 0.28f,
                         Colours::white.withAlpha(0.0f), cap.getX() + cap.getWidth() * 0.32f + cap.getWidth() * 0.45f, cap.getY() + cap.getHeight() * 0.28f, true);
    g.setGradientFill(sheen);
    g.fillEllipse(cap);
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.28f), cap.getX(), cap.getY(), Colours::white.withAlpha(0.0f), cap.getCentreX(), cap.getCentreY(), false));
    g.drawEllipse(cap.reduced(0.6f), 1.1f);
    /* the mark: a groove, white paint in it, a little short of the edge; sometimes worn at its tip */
    auto p0 = top.getPointOnCircumference(rb * 0.16f, ang), p1 = top.getPointOnCircumference(rb * 0.92f, ang);
    float w = jmax(1.5f, d * (0.050f + 0.012f * rnd.nextFloat()));
    g.setColour(Colours::black.withAlpha(0.85f));
    g.drawLine(Line<float>(p0, p1), w + 1.4f);
    g.setColour(Colour(0xfff4f1e8));
    auto tip = top.getPointOnCircumference(rb * (0.86f - 0.08f * rnd.nextFloat()), ang);
    g.drawLine(Line<float>(top.getPointOnCircumference(rb * 0.2f, ang), tip), w);
    if (rnd.nextFloat() < 0.35f) {                                         /* the paint chipped near the tip */
        g.setColour(Colour(0xff1a1b1d));
        g.fillEllipse(Rectangle<float>(w * 0.9f, w * 0.9f).withCentre(top.getPointOnCircumference(rb * (0.55f + 0.25f * rnd.nextFloat()), ang)));
    }
}

/* ------------------------------------------------------------------------------- the fader --- */
void drawFader(juce::Graphics &g, juce::Rectangle<float> r, float pos, bool bipolar, int seed)
{
    using namespace juce;
    Random rnd(seed * 104729 + 7);
    const float cx = r.getCentreX(), capH = jmin(26.0f, r.getHeight() * 0.18f), top = r.getY() + capH * 0.5f,
                bot = r.getBottom() - capH * 0.5f;
    /* the scale, printed a hair off */
    const float printDy = (rnd.nextFloat() - 0.5f) * 1.4f, printDx = (rnd.nextFloat() - 0.5f) * 0.8f;
    g.setColour(col::legend.withAlpha(0.55f));
    for (int i = 0; i <= 10; i++) {
        float y = top + (bot - top) * (float)i / 10.0f + printDy;
        float w = (i == 0 || i == 10 || (bipolar && i == 5)) ? r.getWidth() * 0.5f : r.getWidth() * 0.36f;
        g.drawLine(cx - w + printDx, y, cx - r.getWidth() * 0.15f + printDx, y, 1.0f);
        g.drawLine(cx + r.getWidth() * 0.15f + printDx, y, cx + w + printDx, y, 1.0f);
    }
    /* the slot: a cut in the panel, its far wall lit, the dark below */
    auto slot = Rectangle<float>(cx - 3.2f, top - 5.0f, 6.4f, bot - top + 10.0f);
    g.setColour(Colour(0xff020203));
    g.fillRoundedRectangle(slot, 3.0f);
    g.setGradientFill(ColourGradient(Colours::black, slot.getX(), slot.getY(), Colour(0xff2a2c30), slot.getRight(), slot.getY(), false));
    g.fillRoundedRectangle(slot.withTrimmedLeft(slot.getWidth() * 0.55f).reduced(0.0f, 2.0f), 1.5f);
    g.setColour(Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(slot.translated(0.0f, 0.6f), 3.0f, 0.8f);

    /* the cap: its own faults (seed) */
    const float dx = (rnd.nextFloat() - 0.5f) * 2.4f, tilt = (rnd.nextFloat() - 0.5f) * 0.028f, wv = (rnd.nextFloat() - 0.5f) * 1.2f;
    float y = bot - (bot - top) * pos;
    float cw = r.getWidth() * 0.66f + wv;
    auto capR = Rectangle<float>(cw, capH).withCentre({cx + dx, y});
    Graphics::ScopedSaveState st(g);
    g.addTransform(AffineTransform::rotation(tilt, capR.getCentreX(), capR.getCentreY()));
    /* its shadow on the panel */
    g.setColour(Colours::black.withAlpha(0.35f));
    g.fillRoundedRectangle(capR.translated(1.5f, 4.0f).expanded(1.5f), 4.0f);
    g.setColour(Colours::black.withAlpha(0.25f));
    g.fillRoundedRectangle(capR.translated(2.5f, 7.0f).expanded(2.5f), 5.0f);
    /* the body: the front face below, darker; the top face with its two slopes meeting at the ridge */
    float shade = 0.9f + 0.2f * rnd.nextFloat();
    auto front = capR.withTrimmedTop(capR.getHeight() * 0.78f);
    g.setColour(Colour(0xff08090a));
    g.fillRoundedRectangle(capR, 3.5f);
    auto face = capR.withTrimmedBottom(capR.getHeight() * 0.18f);
    g.setGradientFill(ColourGradient(Colour(0xff3d4044).withMultipliedBrightness(shade), face.getX(), face.getY(),
                                     Colour(0xff141518).withMultipliedBrightness(shade), face.getX(), face.getBottom(), false));
    g.fillRoundedRectangle(face, 3.0f);
    /* the ridges you'd feel under a fingertip */
    for (int k = 1; k <= 4; k++) {
        float yy = face.getY() + face.getHeight() * (0.12f * (float)k);
        g.setColour(Colours::white.withAlpha(0.07f));
        g.drawHorizontalLine((int)yy, face.getX() + 2.5f, face.getRight() - 2.5f);
        g.setColour(Colours::black.withAlpha(0.35f));
        g.drawHorizontalLine((int)yy + 1, face.getX() + 2.5f, face.getRight() - 2.5f);
    }
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.22f), face.getX(), face.getY(), Colours::white.withAlpha(0.0f), face.getRight(), face.getY(), false));
    g.drawRoundedRectangle(face.reduced(0.5f), 3.0f, 0.9f);
    g.setColour(Colours::white.withAlpha(0.05f));
    g.fillRect(front.reduced(2.0f, 0.0f).withHeight(1.0f));
    /* the white line: painted by a machine that was not quite set right */
    float ly = face.getCentreY() + capH * 0.08f + (rnd.nextFloat() - 0.5f) * 2.6f;
    float len = face.getWidth() * (0.62f + 0.22f * rnd.nextFloat()), lt = (rnd.nextFloat() - 0.5f) * 0.05f;
    float lx = face.getCentreX() + (rnd.nextFloat() - 0.5f) * 1.6f;
    float th = 1.6f + 0.9f * rnd.nextFloat();
    Path line;
    line.addRectangle(-len * 0.5f, -th * 0.5f, len, th);
    line.applyTransform(AffineTransform::rotation(lt).translated(lx, ly));
    g.setColour(Colours::black.withAlpha(0.5f));
    g.fillPath(line, AffineTransform::translation(0.0f, 0.7f));
    g.setColour(Colour(0xfff2efe6).withAlpha(0.82f + 0.18f * rnd.nextFloat()));
    g.fillPath(line);
    if (rnd.nextFloat() < 0.22f) {                                         /* worn thin in a spot */
        float wx = lx + (rnd.nextFloat() - 0.5f) * len * 0.8f;
        g.setColour(Colour(0xff2a2c30).withAlpha(0.55f));
        g.fillRect(Rectangle<float>(1.0f + rnd.nextFloat() * 1.5f, th + 0.4f).withCentre({wx, ly}));
    }
}

/* ------------------------------------------------------------------------------- buttons, LEDs --- */
void drawButton(juce::Graphics &g, juce::Rectangle<float> r0, juce::Colour lit, bool down, bool over, int seed)
{
    using namespace juce;
    Random rnd(seed * 31337 + 3);
    /* (pressed into its hole a little off) */
    auto r = r0.translated((rnd.nextFloat() - 0.5f) * 1.2f, (rnd.nextFloat() - 0.5f) * 1.0f);
    bool on = !lit.isTransparent();
    g.setColour(Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(r.expanded(1.5f).translated(0.0f, 1.0f), 5.0f);             /* the hole's edge */
    g.setColour(Colours::black.withAlpha(0.35f));
    g.fillRoundedRectangle(r.translated(0.8f, down ? 1.0f : 3.0f), 4.0f);
    auto b = r.translated(0.0f, down ? 1.2f : 0.0f);
    if (on) {
        g.setColour(lit.withAlpha(0.28f));
        g.fillRoundedRectangle(b.expanded(4.0f), 7.0f);
        g.setGradientFill(ColourGradient(lit.brighter(0.45f), b.getCentreX(), b.getCentreY(), lit.darker(0.35f), b.getX(), b.getY(), true));
    } else {
        float sh = 0.94f + 0.1f * rnd.nextFloat();
        g.setGradientFill(ColourGradient(Colour(over ? 0xffa3a6ab : 0xff969a9f).withMultipliedBrightness(sh), b.getX(), b.getY(),
                                         Colour(0xff656970).withMultipliedBrightness(sh), b.getX(), b.getBottom(), false));
    }
    g.fillRoundedRectangle(b, 4.0f);
    /* rubber: a faint dish in the middle, a soft top edge */
    g.setGradientFill(ColourGradient(Colours::black.withAlpha(0.10f), b.getCentreX(), b.getCentreY(), Colours::transparentBlack, b.getRight(), b.getCentreY(), true));
    g.fillRoundedRectangle(b.reduced(2.0f), 3.0f);
    g.setColour(Colours::white.withAlpha(on ? 0.35f : 0.28f));
    g.drawHorizontalLine((int)b.getY() + 1, b.getX() + 3.0f, b.getRight() - 3.0f);
    g.setColour(Colours::black.withAlpha(0.45f));
    g.drawRoundedRectangle(b, 4.0f, 1.0f);
}

/* ------------------------------------------------------------------------------- the surface --- */
/* a tile of relief, seamless: periodic value noise in a few octaves, its slope lit from the top left; white where
 * a bump faces the light, black where it turns away. Made once per kind and pixel density */
static juce::Image surfaceTile(int kind, float sc)
{
    using namespace juce;
    const int N = 256;
    Image img(Image::ARGB, N, N, true);
    std::vector<float> h((size_t)(N * N), 0.0f);
    Random rnd(kind ? 4242 : 1717);
    struct Oct { float period, amp; };
    std::vector<Oct> octs = kind == 0 ? std::vector<Oct>{{1.1f, 0.40f}, {2.2f, 0.40f}, {4.5f, 0.12f}}
                                      : std::vector<Oct>{{5.0f, 0.35f}, {11.0f, 0.45f}, {30.0f, 0.2f}};
    for (auto o : octs) {
        int L = jmax(2, (int)std::lround((float)N / jmax(1.0f, o.period * sc)));
        std::vector<float> lat((size_t)(L * L));
        for (auto &v : lat)
            v = rnd.nextFloat();
        float cell = (float)N / (float)L;
        for (int y = 0; y < N; y++)
            for (int x = 0; x < N; x++) {
                float fx = (float)x / cell, fy = (float)y / cell;
                int x0 = (int)fx, y0 = (int)fy;
                float tx = fx - (float)x0, ty = fy - (float)y0;
                tx = tx * tx * (3.0f - 2.0f * tx);
                ty = ty * ty * (3.0f - 2.0f * ty);
                auto at = [&](int i, int j) { return lat[(size_t)(((j % L + L) % L) * L + ((i % L + L) % L))]; };
                float v = (at(x0, y0) * (1 - tx) + at(x0 + 1, y0) * tx) * (1 - ty) + (at(x0, y0 + 1) * (1 - tx) + at(x0 + 1, y0 + 1) * tx) * ty;
                h[(size_t)(y * N + x)] += v * o.amp;
            }
    }
    Image::BitmapData bd(img, Image::BitmapData::writeOnly);
    const float k = kind == 0 ? 1.9f : 2.6f;
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            auto H = [&](int i, int j) { return h[(size_t)(((j + N) % N) * N + ((i + N) % N))]; };
            float nx = H(x - 1, y) - H(x + 1, y), ny = H(x, y - 1) - H(x, y + 1);
            float s = (nx * 0.7f + ny * 0.7f) * k + (rnd.nextFloat() - 0.5f) * (kind == 0 ? 0.10f : 0.03f);
            s = jlimit(-1.0f, 1.0f, s);
            uint8 a = (uint8)jlimit(0, 255, (int)std::lround(std::fabs(s) * 255.0f));
            bd.setPixelColour(x, y, s > 0 ? Colour::fromRGBA(255, 255, 255, a) : Colour::fromRGBA(0, 0, 0, a));
        }
    return img;
}

void drawSurface(juce::Graphics &g, const juce::Path &region, int kind, float sc, float strength)
{
    using namespace juce;
    juce::Image tile = surfaceTile(kind, sc);
    Graphics::ScopedSaveState st(g);
    g.reduceClipRegion(region);
    g.setTiledImageFill(tile, 0, 0, strength);
    g.fillPath(region);
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

/* ------------------------------------------------------------------------------- the relic --- */
/* Aged as a custom shop relics a guitar: by hand, where the years would really have gone. The white lacquer toned to
 * ivory and checked in fine cracks; worn through to the grey primer where the forearms and palms rest; the panel's
 * paint polished thin to bare aluminium in front of the keys; the print rubbed where the fingers go; the screws dulled.
 * No dirt for its own sake. */
static void checking(juce::Graphics &g, juce::Rectangle<float> area, int n, juce::Random &rnd, juce::Colour c)
{
    using namespace juce;
    /* lacquer checks: fine, mostly across, each a little wavy, in clusters */
    for (int i = 0; i < n; i++) {
        float x = area.getX() + rnd.nextFloat() * area.getWidth(), y = area.getY() + rnd.nextFloat() * area.getHeight();
        float len = 10.0f + rnd.nextFloat() * 46.0f, tilt = (rnd.nextFloat() - 0.5f) * 0.35f;
        Path p;
        p.startNewSubPath(x, y);
        int segs = 4;
        for (int k = 1; k <= segs; k++)
            p.lineTo(x + len * (float)k / (float)segs, y + tilt * len * (float)k / (float)segs + (rnd.nextFloat() - 0.5f) * 1.6f);
        g.setColour(c.withMultipliedAlpha(0.5f + 0.5f * rnd.nextFloat()));
        g.strokePath(p, PathStrokeType(0.55f));
        g.setColour(Colours::white.withAlpha(0.25f));
        g.strokePath(p, PathStrokeType(0.5f), AffineTransform::translation(0.0f, 0.7f));
    }
}

static void wearPatch(juce::Graphics &g, juce::Point<float> c, float rx, float ry, juce::Colour bare, juce::Random &rnd)
{
    using namespace juce;
    /* the finish worn through: an irregular soft-edged patch, a thin darker rim where the lacquer ends */
    Path p;
    const int n = 36;
    float ph = rnd.nextFloat() * 6.0f;
    for (int i = 0; i <= n; i++) {
        float a = MathConstants<float>::twoPi * (float)i / (float)n;
        float w = 0.78f + 0.14f * std::sin(a * 3.0f + ph) + 0.1f * rnd.nextFloat();
        Point<float> q(c.x + std::sin(a) * rx * w, c.y - std::cos(a) * ry * w);
        if (i == 0)
            p.startNewSubPath(q);
        else
            p.lineTo(q);
    }
    p.closeSubPath();
    p = p.createPathWithRoundedCorners(6.0f);
    g.setColour(Colours::black.withAlpha(0.18f));
    g.strokePath(p, PathStrokeType(1.6f));
    g.setColour(bare);
    g.fillPath(p);
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.10f), c.x, c.y - ry * 0.4f, Colours::transparentWhite, c.x, c.y + ry, false));
    g.fillPath(p);
}

void drawScrew(juce::Graphics &g, juce::Point<float> c, float rr, int seed)
{
    using namespace juce;
    Random rnd(seed);
    auto r = Rectangle<float>(rr * 2.0f, rr * 2.0f).withCentre(c);
    g.setColour(Colour(0x50000000));
    g.fillEllipse(r.translated(0.0f, 1.2f));
    /* nickel gone dull: grey, a little warm, the head's edge still bright */
    g.setGradientFill(ColourGradient(Colour(0xffc2bdb2), r.getX(), r.getY(), Colour(0xff6e685e), r.getRight(), r.getBottom(), false));
    g.fillEllipse(r);
    g.setColour(Colour(0x30584a30));
    g.fillEllipse(r.reduced(rr * 0.25f));
    g.setColour(Colours::white.withAlpha(0.35f));
    g.drawEllipse(r.reduced(0.6f), 0.8f);
    float a = rnd.nextFloat() * MathConstants<float>::pi;
    g.setColour(Colour(0xff35302a));
    g.drawLine(Line<float>(c.getPointOnCircumference(rr * 0.72f, a), c.getPointOnCircumference(rr * 0.72f, a + MathConstants<float>::pi)), rr * 0.24f);
}

void drawAgingUnder(juce::Graphics &g, juce::Rectangle<float> r, const std::vector<juce::Rectangle<float>> &worn,
                    const std::vector<juce::Rectangle<float>> &frames)
{
    using namespace juce;
    Random rnd(1971);
    const Colour primer(0xff8c8476);
    /* the body: worn through along the front edge where the forearms rest, and at the corners that met cases and
     * stands; checking over the lacquer */
    {
        Graphics::ScopedSaveState st(g);
        Path body, panel;
        body.addRoundedRectangle(r, 24.0f);
        panel.addRoundedRectangle(r.reduced(12.0f), 14.0f);
        body.setUsingNonZeroWinding(false);
        body.addPath(panel);
        g.reduceClipRegion(body);
        for (int i = 0; i < 7; i++)
            wearPatch(g, {r.getX() + r.getWidth() * (0.08f + 0.12f * (float)i + 0.04f * rnd.nextFloat()), r.getBottom() - 2.0f},
                      34.0f + rnd.nextFloat() * 70.0f, 10.0f + rnd.nextFloat() * 6.0f, primer, rnd);
        for (auto c : {r.getTopLeft(), r.getTopRight(), r.getBottomLeft(), r.getBottomRight()})
            wearPatch(g, c.translated(c.x < r.getCentreX() ? 6.0f : -6.0f, c.y < r.getCentreY() ? 6.0f : -6.0f), 22.0f, 18.0f, primer, rnd);
        checking(g, r.withHeight(16.0f), 110, rnd, Colour(0xb05a4a2a));
        checking(g, r.withTop(r.getBottom() - 16.0f), 120, rnd, Colour(0xb05a4a2a));
        checking(g, r.withWidth(16.0f), 30, rnd, Colour(0xb05a4a2a));
        checking(g, r.withLeft(r.getRight() - 16.0f), 30, rnd, Colour(0xb05a4a2a));
    }
    /* the panel: its paint polished thin in front of the keys, where palms rest, and round the most-used controls */
    auto bare = [&](Point<float> c, float rx, float ry, float a) {
        g.setGradientFill(ColourGradient(Colour(0xff6f7276).withAlpha(a), c.x, c.y, Colour(0xff6f7276).withAlpha(0.0f), c.x + rx, c.y, true));
        g.fillEllipse(Rectangle<float>(rx * 2.0f, ry * 2.0f).withCentre(c));
        /* brushed: fine lines across */
        for (int k = 0; k < 40; k++) {
            float y = c.y - ry * 0.6f + rnd.nextFloat() * ry * 1.2f, x0 = c.x - rx * 0.7f + rnd.nextFloat() * rx * 0.4f;
            g.setColour(Colours::white.withAlpha(0.03f + 0.04f * rnd.nextFloat() * a));
            g.drawHorizontalLine((int)y, x0, x0 + rx * (0.4f + 0.8f * rnd.nextFloat()));
        }
    };
    bare({r.getX() + r.getWidth() * 0.30f, r.getY() + r.getHeight() * 0.775f}, 170.0f, 24.0f, 0.8f);
    bare({r.getX() + r.getWidth() * 0.62f, r.getY() + r.getHeight() * 0.777f}, 210.0f, 22.0f, 0.75f);
    bare({r.getX() + r.getWidth() * 0.07f, r.getY() + r.getHeight() * 0.86f}, 70.0f, 70.0f, 0.35f);
    /* the frames' paint chipped here and there, the dark showing through */
    for (auto &f : frames) {
        int n = 2 + rnd.nextInt(4);
        for (int i = 0; i < n; i++) {
            float t = rnd.nextFloat();
            Point<float> c;
            int side = rnd.nextInt(4);
            c = side == 0 ? Point<float>(f.getX() + t * f.getWidth(), f.getY())
              : side == 1 ? Point<float>(f.getX() + t * f.getWidth(), f.getBottom())
              : side == 2 ? Point<float>(f.getX(), f.getY() + t * f.getHeight())
                          : Point<float>(f.getRight(), f.getY() + t * f.getHeight());
            if (side < 2 && c.x < f.getX() + 140.0f && side == 0)
                continue;                                   /* (not through the title) */
            float w = 3.0f + rnd.nextFloat() * 9.0f;
            g.setColour(Colour(0xff2b2d31).withAlpha(0.9f));
            g.fillEllipse(Rectangle<float>(side < 2 ? w : 4.0f, side < 2 ? 4.0f : w).withCentre(c));
        }
    }
    /* polished by hands round the knobs played most: a faint sheen on the paint */
    for (auto &w : worn) {
        if (w.getWidth() < 70.0f)
            continue;                                       /* (knobs only: a fader's legend is narrow) */
        auto c = Point<float>(w.getCentreX(), w.getBottom() + 32.0f);
        ColourGradient cg(Colours::white.withAlpha(0.0f), c.x, c.y, Colours::white.withAlpha(0.0f), c.x + 54.0f, c.y, true);
        cg.addColour(0.6, Colours::white.withAlpha(0.07f));
        g.setGradientFill(cg);
        g.fillEllipse(Rectangle<float>(108.0f, 100.0f).withCentre(c));
    }
    /* the print rubbed where fingers went */
    for (auto &w : worn) {
        for (int i = 0; i < 2; i++) {
            auto c = Point<float>(w.getX() + rnd.nextFloat() * w.getWidth(), w.getY() + w.getHeight() * (0.3f + 0.5f * rnd.nextFloat()));
            float rad = 3.0f + rnd.nextFloat() * 7.0f;
            g.setGradientFill(ColourGradient(col::section.withAlpha(0.85f), c.x, c.y, col::section.withAlpha(0.0f), c.x + rad, c.y, true));
            g.fillEllipse(Rectangle<float>(rad * 2.0f, rad * 1.4f).withCentre(c));
        }
    }
}

void drawAgingOver(juce::Graphics &g, juce::Rectangle<float> r)
{
    using namespace juce;
    Random rnd(1983);
    /* a faint warmth over everything (the years' light), deeper toward the edges */
    g.setGradientFill(ColourGradient(Colour(0x00a08050), r.getCentreX(), r.getCentreY(), Colour(0x22704f24), r.getX(), r.getY(), true));
    g.fillRect(r);
    /* fine scratches in the finish: clusters, as from a belt buckle or a cable dragged over it, never everywhere */
    auto cluster = [&](Point<float> c, float w, float h, int n) {
        for (int i = 0; i < n; i++) {
            float x = c.x + (rnd.nextFloat() - 0.5f) * w, y = c.y + (rnd.nextFloat() - 0.5f) * h;
            float a = 1.2f + (rnd.nextFloat() - 0.5f) * 0.5f, len = 6.0f + rnd.nextFloat() * 40.0f;
            g.setColour(Colours::white.withAlpha(0.05f + 0.09f * rnd.nextFloat()));
            g.drawLine(x, y, x + std::sin(a) * len, y - std::cos(a) * len, 0.5f);
        }
    };
    cluster({r.getX() + r.getWidth() * 0.47f, r.getY() + r.getHeight() * 0.41f}, 220.0f, 90.0f, 45);
    cluster({r.getX() + r.getWidth() * 0.83f, r.getY() + r.getHeight() * 0.62f}, 160.0f, 70.0f, 30);
    cluster({r.getX() + r.getWidth() * 0.18f, r.getY() + r.getHeight() * 0.30f}, 120.0f, 60.0f, 20);
    /* a few honest dings: tiny chips with a bright edge */
    for (int i = 0; i < 10; i++) {
        auto c = Point<float>(r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight());
        float s = 1.5f + rnd.nextFloat() * 2.5f;
        g.setColour(Colour(0x90141416));
        g.fillEllipse(Rectangle<float>(s, s * 0.8f).withCentre(c));
        g.setColour(Colours::white.withAlpha(0.18f));
        g.drawEllipse(Rectangle<float>(s, s * 0.8f).withCentre(c.translated(0.4f, 0.4f)), 0.5f);
    }
    /* fine dust only where it would settle and stay: a light haze, no specks */
    g.setColour(Colour(0x0cb8ab8c));
    g.fillRect(r);
}
} // namespace glui
