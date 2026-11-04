#include "Theme.h"
#include "BinaryData.h"

namespace cc::ui {

static Palette makeVelvet()
{
    Palette p;
    p.bg = juce::Colour (0xff111014);
    p.panel = juce::Colour (0xff1b191e);
    p.panelHi = juce::Colour (0xff221f25);
    p.border = juce::Colour (0xff332d33);
    p.tile = juce::Colour (0xff27232a);
    p.tileHover = juce::Colour (0xff332d36);
    p.tileOn = juce::Colour (0xffe2b866);
    p.tileOnText = juce::Colour (0xff1a1408);
    p.text = juce::Colour (0xffefe8dc);
    p.dim = juce::Colour (0xffa69d8f);
    p.faint = juce::Colour (0xff6d665e);
    p.gold = juce::Colour (0xffe2b866);
    p.goldDim = juce::Colour (0xff8f7037);
    p.velvet = juce::Colour (0xff6e1222);
    p.velvetHi = juce::Colour (0xff9b1c31);
    p.playing = juce::Colour (0xffe0344c);
    p.good = juce::Colour (0xff3ddc84);
    p.bad = juce::Colour (0xffb8323f);
    p.bass = juce::Colour (0xffd0762a);
    p.cls[0] = juce::Colour (0xffe2b866);
    p.cls[1] = juce::Colour (0xff6fb0e8);
    p.cls[2] = juce::Colour (0xff7fd46a);
    p.cls[3] = juce::Colour (0xffb79cff);
    p.cls[4] = juce::Colour (0xffff8a50);
    return p;
}

static Palette makeGraphite()
{
    Palette p = makeVelvet();
    p.bg = juce::Colour (0xff15171a);
    p.panel = juce::Colour (0xff1d2024);
    p.panelHi = juce::Colour (0xff24282d);
    p.border = juce::Colour (0xff343a41);
    p.tile = juce::Colour (0xff2a2f35);
    p.tileHover = juce::Colour (0xff353b43);
    p.tileOn = juce::Colour (0xff5ec8e6);
    p.tileOnText = juce::Colour (0xff071419);
    p.gold = juce::Colour (0xff5ec8e6);
    p.goldDim = juce::Colour (0xff2f6f80);
    p.velvet = juce::Colour (0xff1e3c48);
    p.velvetHi = juce::Colour (0xff2b5566);
    p.cls[0] = juce::Colour (0xff5ec8e6);
    return p;
}

static Palette makeDaylight()
{
    Palette p;
    p.bg = juce::Colour (0xffe9e4da);
    p.panel = juce::Colour (0xfff6f2ea);
    p.panelHi = juce::Colour (0xffffffff);
    p.border = juce::Colour (0xffcfc6b6);
    p.tile = juce::Colour (0xffe6dfd2);
    p.tileHover = juce::Colour (0xffdcd3c3);
    p.tileOn = juce::Colour (0xff8c1d2f);
    p.tileOnText = juce::Colour (0xfffdf6e8);
    p.text = juce::Colour (0xff211c17);
    p.dim = juce::Colour (0xff6b6156);
    p.faint = juce::Colour (0xffa1978a);
    p.gold = juce::Colour (0xff8c1d2f);
    p.goldDim = juce::Colour (0xffb88b8f);
    p.velvet = juce::Colour (0xff8c1d2f);
    p.velvetHi = juce::Colour (0xffb02a40);
    p.playing = juce::Colour (0xffd2283f);
    p.good = juce::Colour (0xff1e9d58);
    p.bad = juce::Colour (0xffa8202f);
    p.bass = juce::Colour (0xffc0661c);
    p.cls[0] = juce::Colour (0xff8c6a1d);
    p.cls[1] = juce::Colour (0xff2f78b8);
    p.cls[2] = juce::Colour (0xff3d9a2e);
    p.cls[3] = juce::Colour (0xff7a54d8);
    p.cls[4] = juce::Colour (0xffd8571c);
    return p;
}

static int g_theme = 0;
static Palette g_pal = makeVelvet();

const Palette& pal() { return g_pal; }
int theme() { return g_theme; }
juce::StringArray themeNames() { return { "Velvet", "Graphite", "Daylight" }; }
void setTheme (int i)
{
    g_theme = juce::jlimit (0, 2, i);
    g_pal = g_theme == 1 ? makeGraphite() : g_theme == 2 ? makeDaylight() : makeVelvet();
}

static juce::Typeface::Ptr face (int weight)
{
    static juce::Typeface::Ptr f[3];
    if (f[0] == nullptr)
    {
        f[0] = juce::Typeface::createSystemTypefaceFor (BinaryData::TitilliumWebRegular_ttf, BinaryData::TitilliumWebRegular_ttfSize);
        f[1] = juce::Typeface::createSystemTypefaceFor (BinaryData::TitilliumWebSemiBold_ttf, BinaryData::TitilliumWebSemiBold_ttfSize);
        f[2] = juce::Typeface::createSystemTypefaceFor (BinaryData::TitilliumWebBold_ttf, BinaryData::TitilliumWebBold_ttfSize);
    }
    return f[juce::jlimit (0, 2, weight)];
}

juce::Font font (float h, int weight)
{
    return juce::Font (juce::FontOptions (face (weight)).withHeight (h));
}

juce::Image logoImage() { return juce::ImageCache::getFromMemory (BinaryData::logo_big_png, BinaryData::logo_big_pngSize); }
juce::Image medallionImage() { return juce::ImageCache::getFromMemory (BinaryData::src_medallion_png, BinaryData::src_medallion_pngSize); }
juce::Image jfImage() { return juce::ImageCache::getFromMemory (BinaryData::jf_velvet_png, BinaryData::jf_velvet_pngSize); }

void fitText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float h, int weight, juce::Colour c,
              juce::Justification just, float minH, int lines)
{
    if (t.isEmpty() || r.isEmpty()) return;
    g.setColour (c);
    float size = h;
    if (lines <= 1)
    {
        while (size > minH)
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (font (size, weight), t, 0, 0);
            if (ga.getBoundingBox (0, -1, true).getWidth() <= r.getWidth())
                break;
            size -= 0.5f;
        }
        g.setFont (font (size, weight));
        g.drawText (t, r, just, true);
    }
    else
    {
        while (size > minH)
        {
            juce::GlyphArrangement ga;
            ga.addFittedText (font (size, weight), t, 0, 0, r.getWidth(), 10000, just, 1000, 1.0f);
            auto bb = ga.getBoundingBox (0, -1, true);
            if (bb.getHeight() <= r.getHeight() && ga.getNumGlyphs() > 0)
                break;
            size -= 0.5f;
        }
        g.setFont (font (size, weight));
        g.drawFittedText (t, r.toNearestInt(), just, lines, 1.0f);
    }
}

LookAndFeel::LookAndFeel() { refreshColours(); }

void LookAndFeel::refreshColours()
{
    auto& p = pal();
    setColour (juce::PopupMenu::backgroundColourId, p.panelHi);
    setColour (juce::PopupMenu::textColourId, p.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, p.gold);
    setColour (juce::PopupMenu::highlightedTextColourId, p.tileOnText);
    setColour (juce::PopupMenu::headerTextColourId, p.gold);
    setColour (juce::TextEditor::backgroundColourId, p.tile);
    setColour (juce::TextEditor::textColourId, p.text);
    setColour (juce::TextEditor::highlightColourId, p.gold.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, p.text);
    setColour (juce::TextEditor::outlineColourId, p.border);
    setColour (juce::TextEditor::focusedOutlineColourId, p.gold);
    setColour (juce::CaretComponent::caretColourId, p.gold);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, p.goldDim);
    setColour (juce::TooltipWindow::backgroundColourId, p.panelHi);
    setColour (juce::TooltipWindow::textColourId, p.text);
    setColour (juce::TooltipWindow::outlineColourId, p.gold);
    setColour (juce::ResizableWindow::backgroundColourId, p.bg);
}

juce::Typeface::Ptr LookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    return face (f.isBold() ? 2 : 1);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (pal().panelHi);
    g.setColour (pal().gold.withAlpha (0.6f));
    g.drawRect (0, 0, w, h, 1);
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    juce::AttributedString s;
    s.append (tipText, font (15.0f, 0), pal().text);
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, 380.0f);
    const int w = (int) tl.getWidth() + 22, h = (int) tl.getHeight() + 14;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (pal().panelHi);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (pal().gold.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    juce::AttributedString s;
    s.append (text, font (15.0f, 0), pal().text);
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, 380.0f);
    tl.draw (g, r.reduced (11.0f, 7.0f));
}

void LookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                 int thumbStart, int thumbSize, bool over, bool down)
{
    juce::Rectangle<float> t = vertical ? juce::Rectangle<float> ((float) x + 2, (float) thumbStart, (float) w - 4, (float) thumbSize)
                                        : juce::Rectangle<float> ((float) thumbStart, (float) y + 2, (float) thumbSize, (float) h - 4);
    juce::ignoreUnused (x, y);
    g.setColour (pal().goldDim.withAlpha (down ? 1.0f : over ? 0.9f : 0.6f));
    g.fillRoundedRectangle (t, 3.0f);
}

void LookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (pal().tile);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 5.0f);
}

void LookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& te)
{
    g.setColour (te.hasKeyboardFocus (true) ? pal().gold : pal().border);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 5.0f, 1.0f);
}

void LookAndFeel::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    g.setColour (pal().panelHi);
    g.fillPath (path);
    g.setColour (pal().gold.withAlpha (0.7f));
    g.strokePath (path, juce::PathStrokeType (1.2f));
}

} // namespace cc::ui
