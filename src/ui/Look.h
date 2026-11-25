/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (GALA SH-1T) */
/* The look of a GAIA SH-01, made new: a white body round a charcoal panel, the sections framed in their colours (the
 * LFO blue, OSC / FILTER / AMP amber, the TRANCE GATE cyan), black knobs with a white line, long faders with black
 * caps, light grey rubber buttons that light red, green, amber or cyan, small LEDs. The fonts are embedded (Fira
 * Sans, Anonymous Pro: SIL OFL, fonts/). */
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

namespace glui
{
namespace col
{
const juce::Colour body{0xffe2d9bd}, panel{0xff2a2c30}, panelDark{0xff1f2124}, section{0xff35383d}, legend{0xfff1f2f4},
    dim{0xff9aa0a8}, amber{0xfff2a31b}, blue{0xff3f95e6}, cyan{0xff22d3ee}, red{0xffff3b2f}, green{0xff6ee04a},
    orange{0xffff9a1f}, lcdBg{0xff0b1016}, lcd{0xff9fe7ff}, lcdDim{0xff3e6a7c};
}

enum class Face { label, title, logo, mono };

/* the fonts and the tag: they live while an editor is open (a SharedResourcePointer in each) */
struct UiResources
{
    UiResources() = default;
    ~UiResources();
};
juce::Font font(Face f, float height);

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font &) override;
    juce::Font getPopupMenuFont() override { return font(Face::label, 16.0f); }
    void drawPopupMenuBackground(juce::Graphics &, int w, int h) override;
};

void drawBody(juce::Graphics &, juce::Rectangle<float> r);          /* the white body and the charcoal panel in it */
void drawSection(juce::Graphics &, juce::Rectangle<float> r, const juce::String &title, juce::Colour accent);
void drawLegend(juce::Graphics &, const juce::String &, juce::Rectangle<float>, float size = 13.0f,
                juce::Colour c = col::legend, juce::Justification j = juce::Justification::centred);
/* a knob in r (square): pos 0..1; bipolar: a centre mark */
void drawKnob(juce::Graphics &, juce::Rectangle<float> r, float pos, bool bipolar);
/* a fader: its slot and scale in r, the cap at pos 0..1 */
void drawFader(juce::Graphics &, juce::Rectangle<float> r, float pos, bool bipolar);
/* a rubber button, lit in a colour (transparent: dark) */
void drawButton(juce::Graphics &, juce::Rectangle<float> r, juce::Colour lit, bool down, bool over);
void drawLed(juce::Graphics &, juce::Point<float> c, float size, juce::Colour colour, float level);
void drawTag(juce::Graphics &, juce::Rectangle<float> r);
/* a relic job, on the body and the panel (under the controls): checked lacquer, worn through at the edges, the paint
 * polished thin where hands rest, the frames' paint chipped, the print rubbed where fingers went (worn: those legends) */
void drawAgingUnder(juce::Graphics &, juce::Rectangle<float> r, const std::vector<juce::Rectangle<float>> &worn,
                    const std::vector<juce::Rectangle<float>> &frames);
/* ... and over everything (a warm tone, fine scratch clusters, a few dings): drawn once into an image */
void drawAgingOver(juce::Graphics &, juce::Rectangle<float> r);
void drawScrew(juce::Graphics &, juce::Point<float> c, float r, int seed);
} // namespace glui
